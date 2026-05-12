#include "ble_manager.h"
#include "usb_manager.h"
#include <NimBLEDevice.h>
#include <algorithm>

static std::vector<BleDevice> discoveredDevices;
static std::vector<String> currentScanFilters;
static bool isScanRequested = false;

struct ConnectionData {
    NimBLEClient* pClient;
    SensorConfig config;
    String lastValue;
    uint32_t last_sent_usb;
};

static std::map<String, ConnectionData> activeConnections;
static SemaphoreHandle_t bleMutex; 
static NimBLEScan* pBLEScan;

static void adjustScanTiming() {
    bool hasConnections = false;
    if (xSemaphoreTake(bleMutex, portMAX_DELAY)) {
        hasConnections = !activeConnections.empty();
        xSemaphoreGive(bleMutex);
    }
    if (hasConnections) {
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(30); 
    } else {
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99); 
    }
}

static void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    String mac = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str();
    mac.toUpperCase();

    if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(10))) {
        if (activeConnections.find(mac) != activeConnections.end()) {
            SensorConfig cfg = activeConnections[mac].config;

            if (length >= cfg.byteOffset + cfg.byteLength) {
                uint32_t rawValue = 0;
                if (cfg.byteLength == 1) {
                    rawValue = pData[cfg.byteOffset];
                } else if (cfg.byteLength == 2) {
                    if (cfg.isBigEndian) rawValue = (pData[cfg.byteOffset] << 8) | pData[cfg.byteOffset + 1];
                    else rawValue = (pData[cfg.byteOffset + 1] << 8) | pData[cfg.byteOffset];
                } else if (cfg.byteLength == 4) {
                    if (cfg.isBigEndian) rawValue = (pData[cfg.byteOffset] << 24) | (pData[cfg.byteOffset + 1] << 16) | (pData[cfg.byteOffset + 2] << 8) | pData[cfg.byteOffset + 3];
                    else rawValue = (pData[cfg.byteOffset + 3] << 24) | (pData[cfg.byteOffset + 2] << 16) | (pData[cfg.byteOffset + 1] << 8) | pData[cfg.byteOffset];
                }

                float finalValue = 0;
                if (cfg.isSigned) {
                    if (cfg.byteLength == 1) finalValue = (int8_t)rawValue;
                    else if (cfg.byteLength == 2) finalValue = (int16_t)rawValue;
                    else if (cfg.byteLength == 4) finalValue = (int32_t)rawValue;
                } else {
                    finalValue = rawValue;
                }

                finalValue = (finalValue * cfg.multiplier) + cfg.addOffset;
                activeConnections[mac].lastValue = String(finalValue, 2); 
                
                uint32_t now = millis();
                if (now - activeConnections[mac].last_sent_usb >= 100) {
                    activeConnections[mac].last_sent_usb = now;
                    sendSensorData(mac, finalValue); 
                }
            }
        }
        xSemaphoreGive(bleMutex);
    }
}

class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        String mac = advertisedDevice->getAddress().toString().c_str();
        mac.toUpperCase();
        int rssi = advertisedDevice->getRSSI();
        uint32_t now = millis();
        String name = advertisedDevice->haveName() ? advertisedDevice->getName().c_str() : "";

        if (!currentScanFilters.empty()) {
            bool matched = false;
            for (const String& filterMac : currentScanFilters) {
                if (mac.equalsIgnoreCase(filterMac)) { matched = true; break; }
            }
            if (!matched) return; 
        }

        bool shouldSendUsb = false;

        if (xSemaphoreTake(bleMutex, 0)) {
            bool found = false;
            for (auto& dev : discoveredDevices) {
                if (dev.mac == mac) {
                    if (name != "") dev.name = name; 
                    dev.rssi = rssi;        
                    dev.last_seen = now;    
                    if (now - dev.last_sent_usb >= 2000) {
                        dev.last_sent_usb = now;
                        shouldSendUsb = true;
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                discoveredDevices.push_back({mac, name, rssi, now, now});
                shouldSendUsb = true; 
            }
            xSemaphoreGive(bleMutex);
        }

        if (shouldSendUsb) {
            sendBeaconData(mac, name, rssi);
        }
    }
};

class MyClientCallback : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pclient) {}
    void onDisconnect(NimBLEClient* pclient) {
        String mac = pclient->getPeerAddress().toString().c_str();
        mac.toUpperCase();
        sendStatusMessage("Verbindung getrennt: " + mac);
        
        if (xSemaphoreTake(bleMutex, portMAX_DELAY)) {
            activeConnections.erase(mac);
            NimBLEDevice::deleteClient(pclient);
            xSemaphoreGive(bleMutex);
        }
        adjustScanTiming();
        if (isScanRequested) pBLEScan->start(0, nullptr, false); 
    }
};

void initBLE() {
    bleMutex = xSemaphoreCreateMutex();
    NimBLEDevice::init("T-Dongle-Coproc");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true); 
    pBLEScan->setDuplicateFilter(false); 
    adjustScanTiming();
}

void startScan(std::vector<String> macFilters) {
    if (xSemaphoreTake(bleMutex, portMAX_DELAY)) {
        currentScanFilters = macFilters;
        isScanRequested = true;
        discoveredDevices.clear();
        xSemaphoreGive(bleMutex);
    }
    adjustScanTiming(); 
    pBLEScan->start(0, nullptr, false);
    sendStatusMessage("Scan gestartet");
}

void stopScan() {
    isScanRequested = false;
    pBLEScan->stop();
    sendStatusMessage("Scan gestoppt");
}

void connectToBleServer(String macAddress, SensorConfig config) {
    macAddress.toUpperCase();
    String cleanMac = macAddress;
    cleanMac.trim();
    sendStatusMessage("Verbindungsaufbau: " + cleanMac);
    NimBLEAddress address(cleanMac.c_str());
    
    pBLEScan->stop(); 
    delay(250); 

    NimBLEClient* pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback(), false);
    pClient->setConnectTimeout(10); 

    bool success = false;
    for (int i = 1; i <= 3; i++) {
        sendStatusMessage(String("Versuch ") + i + "/3...");
        if (pClient->connect(address)) { success = true; break; }
        delay(200); 
    }

    if(success) {
        sendStatusMessage("Verbunden! Suche Services...");
        NimBLERemoteService* pService = pClient->getService(config.serviceUUID.c_str());
        if (pService != nullptr) {
            NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(config.charUUID.c_str());
            if (pChar != nullptr && pChar->canNotify()) {
                pChar->subscribe(true, notifyCallback);
                if (xSemaphoreTake(bleMutex, portMAX_DELAY)) {
                    activeConnections[cleanMac] = {pClient, config, "Warte...", 0};
                    xSemaphoreGive(bleMutex);
                }
                sendStatusMessage("ERFOLG: Abonniert!");
                adjustScanTiming();
                if (isScanRequested) pBLEScan->start(0, nullptr, false);
                return;
            }
        }
        sendStatusMessage("Fehler: Service nicht gefunden!");
        pClient->disconnect(); 
    } else {
        sendStatusMessage("Fehler: Timeout nach 3 Versuchen!");
        NimBLEDevice::deleteClient(pClient);
        adjustScanTiming();
        if (isScanRequested) pBLEScan->start(0, nullptr, false); 
    }
}

void disconnectFromBleServer(String macAddress) {
    macAddress.toUpperCase();
    String cleanMac = macAddress;
    cleanMac.trim();
    if (xSemaphoreTake(bleMutex, portMAX_DELAY)) {
        if (activeConnections.find(cleanMac) != activeConnections.end()) {
            activeConnections[cleanMac].pClient->disconnect();
        }
        xSemaphoreGive(bleMutex);
    }
}

std::vector<BleDevice> getDiscoveredDevices() {
    std::vector<BleDevice> safeCopy;
    uint32_t now = millis();
    if (xSemaphoreTake(bleMutex, portMAX_DELAY)) {
        discoveredDevices.erase(
            std::remove_if(discoveredDevices.begin(), discoveredDevices.end(),
                [now](const BleDevice& d) { return (now - d.last_seen) > 30000; }),
            discoveredDevices.end()
        );
        safeCopy = discoveredDevices; 
        xSemaphoreGive(bleMutex); 
    }
    std::sort(safeCopy.begin(), safeCopy.end(), [](const BleDevice& a, const BleDevice& b) { return a.rssi > b.rssi; });
    return safeCopy;
}

String getBleStatusMsg() {
    String msg = "";
    if (xSemaphoreTake(bleMutex, portMAX_DELAY)) {
        if (!activeConnections.empty()) {
            auto it = activeConnections.begin();
            // Saubere Formatierung für das Display
            msg = "MAC: " + it->first.substring(9) + "\nWert: " + it->second.lastValue;
        } else {
            msg = "Warte auf Daten...";
        }
        xSemaphoreGive(bleMutex);
    }
    return msg;
}

bool isBleConnected() { 
    bool connected = false;
    if (xSemaphoreTake(bleMutex, portMAX_DELAY)) {
        connected = !activeConnections.empty();
        xSemaphoreGive(bleMutex);
    }
    return connected;
}

bool isBleScanning() {
    bool scanning = false;
    if (xSemaphoreTake(bleMutex, portMAX_DELAY)) {
        scanning = isScanRequested;
        xSemaphoreGive(bleMutex);
    }
    return scanning;
}