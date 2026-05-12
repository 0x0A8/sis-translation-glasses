#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Audio.h>
#include <SPIFFS.h>

#define I2S_BCLK  16
#define I2S_LRC   15
#define I2S_DOUT  17

const char* SSID       = "xfy";
const char* PASSWORD   = "notxfinity84";
const char* XI_API_KEY = "sk_3b9b7a3d082adb296fe8e35c630113e7712cfdd5207bfdd4";
const char* VOICE_ID   = "hA4zGnmTwX2NQiTRMt7o"; // ElevenLabs "George" - change as needed

const char* TEXT = "White Monster is soooo good";

Audio audio;

void setup() {
    Serial.begin(115200);

    WiFi.begin(SSID, PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected!");

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(14);

    // Build the ElevenLabs TTS endpoint URL
    // output_format=mp3_22050_32 keeps it small and ESP32-friendly
    String url = "https://api.elevenlabs.io/v1/text-to-speech/";
    url += VOICE_ID;
    url += "?output_format=mp3_22050_32";

    // Build JSON body
    String body = "{\"text\":\"";
    body += TEXT;
    body += "\",\"model_id\":\"eleven_turbo_v2\"}";

    // Make HTTPS POST request manually, stream response into audio
    WiFiClientSecure client;
    client.setInsecure(); // skip cert validation - fine for hobby use

    Serial.println("Connecting to ElevenLabs...");
    if (!client.connect("api.elevenlabs.io", 443)) {
        Serial.println("Connection failed!");
        return;
    }
    uint8_t begin_ms = millis();
    // Send HTTP request
    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: api.elevenlabs.io");
    client.println("xi-api-key: " + String(XI_API_KEY));
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(body.length()));
    client.println("Connection: close");
    client.println();
    client.print(body);

    Serial.println("Waiting for response...");

    // Skip HTTP headers - read until blank line
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break; // end of headers
    }

    // Save MP3 stream to SPIFFS then play
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed!");
        return;
    }

    File f = SPIFFS.open("/tts.mp3", FILE_WRITE);
    if (!f) {
        Serial.println("Failed to open file for writing!");
        return;
    }

    Serial.println("Downloading TTS audio...");
    uint8_t buf[512];
    int totalBytes = 0;
    while (client.connected() || client.available()) {
        int len = client.available();
        if (len > 0) {
            int toRead = min(len, (int)sizeof(buf));
            client.readBytes(buf, toRead);
            f.write(buf, toRead);
            totalBytes += toRead;
        }
    }
    f.close();
    client.stop();

    Serial.printf("Downloaded %d bytes. Playing...\n", totalBytes);
    Serial.print("Time to play: ");
    Serial.println(millis() - begin_ms);
    audio.connecttoFS(SPIFFS, "/tts.mp3");
}

void loop() {
    audio.loop();
}

void audio_info(const char *info) {
    Serial.println(info);
}
void audio_eof_mp3(const char *info) {
    Serial.print("End of file: ");
    Serial.println(info);
}