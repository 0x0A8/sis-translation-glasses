// main.cpp — SPH0645 → ESP32-S3 → MP3 → HTTP download
// Libraries needed (install via Arduino Library Manager):
//   - ESP8266Audio  (by earlephilhower)
//   - LittleFS (built into ESP32 Arduino core)

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"

static i2s_chan_handle_t rx_chan = NULL;
static TaskHandle_t recordTask = NULL;
volatile bool recording = false;
volatile bool recording_finished = false;
volatile uint32_t record_start_ms = 0;

// ── WiFi credentials ──────────────────────────────────────────
const char* SSID     = "xfy";
const char* PASSWORD = "notxfinity84";

// ── I2S pin config (matches wiring above) ────────────────────
#define I2S_PORT        I2S_NUM_0
#define I2S_BCLK_PIN    14   // BCLK
#define I2S_WS_PIN      15   // LRCL / word select
#define I2S_DATA_PIN    16   // DOUT

// ── Recording settings ────────────────────────────────────────
#define SAMPLE_RATE     16000
#define RECORD_SECONDS  10
#define OUTPUT_FILE     "/recording.wav"   // WAV is simpler; see note below
#define AUDIO_GAIN      3.0f          // digital amplification factor, adjust as needed

WebServer server(80);

// ── I2S initialisation ────────────────────────────────────────
void i2s_init() {
  // 1. Create the RX channel
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

  // 2. Configure standard I2S mode for the SPH0645
  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_BCLK_PIN,
      .ws   = (gpio_num_t)I2S_WS_PIN,
      .dout = I2S_GPIO_UNUSED,
      .din  = (gpio_num_t)I2S_DATA_PIN,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv   = false,
      },
    },
  };

  // SPH0645 outputs data on the falling edge — set WS to left (0)
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
}

// ── WAV header writer ─────────────────────────────────────────
void write_wav_header(File &f, uint32_t data_len) {
  uint32_t sr     = SAMPLE_RATE;
  uint16_t bps    = 16;
  uint16_t ch     = 1;
  uint32_t byte_r = sr * ch * (bps / 8);
  uint16_t blk_a  = ch * (bps / 8);
  uint32_t chunk2 = data_len;
  uint32_t chunk0 = 36 + chunk2;

  f.write((uint8_t*)"RIFF", 4);  f.write((uint8_t*)&chunk0, 4);
  f.write((uint8_t*)"WAVE", 4);
  f.write((uint8_t*)"fmt ", 4);
  uint32_t sc = 16; f.write((uint8_t*)&sc, 4);
  uint16_t af = 1;  f.write((uint8_t*)&af, 2);
                    f.write((uint8_t*)&ch, 2);
                    f.write((uint8_t*)&sr, 4);
                    f.write((uint8_t*)&byte_r, 4);
                    f.write((uint8_t*)&blk_a, 2);
                    f.write((uint8_t*)&bps, 2);
  f.write((uint8_t*)"data", 4);  f.write((uint8_t*)&chunk2, 4);
}

// ── Record audio to WAV ───────────────────────────────────────
void record_audio() {
  Serial.println("Recording…");
  recording = true;

  File f = LittleFS.open(OUTPUT_FILE, FILE_WRITE);
  if (!f) { Serial.println("File open failed"); return; }

  // Reserve space for header; fill it in after we know data size
  uint8_t hdr[44] = {};
  f.write(hdr, 44);

  const int buf_samples = 256;
  int32_t  raw[buf_samples];
  int16_t  pcm[buf_samples];
  size_t   bytes_read;
  uint32_t total_samples = SAMPLE_RATE * RECORD_SECONDS;
  uint32_t written = 0;

  while (written < total_samples) {
    i2s_channel_read(rx_chan, raw, buf_samples * sizeof(int32_t), &bytes_read, portMAX_DELAY);
    int got = bytes_read / sizeof(int32_t);
    for (int i = 0; i < got; i++) {
      // SPH0645: data is left-justified in the 32-bit word — shift down by 11
      int32_t sample = raw[i] >> 14;
      int32_t amplified = (int32_t)(sample * AUDIO_GAIN);
      if (amplified > INT16_MAX) amplified = INT16_MAX;
      if (amplified < INT16_MIN) amplified = INT16_MIN;
      pcm[i] = (int16_t)amplified;
    }
    f.write((uint8_t*)pcm, got * sizeof(int16_t));
    written += got;
  }

  // Patch the WAV header now we know the data size
  uint32_t data_bytes = written * sizeof(int16_t);
  f.seek(0);
  write_wav_header(f, data_bytes);
  f.close();

  Serial.printf("Saved %u bytes to %s\n", data_bytes + 44, OUTPUT_FILE);
  recording = false;
  recording_finished = true;
}

// ── HTTP handlers ─────────────────────────────────────────────
void record_audio_task(void* param) {
  record_audio();
  recordTask = NULL;
  vTaskDelete(NULL);
}

void handle_root() {
  String html =
    "<html><body style='font-family:sans-serif;padding:2rem'>"
    "<h2>ESP32-S3 Audio Recorder</h2>"
    "<p><a href='/record'><button>Record 10 seconds</button></a></p>"
    "<p><a href='/download'><button>Download recording.wav</button></a></p>"
    "</body></html>";
  server.send(200, "text/html", html);
}

void handle_record_page() {
  String html =
    "<html><body style='font-family:sans-serif;padding:2rem'>"
    "<h2>Recording Status</h2>"
    "<p id='status'>Preparing…</p>"
    "<p>Time left: <span id='timer'>" + String(RECORD_SECONDS) + "</span>s</p>"
    "<p><button id='startBtn' onclick='startRecording()'>Start recording</button></p>"
    "<p><a id='downloadLink' href='/download' style='display:none'><button>Download recording.wav</button></a></p>"
    "<script>"
    "const duration = " + String(RECORD_SECONDS) + ";"
    "const statusEl = document.getElementById('status');"
    "const timerEl = document.getElementById('timer');"
    "const downloadEl = document.getElementById('downloadLink');"
    "const startBtn = document.getElementById('startBtn');"
    "function updateStatus(data) {"
      "if (data.recording) {"
        "statusEl.textContent = 'Recording...';"
        "timerEl.textContent = data.remaining;"
        "downloadEl.style.display = 'none';"
        "startBtn.disabled = true;"
      "} else if (data.finished) {"
        "statusEl.textContent = 'Finished! Download ready.';"
        "timerEl.textContent = '0';"
        "downloadEl.style.display = 'inline-block';"
        "startBtn.disabled = false;"
      "} else if (data.ready) {"
        "statusEl.textContent = 'Ready to record.';"
        "timerEl.textContent = duration;"
        "downloadEl.style.display = data.hasFile ? 'inline-block' : 'none';"
        "startBtn.disabled = false;"
      "} else {"
        "statusEl.textContent = 'Idle.';"
        "timerEl.textContent = duration;"
        "downloadEl.style.display = data.hasFile ? 'inline-block' : 'none';"
        "startBtn.disabled = false;"
      "}"
    "}"
    "function pollStatus() {"
      "fetch('/record/status').then(r => r.json()).then(data => {"
        "updateStatus(data);"
        "if (data.recording) setTimeout(pollStatus, 250);"
      "});"
    "}"
    "function startRecording() {"
      "fetch('/record/start').then(r => r.json()).then(data => {"
        "if (data.error) { statusEl.textContent = data.error; return; }"
        "updateStatus(data);"
        "if (data.recording) pollStatus();"
      "});"
    "}"
    "window.addEventListener('load', () => {"
      "pollStatus();"
      "if (!navigator.onLine || !startBtn.disabled) {"
        "startRecording();"
      "}"
    "});"
    "</script>"
    "</body></html>";
  server.send(200, "text/html", html);
}

void handle_record_start() {
  if (recording) {
    server.send(200, "application/json", "{\"recording\":true,\"remaining\":\"" + String(RECORD_SECONDS - (millis() - record_start_ms) / 1000) + "\"}");
    return;
  }

  recording_finished = false;
  record_start_ms = millis();
  BaseType_t created = xTaskCreatePinnedToCore(record_audio_task, "record", 8192, NULL, 1, &recordTask, 1);
  if (created != pdPASS) {
    server.send(500, "application/json", "{\"error\":\"Record task failed\"}");
    return;
  }

  server.send(200, "application/json", "{\"started\":true,\"recording\":true,\"remaining\":" + String(RECORD_SECONDS) + "}");
}

void handle_record_status() {
  bool hasFile = LittleFS.exists(OUTPUT_FILE);
  String body;
  if (recording) {
    uint32_t elapsed = (millis() - record_start_ms) / 1000;
    if (elapsed > RECORD_SECONDS) elapsed = RECORD_SECONDS;
    body = "{\"recording\":true,\"remaining\":" + String(RECORD_SECONDS - elapsed) + ",\"finished\":false,\"ready\":false,\"hasFile\":" + String(hasFile ? "true" : "false") + "}";
  } else if (recording_finished || hasFile) {
    body = "{\"recording\":false,\"remaining\":0,\"finished\":true,\"ready\":true,\"hasFile\":true}";
  } else {
    body = "{\"recording\":false,\"remaining\":0,\"finished\":false,\"ready\":true,\"hasFile\":" + String(hasFile ? "true" : "false") + "}";
  }
  server.send(200, "application/json", body);
}

void handle_download() {
  File f = LittleFS.open(OUTPUT_FILE, FILE_READ);
  if (!f) { server.send(404, "text/plain", "No recording found"); return; }
  server.sendHeader("Content-Disposition", "attachment; filename=recording.wav");
  server.streamFile(f, "audio/wav");
  f.close();
}

// ── Setup & loop ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed"); return;
  }

  i2s_init();

  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nReady at http://%s\n", WiFi.localIP().toString().c_str());

  server.on("/",             handle_root);
  server.on("/record",       handle_record_page);
  server.on("/record/start", handle_record_start);
  server.on("/record/status",handle_record_status);
  server.on("/download",     handle_download);
  server.begin();
}

void loop() {
  server.handleClient();
}