#pragma once
#include <Arduino.h>

void initUSB();
void processUSBRx();
void processUSBTx();

void sendSensorData(String mac, float value);
void sendBeaconData(String mac, String name, int rssi);
void sendStatusMessage(String msg);

// NEU: Error Events
void sendErrorEvent(String type, String msg, String mac = "");