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
    pinMode(BOOT_PIN, INPUT_PULLUP);

    FastLED.addLeds<APA102, LED_DI_PIN, LED_CI_PIN, BGR>(leds, 1);
    
    // Wir starten im MODE_GRAPHIC_LED_OFF -> Die LED muss initial aus sein!
    FastLED.setBrightness(0);
    leds[0] = CRGB::Black; 
    FastLED.show();

    WiFi.mode(WIFI_OFF);

    initDisplay();
    initUSB();     
    initBLE();
}

void loop() {
    static uint32_t last_ui_update = 0;
    static uint32_t last_btn_press = 0;
    uint32_t now = millis();

    // --- Flanken-gesteuertes Button Handling ---
    bool current_btn_state = (digitalRead(BOOT_PIN) == LOW);
    static bool last_btn_state = false;

    if (current_btn_state && !last_btn_state) { 
        if (now - last_btn_press > 50) { 
            DisplayMode cur = getDisplayMode();
            cur = (DisplayMode)((cur + 1) % 6); // Rotiert durch alle 6 Modi
            setDisplayMode(cur);
            
            // LED Logik auswerten
            bool led_is_on_mode = (cur == MODE_GRAPHIC_LED_ON || cur == MODE_TEXT_LED_ON || cur == MODE_OFF_LED_ON);
            
            if (!led_is_on_mode) {
                FastLED.setBrightness(0); 
                leds[0] = CRGB::Black;
                FastLED.show();
            } else {
                FastLED.setBrightness(100); 
            }

            // Zeige Overlay (außer der Bildschirm ist aus)
            if (cur != MODE_OFF_LED_OFF && cur != MODE_OFF_LED_ON) {
                String modeName = "";
                if (cur == MODE_GRAPHIC_LED_OFF) modeName = "Grafik (LED Aus)";
                else if (cur == MODE_GRAPHIC_LED_ON) modeName = "Grafik (LED An)";
                else if (cur == MODE_TEXT_LED_OFF) modeName = "Text (LED Aus)";
                else if (cur == MODE_TEXT_LED_ON) modeName = "Text (LED An)";
                showModeOverlay(modeName);
            }
            last_btn_press = now;
        }
    }
    last_btn_state = current_btn_state; 

    processUSBRx(); 
    processUSBTx();

    // UI & LED Update (1x pro Sekunde)
    if (now - last_ui_update > 1000) {
        last_ui_update = now;
        DisplayMode mode = getDisplayMode();

        bool isConn = isBleConnected();
        bool isScan = isBleScanning();
        String statusMsg = getBleStatusMsg();

        // UI Hintergrundberechnung
        updateDisplayUi(getDiscoveredDevices(), statusMsg, isConn, isScan);

        // Darf die LED in diesem Modus überhaupt leuchten?
        bool led_is_on_mode = (mode == MODE_GRAPHIC_LED_ON || mode == MODE_TEXT_LED_ON || mode == MODE_OFF_LED_ON);

        if (led_is_on_mode) {
            uint8_t ledHue = isConn ? 96 : (isScan ? 160 : 32); 
            FastLED.setBrightness(100);
            leds[0] = CHSV(ledHue, 255, 255); 
            FastLED.show();
            delay(15); 
            leds[0] = CHSV(ledHue, 255, 100); 
            FastLED.show();
        } else {
            FastLED.setBrightness(0);
            leds[0] = CRGB::Black;
            FastLED.show();
        }
    }

    keepDisplayAlive();
    delay(2);
}