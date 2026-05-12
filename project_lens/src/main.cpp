#include <Arduino.h>
#include <Audio.h>
#include <LittleFS.h>

// I2S pins
#define I2S_BCLK 16
#define I2S_LRC 15
#define I2S_DIN 17

Audio audio;

void setup() {
    Serial.begin(115200);
    
    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return;
    }
    
    // Initialize I2S
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
    audio.setVolume(21); // 0...21
    
    // Play the MP3 file
    audio.connecttoFS(LittleFS, "/test.mp3");
    
    Serial.println("Playing MP3...");
}

void loop() {
    audio.loop();
}