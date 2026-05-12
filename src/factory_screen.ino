#include <Arduino.h>
#include <WiFi.h>
#include <FastLED.h>
#include "config.h"
#include "ble_manager.h"
#include "display_manager.h"
#include "usb_manager.h"

CRGB leds[1];

void setup() {
    Serial.begin(115200);
    
    // Status LED
    FastLED.addLeds<APA102, LED_DI_PIN, LED_CI_PIN, BGR>(leds, 1);
    FastLED.setBrightness(100);
    leds[0] = CHSV(160, 255, 255); 
    FastLED.show();

    WiFi.mode(WIFI_OFF);

    initDisplay();
    initUSB();     
    initBLE();
}

void loop() {
    static uint32_t last_ui_update = 0;
    uint32_t now = millis();

    // USB Befehle vom M5Tab verarbeiten
    processUSBRx(); // Liest Kommandos ein
    processUSBTx(); // <-- NEU: Feuert die gepufferten JSONs an das M5Tab ab

    // UI aktualisieren (1x pro Sekunde)
    if (now - last_ui_update > 1000) {
        last_ui_update = now;

        std::vector<BleDevice> currentDevices = getDiscoveredDevices();
        bool isConn = isBleConnected();
        String status = getBleStatusMsg();

        // --> HINWEIS: Alle ungeschützten Serial.prints wurden hier entfernt, 
        // um den JSON-Stream für das M5Tab nicht zu korrumpieren!

        // Display aktualisieren
        updateDisplayUi(currentDevices, status, isConn);

        // LED Indikator
        uint8_t ledHue = isConn ? 96 : 160;
        leds[0] = CHSV(ledHue, 255, 255); 
        FastLED.show();
        delay(20); 
        leds[0] = CHSV(ledHue, 255, 100); 
        FastLED.show();
    }

    keepDisplayAlive();
    delay(5);
}