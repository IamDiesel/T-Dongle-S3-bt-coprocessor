#include "usb_manager.h"
#include "ble_manager.h"
#include "display_manager.h" 
#include <ArduinoJson.h>
#include <vector>

static SemaphoreHandle_t usbMutex;
static std::vector<String> txBuffer;

void initUSB() {
    usbMutex = xSemaphoreCreateMutex();
}

static void enqueueJson(const JsonDocument& doc, bool isPriority) {
    String output;
    serializeJson(doc, output);
    TickType_t waitTime = isPriority ? pdMS_TO_TICKS(10) : 0; 

    if (xSemaphoreTake(usbMutex, waitTime)) {
        if (txBuffer.size() < 150) { 
            txBuffer.push_back(output);
        }
        xSemaphoreGive(usbMutex);
    }
}

void sendSensorData(String mac, float value) {
    JsonDocument doc;
    doc["event"] = "sensor";
    doc["mac"] = mac;
    doc["value"] = value;
    enqueueJson(doc, true); 
}

void sendBeaconData(String mac, String name, int rssi) {
    JsonDocument doc;
    doc["event"] = "beacon";
    doc["mac"] = mac;
    if(name != "") doc["name"] = name;
    doc["rssi"] = rssi;
    enqueueJson(doc, false); 
}

void sendStatusMessage(String msg) {
    JsonDocument doc;
    doc["event"] = "status";
    doc["msg"] = msg;
    enqueueJson(doc, true); 
}

void sendErrorEvent(String type, String msg, String mac) {
    JsonDocument doc;
    doc["event"] = "error";
    doc["type"] = type;
    doc["msg"] = msg;
    if (mac != "") doc["mac"] = mac;
    enqueueJson(doc, true); 
    
    showErrorDisplay(msg);
}

void processUSBTx() {
    std::vector<String> copyBuffer;
    
    if (xSemaphoreTake(usbMutex, portMAX_DELAY)) {
        if (!txBuffer.empty()) {
            copyBuffer = txBuffer; 
            txBuffer.clear();      
        }
        xSemaphoreGive(usbMutex);
    }

    for (const String& s : copyBuffer) {
        Serial.println(s);
    }
}

void processUSBRx() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if(input.length() == 0) return;

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, input);

        if (!error) {
            String cmd = doc["cmd"] | "";
            
           if (cmd == "scan_start") {
                std::vector<String> filters;
                if (doc["filters"].is<JsonArray>()) {
                    JsonArray arr = doc["filters"].as<JsonArray>();
                    for (JsonVariant v : arr) filters.push_back(v.as<String>());
                }
                startScan(filters);
            }
            else if (cmd == "scan_stop") {
                stopScan();
            }
            else if (cmd == "alarm_on") {
                setAlarmActive(true);
            }
            else if (cmd == "alarm_off") {
                setAlarmActive(false);
            }
            else if (cmd == "brightness") {
                setDisplayBrightness(doc["value"] | 100);
            }
            else if (cmd == "connect") {
                String mac = doc["mac"] | "";
                SensorConfig cfg;
                cfg.serviceUUID = doc["service"] | "FFF0";
                cfg.charUUID    = doc["char"]    | "FFF1";
                cfg.byteOffset  = doc["offset"]  | 0;
                cfg.byteLength  = doc["length"]  | 1;
                cfg.isBigEndian = doc["big_endian"] | true;
                cfg.isSigned    = doc["is_signed"]  | false;
                cfg.multiplier  = doc["multiplier"] | 1.0;
                cfg.addOffset   = doc["add"]        | 0.0;
                
                if (mac != "") connectToBleServer(mac, cfg);
                else sendErrorEvent("invalid_command", "MAC-Adresse fehlt beim Connect!", "");
            } 
            else if (cmd == "disconnect") {
                String mac = doc["mac"] | "";
                if (mac != "") disconnectFromBleServer(mac);
                else sendErrorEvent("invalid_command", "MAC-Adresse fehlt beim Disconnect!", "");
            }
            else {
                sendErrorEvent("invalid_command", "Unbekannter Befehl: " + cmd, "");
            }
        } else {
            if (input == "scan_start") startScan({});
            else if (input == "scan_stop") stopScan();
            else if (input == "alarm_on") setAlarmActive(true);
            else if (input == "alarm_off") setAlarmActive(false);
            else if (input.startsWith("brightness ")) {
                setDisplayBrightness(input.substring(11).toInt());
            }
            else if (input.startsWith("connect ")) {
                String targetMac = input.substring(8);
                targetMac.trim();
                SensorConfig cfg = {"FFF0", "FFF1", 5, 2, true, false, 1.0, 0.0};
                connectToBleServer(targetMac, cfg);
            }
            else if (input.startsWith("disconnect ")) {
                disconnectFromBleServer(input.substring(11));
            }
            else {
                sendErrorEvent("invalid_command", "Unbekannt: " + input, "");
            }
        }
    }
}