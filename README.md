
---

# Lola OS - Smart Home & BLE Hub

Lola OS ist eine maßgeschneiderte Firmware für das **M5Tab**, die als zentrale Smart-Home-Schnittstelle, Baby-Monitor und hochleistungsfähiger Bluetooth-Hub dient.

Um die Hardware-Limitierungen des internen ESP32-Funkchips bei gleichzeitiger Nutzung von WLAN (MQTT/Video-Streaming) und Bluetooth zu umgehen, nutzt dieses Projekt eine Dual-Core-Architektur: Das M5Tab fungiert als Host, während ein per USB angeschlossener **LilyGO T-Dongle-S3** als dedizierter BLE-Coprozessor arbeitet.

## ✨ Features

* **Dual-Core Hardware Architektur:** Nahtlose Trennung von rechenintensiver UI/WLAN-Logik (M5Tab) und zeitkritischen Funkprotokollen (T-Dongle-S3).
* **Dynamisches BLE Time-Slicing:** Der Coprozessor teilt seine Antennenzeit intelligent zwischen aktivem Scannen (Beacons/Tracker) und aktiven Sensorverbindungen auf.
* **Edge Processing:** Rohdaten von Bluetooth-Sensoren (z. B. Druckmatten) werden direkt auf dem Dongle verarbeitet und als saubere JSON-Fließkommazahlen an das Tab übergeben.
* **Smart Home Integration:** Native Anbindung an Home Assistant via MQTT.
* **Baby-Monitor:** Integrierter Audio- und Video-Stream (MJPEG) direkt auf dem Dashboard.
* **LVGL Benutzeroberfläche:** Flüssiges, responsives Touch-Interface mit Dark-Mode, Echtzeit-Graphen und dynamischen Einstellungsmenüs.

## 🛠️ Hardware-Voraussetzungen

1. **M5Stack M5Tab** (Hauptrechner, Display, Audio, WLAN)
2. **LilyGO T-Dongle-S3** (BLE Coprozessor, angesteckt über USB-C)
3. Kompatible BLE Sensoren (z. B. CatMat Drucksensor, Kippy Tracker, Shelly BLE Beacons)

## 📁 Repository-Struktur

Das Projekt ist in zwei Hauptkomponenten unterteilt, die beide über PlatformIO verwaltet werden:

```text
├── src/                  # Lola OS (M5Tab Firmware)
│   ├── config/           # Globale Einstellungen, Secrets & SharedData
│   ├── gui/              # LVGL-basierte Benutzeroberfläche (Dashboards, Popups)
│   ├── ha/               # Home Assistant & MQTT Logik
│   ├── media/            # Audio- und Video-Streaming
│   ├── system/           # Kernlogik (BLE Parser, WebSetup, LVGL Driver)
│   └── main.cpp          # Einstiegspunkt M5Tab
│
├── dongle_firmware/      # Coprozessor Firmware (LilyGO T-Dongle-S3)
│   ├── ble_manager.cpp   # Funk-Logik, Auto-Retries & Time-Slicing
│   ├── usb_manager.cpp   # USB-CDC Kommunikation & JSON Puffer
│   └── factory_screen.ino# Einstiegspunkt Dongle
│
├── platformio.ini        # Build-Konfigurationen für beide Geräte
└── README.md             # Diese Dokumentation

```

## 🚀 Installation & Kompilieren

Dieses Projekt verwendet **PlatformIO**.

### 1. Vorbereitung

Erstelle eine Datei `src/config/secrets.h` basierend auf der Vorlage `secrets_dummy.h` und trage deine WLAN- und MQTT-Daten ein.

### 2. Coprozessor flashen (T-Dongle-S3)

1. Stecke den T-Dongle-S3 in deinen PC.
2. Wähle in PlatformIO das Environment `env:T-Dongle-S3`.
3. Kompiliere den Code aus dem Ordner `dongle_firmware` und lade ihn hoch.

### 3. Host flashen (M5Tab)

1. Stecke das M5Tab an deinen PC.
2. Wähle in PlatformIO das Environment für das M5Tab (z.B. `esp32p4_m5stack`).
3. Kompiliere und flashe den Code.
4. Schließe den Dongle an den USB-C-Port des M5Tabs an. Das System initialisiert die USB-CDC-Verbindung automatisch.

## 🔌 Kommunikationsprotokoll (Host <-> Dongle)

Die Kommunikation erfolgt über **USB-CDC (115200 Baud)** via JSON. Der Dongle besitzt eine integrierte Anti-Spam-Drosselung (Beacons max. alle 2 Sekunden, Sensordaten max. 10Hz), um den USB-Puffer des Host-Systems zu schonen.

**Beispiel: M5Tab sendet Verbindungsbefehl:**

```json
{
  "cmd": "connect",
  "mac": "70:4B:CA:46:22:6E",
  "service": "FFF0",
  "char": "FFF1",
  "offset": 5,
  "length": 2
}

```

**Beispiel: Dongle meldet Sensorwert zurück:**

```json
{
  "event": "sensor",
  "mac": "70:4B:CA:46:22:6E",
  "value": 4531.0
}

```

*(Für eine vollständige Auflistung aller Befehle siehe die interne Schnittstellenspezifikation).*