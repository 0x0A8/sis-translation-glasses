/**
 * i2s_record_playback.ino
 * ESP32-S3 — dual SPH0645 mic record + MAX98357A playback
 *
 * Microphones (I2S0 — input):
 *   LRCLK  → GPIO 12
 *   DOUT   → GPIO 13
 *   BCLK   → GPIO 14
 *
 * Speaker / MAX98357A (I2S1 — output):
 *   LRC    → GPIO 4
 *   BCLK   → GPIO 5
 *   DIN    → GPIO 6
 *
 * Usage (Serial Monitor @ 115200):
 *   'L' → select left  mic (SEL tied to GND)
 *   'R' → select right mic (SEL tied to 3.3V)
 *   'r' → record  RECORD_SECONDS of audio
 *   'p' → play back the last recording
 */

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <string.h>

// ──────────────────────────────────────────────
//  Config
// ──────────────────────────────────────────────
#define SAMPLE_RATE       16000          // Hz
#define SAMPLE_BITS       32             // SPH0645 outputs 24-bit in a 32-bit frame
#define RECORD_SECONDS    5
#define BUFFER_SAMPLES    (SAMPLE_RATE * RECORD_SECONDS)  // mono samples
#define BYTES_PER_SAMPLE  (SAMPLE_BITS / 8)

// Mic pins
#define MIC_BCLK_PIN      14
#define MIC_LRCLK_PIN     12
#define MIC_DATA_PIN      13

// Amp pins
#define AMP_BCLK_PIN      5
#define AMP_LRCLK_PIN     4
#define AMP_DATA_PIN      6

// ──────────────────────────────────────────────
//  Globals
// ──────────────────────────────────────────────
static i2s_chan_handle_t rx_handle = NULL;
static i2s_chan_handle_t tx_handle = NULL;

// Audio buffer — stores 32-bit raw samples, then we convert to 16-bit for playback
static int16_t *play_buf_16 = nullptr;
static size_t   recorded_samples = 0;

typedef enum { MIC_LEFT, MIC_RIGHT } mic_channel_t;
static mic_channel_t active_mic = MIC_LEFT;

// ──────────────────────────────────────────────
//  I2S initialisation helpers
// ──────────────────────────────────────────────

static esp_err_t init_mic_i2s(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_0,          // controller 0 → RX
        I2S_ROLE_MASTER
    );
    chan_cfg.dma_desc_num  = 8;
    chan_cfg.dma_frame_num = 256;

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg;
    std_cfg.clk_cfg.sample_rate_hz = SAMPLE_RATE;
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
#if SOC_I2S_HW_VERSION_2
    std_cfg.clk_cfg.ext_clk_freq_hz = 0;
#endif
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        (i2s_data_bit_width_t)SAMPLE_BITS,
                        I2S_SLOT_MODE_STEREO   // L+R so we can pick channel in SW
                    );
    std_cfg.gpio_cfg.bclk = (gpio_num_t)MIC_BCLK_PIN;
    std_cfg.gpio_cfg.ws = (gpio_num_t)MIC_LRCLK_PIN;
    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din = (gpio_num_t)MIC_DATA_PIN;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv = false;

    return i2s_channel_init_std_mode(rx_handle, &std_cfg);
}

static esp_err_t init_amp_i2s(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_1,          // controller 1 → TX
        I2S_ROLE_MASTER
    );
    chan_cfg.dma_desc_num  = 8;
    chan_cfg.dma_frame_num = 256;

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_std_config_t std_cfg;
    std_cfg.clk_cfg.sample_rate_hz = SAMPLE_RATE;
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
#if SOC_I2S_HW_VERSION_2
    std_cfg.clk_cfg.ext_clk_freq_hz = 0;
#endif
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT,
                        I2S_SLOT_MODE_STEREO   // amp averages L+R internally
                    );
    std_cfg.gpio_cfg.bclk = (gpio_num_t)AMP_BCLK_PIN;
    std_cfg.gpio_cfg.ws = (gpio_num_t)AMP_LRCLK_PIN;
    std_cfg.gpio_cfg.dout = (gpio_num_t)AMP_DATA_PIN;
    std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv = false;

    return i2s_channel_init_std_mode(tx_handle, &std_cfg);
}

// ──────────────────────────────────────────────
//  Record
// ──────────────────────────────────────────────

static void do_record(void)
{
    const size_t stereo_bytes = BUFFER_SAMPLES * 2 * BYTES_PER_SAMPLE;
    int32_t *stereo_buf = (int32_t *)ps_malloc(BUFFER_SAMPLES * 2 * BYTES_PER_SAMPLE);
    if (!stereo_buf) {
        Serial.println("[ERROR] Not enough heap for record buffer");
        return;
    }

    Serial.printf("[REC] Recording %d s from %s mic...\n",
                  RECORD_SECONDS,
                  active_mic == MIC_LEFT ? "LEFT" : "RIGHT");

    size_t bytes_read = 0;
    esp_err_t err = i2s_channel_read(rx_handle, stereo_buf, stereo_bytes,
                                      &bytes_read, portMAX_DELAY);
    if (err != ESP_OK) {
        Serial.printf("[ERROR] i2s_channel_read failed: %d\n", err);
        free(stereo_buf);
        return;
    }

    size_t total_stereo_samples = bytes_read / BYTES_PER_SAMPLE;  // L+R samples
    recorded_samples = 0;

    int ch_offset = (active_mic == MIC_LEFT) ? 0 : 1;

    for (size_t i = ch_offset; i < total_stereo_samples; i += 2) {
        int32_t s32 = stereo_buf[i] >> 8;
        play_buf_16[recorded_samples++] = (int16_t)(s32 >> 8);
        if (recorded_samples >= BUFFER_SAMPLES) break;
    }

    free(stereo_buf);
    Serial.printf("[REC] Done — captured %zu samples (%.2f s)\n",
                  recorded_samples, (float)recorded_samples / SAMPLE_RATE);
}

// ──────────────────────────────────────────────
//  Playback
// ──────────────────────────────────────────────

static void do_playback(void)
{
    i2s_channel_enable(tx_handle);
    if (recorded_samples == 0) {
        Serial.println("[PLAY] Nothing recorded yet — press 'r' first");
        return;
    }

    Serial.printf("[PLAY] Playing %zu samples...\n", recorded_samples);

    const size_t CHUNK = 512;  // samples per write
    int16_t chunk_buf[CHUNK * 2];  // *2 for stereo

    size_t offset = 0;
    while (offset < recorded_samples) {
        size_t n = min(CHUNK, recorded_samples - offset);
        for (size_t i = 0; i < n; i++) {
            chunk_buf[i * 2]     = play_buf_16[offset + i];  // L
            chunk_buf[i * 2 + 1] = play_buf_16[offset + i];  // R (mono dup)
        }

        size_t bytes_written = 0;
        i2s_channel_write(tx_handle, chunk_buf, n * 2 * sizeof(int16_t),
                          &bytes_written, portMAX_DELAY);
        offset += n;
    }

    memset(chunk_buf, 0, sizeof(chunk_buf));
    size_t bw = 0;
    i2s_channel_write(tx_handle, chunk_buf, sizeof(chunk_buf), &bw, portMAX_DELAY);
    i2s_channel_disable(tx_handle);
    Serial.println("[PLAY] Done");
}

// ──────────────────────────────────────────────
//  Arduino entry points
// ──────────────────────────────────────────────

void setup(void)
{
    // 1. Establish Serial communication first
    Serial.begin(115200);
    
    // Wait up to 5 seconds for the Serial Monitor window to actually open
    while (!Serial && millis() < 5000) {
        delay(10);
    }

    Serial.println("\n=========================================");
    Serial.println("!!! SERIAL IS ONLINE !!!");
    Serial.println("=========================================");

    Serial.println("\n=== ESP32-S3 I2S Mic Record/Playback ===");
    Serial.println("Commands:");
    Serial.println("  L  — use LEFT  mic (SEL → GND)");
    Serial.println("  R  — use RIGHT mic (SEL → 3.3V)");
    Serial.println("  r  — record");
    Serial.println("  p  — playback");
    Serial.println("=========================================\n");

    // 2. Initialize Hardware Channels FIRST while internal RAM is empty.
    // This stops the I2S driver handles from being forced into external PSRAM.
    Serial.println("[INIT] Starting mic I2S (I2S0)...");
    ESP_ERROR_CHECK(init_mic_i2s());
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    Serial.println("[INIT] Mic I2S OK");

    Serial.println("[INIT] Starting amp I2S (I2S1)...");
    ESP_ERROR_CHECK(init_amp_i2s());
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    Serial.println("[INIT] Amp I2S OK");

    // 3. NOW allocate the large audio playback buffer into the PSRAM safely
    Serial.println("[INIT] Allocating main audio buffer in PSRAM...");
    play_buf_16 = (int16_t *)ps_malloc(BUFFER_SAMPLES * sizeof(int16_t));
    if (!play_buf_16) {
        Serial.println("[ERROR] PSRAM allocation failed! Ensure PSRAM is enabled in Tools menu.");
        while (true) delay(1000);
    }

    Serial.printf("[INIT] Active mic: %s\n", active_mic == MIC_LEFT ? "LEFT" : "RIGHT");
    Serial.printf("[INIT] Record buffer: %d samples @ %d Hz = %d s, %.1f KB\n",
                  BUFFER_SAMPLES, SAMPLE_RATE, RECORD_SECONDS,
                  (float)(BUFFER_SAMPLES * sizeof(int16_t)) / 1024.0f);
}

void loop(void)
{
    if (!Serial.available()) return;

    char cmd = Serial.read();
    while (Serial.available()) Serial.read(); // Flush trailing characters

    switch (cmd) {
        case 'L':
        case 'l':
            active_mic = MIC_LEFT;
            Serial.println("[CFG] Switched to LEFT mic");
            break;

        case 'R':
        case 'r':
            if (cmd == 'R') {
                active_mic = MIC_RIGHT;
                Serial.println("[CFG] Switched to RIGHT mic");
            } else {
                do_record();
            }
            break;

        case 'p':
        case 'P':
            do_playback();
            break;

        default:
            Serial.printf("[?] Unknown command '%c'\n", cmd);
            break;
    }
}