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
    uint32_t last_sent_usb; // Zeitstempel für die USB-Drosselung
};

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

void startScan(std::vector<String> macFilters);
void stopScan();

void connectToBleServer(String macAddress, SensorConfig config);
void disconnectFromBleServer(String macAddress);

std::vector<BleDevice> getDiscoveredDevices();
String getBleStatusMsg();
bool isBleConnected();
bool isBleScanning(); // <-- NEU: Für die Display-Logik