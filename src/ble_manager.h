#pragma once
#include <Arduino.h>
#include <vector>
#include <map>

// --- Datenstrukturen ---
struct BleDevice {
    String mac;
    String name;
    int rssi;
    uint32_t last_seen;
};

// Konfiguration für das dynamische Auslesen eines Sensors
struct SensorConfig {
    String serviceUUID;
    String charUUID;
    uint8_t byteOffset;
    uint8_t byteLength;
    bool isBigEndian;
    bool isSigned;
    float multiplier;
    float addOffset;
};

// --- Öffentliche Funktionen ---
void initBLE();

// Scanner-Steuerung
void startScan(std::vector<String> macFilters);
void stopScan();

// Verbindungs-Steuerung
void connectToBleServer(String macAddress, SensorConfig config);
void disconnectFromBleServer(String macAddress);

// Getter für die UI
std::vector<BleDevice> getDiscoveredDevices();
String getBleStatusMsg();
bool isBleConnected();