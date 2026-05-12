#pragma once
#include <Arduino.h>
#include <vector>
#include "ble_manager.h" 

void initDisplay();
void updateDisplayUi(const std::vector<BleDevice>& devices, String statusMsg, bool isConnected);
void keepDisplayAlive();