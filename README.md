
# Lola OS - BLE Coprozessor (T-Dongle-S3)
Dieses Repository enthält die Firmware für den dedizierten Bluetooth-Coprozessor des Lola OS Smart Home Systems.
Das Hauptprojekt (die Firmware für das Host-System / M5Tab) findest du hier: https://github.com/IamDiesel/Lela


## 🧠 Warum ein Coprozessor?

Bluetooth Low Energy (BLE) und intensives WLAN (Video-Streaming, MQTT) teilen sich auf einem ESP32 dieselbe 2.4 GHz Antenne. Wenn das Host-System gleichzeitig nach Beacons scannt und hohe Datenmengen streamt, kommt es zur sogenannten **Radio Starvation** (die Antenne "verhungert"). Verbindungen brechen ab oder Pakete gehen verloren.

Dieses Projekt löst das Problem architektonisch: Ein per USB angeschlossener **LilyGO T-Dongle-S3** übernimmt 100% der Bluetooth-Aufgaben. Er verarbeitet alle Scans und Sensorverbindungen autark und übergibt dem M5Tab über USB-CDC nur noch saubere, aufbereitete JSON-Daten.

## ✨ Kern-Features

* **Dynamisches Time-Slicing:** Der Dongle erkennt automatisch, ob er "nur scannen" oder auch Sensoren (z.B. Druckmatten) abfragen muss. Er passt seine Funk-Fenster dynamisch an (z.B. 99ms Scan-Zeit vs. 30ms Scan-Zeit im Mischbetrieb), um verbundenen Sensoren genug "Atemluft" zu geben.
* **Intelligente Anti-Spam Drosselung:** Ein Scanner in einer typischen Umgebung empfängt oft hunderte Pakete pro Sekunde, was die Grafik-Engine (LVGL) des Host-Systems zum Absturz bringen kann. Dieser Coprozessor drosselt den Traffic intelligent:
* Umgebungs-Beacons werden max. alle **2 Sekunden** an das Host-System gemeldet.
* Aktive Sensordaten werden auf **10 Updates pro Sekunde (10 Hz)** limitiert.


* **Lock-Free USB-Buffer (Swap-and-Print):** Ein hochpriorisierter Thread sammelt BLE-Interrupts in einem Vektor. Um *Priority Inversions* mit der langsameren seriellen USB-Ausgabe zu vermeiden, wird der Puffer in Mikrosekunden getauscht. Der Host erhält dadurch flüssige JSON-Strings ohne Fragmentierung.
* **Edge-Computing & Parsing:** Der Dongle empfängt eine dynamische JSON-Konfiguration (Offset, Length, Endianness, Multiplikator) vom Host, liest die rohen Bytes des Bluetooth-Sensors aus und sendet fertige Fließkommazahlen (Floats) zurück.

## 🛠️ Hardware & Installation

* **Hardware:** LilyGO T-Dongle-S3 (ESP32-S3)
* **Schnittstelle:** USB-C (wird vom M5Tab als USB-Host mit Strom versorgt)
* **Framework:** Arduino (via PlatformIO)

### Kompilieren

Das Projekt ist für **PlatformIO** vorkonfiguriert.

1. Repository klonen.
2. Den T-Dongle-S3 in den PC einstecken.
3. Den Upload über das Environment `env:T-Dongle-S3` starten.

## 🔌 API / Kommunikationsprotokoll (JSON)

Die Kommunikation läuft über die USB-CDC Schnittstelle (115200 Baud).

### 1. Befehle vom Host an den Dongle (RX)

**Scannen:**

```json
{"cmd": "scan_start", "filters": ["70:4B:CA:46:22:6E"]}
{"cmd": "scan_stop"}

```

**Verbindung zu einem Sensor aufbauen (mit Edge-Processing Parametern):**

```json
{
  "cmd": "connect",
  "mac": "70:4B:CA:46:22:6E",
  "service": "FFF0",
  "char": "FFF1",
  "offset": 5,
  "length": 2,
  "big_endian": true,
  "is_signed": false,
  "multiplier": 1.0,
  "add": 0.0
}

```

### 2. Events vom Dongle an den Host (TX)

**Umgebungsgeräte (Beacons - gedrosselt auf 0.5 Hz):**

```json
{"event":"beacon","mac":"70:4B:CA:46:22:6E","name":"CatMat-EXT","rssi":-67}

```

**Fertig berechnete Sensorwerte (gedrosselt auf 10 Hz):**

```json
{"event":"sensor","mac":"70:4B:CA:46:22:6E","value":4531.0}

```

**Diagnose & Status:**

```json
{"event":"status","msg":"Physisch verbunden! Suche Services..."}

```
