#pragma once
#include <Arduino.h>

void initUSB();
void processUSBRx(); // Befehle vom M5Tab lesen
void processUSBTx(); // JSON Puffer sicher an das M5Tab senden

void sendSensorData(String mac, float value);
void sendBeaconData(String mac, String name, int rssi);
void sendStatusMessage(String msg);