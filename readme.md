# 🧠 DLS_MPAD — ESP32 BLE Macro Pad with OTA, Wi-Fi Config, Key Mapping, and Wake Mode

## 📦 Overview

**DLS_MPAD** is a smart Bluetooth LE (BLE) keyboard device built on ESP32, designed for flexible key mapping, wireless updates (OTA), and dynamic mode switching. It supports:

- BLE HID keyboard emulation
- Web-based key remapping and Wi-Fi setup via hotspot
- Firmware update from a remote server
- EEPROM-based persistent settings
- Wake mode: auto-press random characters periodically
- Buzzer sound cues for interaction feedback

---

## 🚀 Features

| Feature                       | Description |
|------------------------------|-------------|
| 🔤 **BLE Keyboard Emulation**  | Uses `BleKeyboard` to send keypresses via Bluetooth to connected host. |
| 🌐 **Wi-Fi Config via Web UI** | Hold a specific key to create an AP + HTTP server for configuring Wi-Fi and key mappings. |
| 🔁 **EEPROM Persistence**     | Stores key mappings, version info, and Wi-Fi credentials. Restores them on boot. |
| 🔄 **OTA Firmware Updates**   | Pulls updates from a backend HTTP server using version matching. |
| 🔘 **Wake Mode**              | Toggle a mode that sends a random keypress (a–z) every 5 seconds. |
| 🔔 **Buzzer Feedback**        | Plays tones to indicate power-on, mode switch, or long-press actions. |
| 🧩 **Flexible Key Mapping**   | Each button can send up to 3 key codes (modifier keys or characters). |

---

## 🛠️ Configurable Components

| Component        | Description |
|------------------|-------------|
| **Key Mappings** | Via web interface (`/getkeymapping`, `/save`) |
| **Wi-Fi SSID & Password** | Via web interface (`/wifi`) |
| **BLE Device Info** | `BleKeyboard("DLS_MPAD", "Domestic Labs", 100);` |
| **OTA Update URLs** | Dynamically built using MAC address and `ipServer` |
| **Buzzer Sounds** | Customizable in `playPowerOnTone()`, `playTone_SharpBlips()`, `playTone_LongLowBeep()` |
| **Keypad Layout** | Configured via `rowPins[]` and `colPins[]` |

---

## 🔌 Pin Assignments

| Pin | Function     |
|-----|--------------|
| 6   | Row 0        |
| 1   | Row 1        |
| 2   | Row 2        |
| 3   | Column 0     |
| 4   | Column 1     |
| 5   | Column 2     |
| 7   | Buzzer       |

---

## ⌨️ Special Button Behaviors

- **(1,0)** → Hold for 10s: Start AP and local HTTP config server  
- **(2,0)** → Toggle Wake Mode: Starts sending random letter every 5s

---

## 📘 Function Breakdown

### 🧠 EEPROM

- `writeStringToEEPROM()` / `readStringFromEEPROM()`  
  Store and retrieve strings like SSID, password, or version.
  
- `saveWiFiCredentials()` / `loadWiFiCredentials()`  
  Handle Wi-Fi persistence.

- `saveMappingsToEEPROM()` / `loadMappingsFromEEPROM()`  
  Store and retrieve key mappings in 9 slots × 3 keys.

- `saveVersionToEEPROM()` / `loadVersionFromEEPROM()`  
  Manage OTA versioning.

---

### 🌐 Wi-Fi & Web Server

- `connectToSavedWiFi()`  
  Attempts auto-connect to stored SSID/PW.

- `startHotspotAndServer()`  
  Creates fallback AP and starts the web config server.

- `handleWiFiSave()`  
  HTTP handler for storing Wi-Fi credentials.

- `handleSave()` / `handleGet()`  
  Save and fetch key mappings as JSON.

---

### 🧩 BLE Keyboard

- `bleKeyboard.begin()`  
  Initializes BLE HID.

- `pressMappedKeys(index)`  
  Reads and sends key presses from mappings.

- `bleKeyboard.press()/release()`  
  Used to send real key events (manual and auto modes).

---

### 🔔 Sound Feedback

- `playPowerOnTone()`  
  Startup tone on boot.

- `playTone_SharpBlips()`  
  Played when hotspot/server is triggered.

- `playTone_LongLowBeep()`  
  Played when Wake Mode is activated.

---

### 🔄 OTA Updates

- `checkForOTAUpdate()`  
  Queries version endpoint and performs HTTP OTA update if a new version is found.

- `isVersionNewer()`  
  Compares `current` and `latest` semantic version strings.

---

### 🔁 Loop Tasks

- Matrix scanning: Detects which key is pressed (with debounce)
- Wake Mode: Periodically sends random character via BLE
- Button long-press detection
- Dynamic sound effects and UI server management

---

## 🧪 Debug/Serial Logs

- Shows BLE connection status
- Logs key presses and indexes
- Displays OTA and Wi-Fi operations
- Useful for field debugging or remote monitoring

---

## 📦 Future Enhancements (Suggested)

- Key repeat or macro sequences
- Configurable wake interval
- Web UI for tone testing
- GUI to drag-drop mappings to keys
- Multi-device Bluetooth pairing memory

## Details on the library used can be found on below link:

- https://github.com/T-vK/ESP32-BLE-Keyboard