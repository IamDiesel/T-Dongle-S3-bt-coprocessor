#pragma once
#include <Arduino.h>
#include <vector>
#include "ble_manager.h" 

enum DisplayMode {
    MODE_GRAPHIC_LED_OFF = 0,
    MODE_GRAPHIC_LED_ON = 1,
    MODE_TEXT_LED_OFF = 2,
    MODE_TEXT_LED_ON = 3,
    MODE_OFF_LED_OFF = 4,
    MODE_OFF_LED_ON = 5
};

void initDisplay();
void updateDisplayUi(const std::vector<BleDevice>& devices, String statusMsg, bool isConnected, bool isScanning);
void keepDisplayAlive();

void setDisplayMode(DisplayMode mode);
DisplayMode getDisplayMode();
void showModeOverlay(String modeName);

void setAlarmActive(bool active);
bool getAlarmActive();

void setDisplayBrightness(uint8_t percent);
uint8_t getDisplayBrightness();

// Error Handling
void showErrorDisplay(String errMsg);
bool getErrorActive();