# 🧠 DLS_MPAD — ESP32 BLE Macro Pad with OTA, Wi-Fi Config, Key Mapping, and Wake Mode

## 📦 Overview

**DLS_MPAD** is a smart Bluetooth LE (BLE) macro keyboard built on ESP32, designed for **flexible key mapping**, **OTA firmware updates**, **Wi-Fi configuration via hotspot**, and **power-saving wake mode**.  
It now includes **light sleep wake handling**, **BLE disconnection/re-advertising**, and **extended key mappings**.


---

## 🚀 Features

| Feature                        | Description |
|--------------------------------|-------------|
| 🔤 **BLE Keyboard Emulation**   | Emulates a HID keyboard over Bluetooth to send key presses. |
| 🌐 **Web-based Config (Hotspot)** | Hold a key to start AP + web server for configuring Wi-Fi and remapping keys via an HTML UI. |
| 🔄 **EEPROM Persistence**      | Stores key mappings, version info, and Wi-Fi credentials. Restores them on boot. |
| 🔁 **OTA Firmware Updates**    | Automatically checks backend server for new firmware and updates. |
| 🔘 **Wake Mode + Light Sleep** | Periodically sends random characters, then puts ESP32 into **light sleep** for power savings. |
| 🔌 **BLE Disconnect & Reconnect** | Long-press special key to disconnect Bluetooth and restart advertising with a new name. |
| 🔔 **Buzzer Sound Feedback**   | Multiple tones for startup, mode switches, and special actions. |
| 🧩 **Extended Key Mapping**    | Each button can send up to **6 key codes** (was 3 in older version). |

---

## 🛠️ Configurable Components

| Component              | Description |
|------------------------|-------------|
| **Key Mappings**       | Web interface: `/getkeymapping`, `/savekeymapping` |
| **Wi-Fi SSID & Password** | Web interface: `/wifi` |
| **BLE Device Info** | `BleKeyboard("DLS_MPAD", "Domestic Labs", 100);` |
| **OTA Update URLs**    | Built dynamically using device MAC + `ipServer` |
| **Buzzer Sounds**      | Customizable in helper functions (`playPowerOnTone()`, `wakeFunctionStartBeep()`, etc.) |
| **Keypad Layout**      | Configured with `rowPins[]` and `colPins[]` |
| **Sleep Timeout**      | Wake mode timeout (`WAKE_MODE_TIMEOUT`, default 60s) |

---

## 🔌 Pin Assignments

| Pin | Function |
|-----|----------|
| 6   | Row 0    |
| 1   | Row 1    |
| 2   | Row 2    |
| 3   | Column 0 |
| 4   | Column 1 |
| 5   | Column 2 |
| 7   | Buzzer   |

---

## ⌨️ Special Button Behaviors

| Button (Row,Col) | Short Press | Long Press (>5s) |
|------------------|-------------|------------------|
| **(0,0)**        | Normal mapped key | Disconnect Bluetooth & restart advertising with new name |
| **(1,0)**        | Normal mapped key | Start Wi-Fi AP + Web Config Server |
| **(2,0)**        | Normal mapped key | Toggle **Wake Mode** (auto key send + light sleep) |

---

## 📘 Function Breakdown

### 🧠 EEPROM
- `writeStringToEEPROM()` / `readStringFromEEPROM()`  
  Store and retrieve strings like SSID, password, or version.
  
- `saveWiFiCredentials()` / `loadWiFiCredentials()`  
  Handle Wi-Fi persistence.

- `saveMappingsToEEPROM()` / `loadMappingsFromEEPROM()`  
  Store and retrieve key mappings in 9 slots × 6 keys, Key addresses reorganized (`VERSION_EEPROM_ADDR=0`, `WIFI_EEPROM_ADDR=50`, `KEYMAPPINGS_ADDR=120`).

- `saveVersionToEEPROM()` / `loadVersionFromEEPROM()`  
  Manage OTA versioning.

### 🌐 Wi-Fi & Web Server
- `startHotspotAndServer()` → Launches AP + Web UI (`frontend_html.h`).  
- Endpoints:  
  - `/` → Config UI  
  - `/savekeymapping` → Save mappings (auto-restarts device)  
  - `/getkeymapping` → Fetch mappings  
  - `/wifi` → Save Wi-Fi credentials  
- Endpoints Functions:
  - `connectToSavedWiFi()`  
  Attempts auto-connect to stored SSID/PW.

  - `startHotspotAndServer()`  
    Creates fallback AP and starts the web config server.

  - `handleWiFiSave()`  
    HTTP handler for storing Wi-Fi credentials.

  - `handleSave()` / `handleGet()`  
    Save and fetch key mappings as JSON.

### 🧩 BLE Keyboard
- `pressMappedKeys()` sends multiple key codes (up to 6).  
- Supports modifiers (`CTRL`, `ALT`, `SHIFT`, `GUI`).  
- `disconnectBluetooth()` safely disconnects, increments device name, and restarts advertising. // Increment name needs to be changed.

### 🔔 Sound Feedback
- `playPowerOnTone()`  
  Startup tone on boot.

- `playTone_SharpBlips()`  
  Played when hotspot/server is triggered.

- `playTone_LongLowBeep()`  
  Played when Wake Mode is activated.

### 🔄 OTA Updates

- `checkForOTAUpdate()`  
  Queries version endpoint and performs HTTP OTA update if a new version is found.

- `isVersionNewer()`  
  Compares `current` and `latest` semantic version strings.

### 🌙 Power Saving
- **Wake Mode**: sends random key every ~0.5s, then enters **light sleep** for `WAKE_MODE_TIMEOUT` (default 60s).  
- `goToLightSleep()` handles timed sleep and reconnection to BLE.  
- CPU frequency reduced to **80MHz** for lower idle consumption.

---


### 🔁 Loop Tasks

- Matrix scanning: Detects which key is pressed (with debounce)
- Wake Mode: Periodically sends random character via BLE
- Button long-press detection
- Dynamic sound effects and UI server management

---


## 🧪 Debug/Serial Logs
- Shows OTA version check, BLE connect/disconnect, Wi-Fi config, sleep/wake transitions.  
- Logs key presses and indexes.  
- Confirms when BLE reconnects after sleep.

---

## 📦 Future Enhancements 
- 📝 Support for macro sequences  
- 🔋 Smarter deep sleep after long inactivity  

---

## 📚 Reference
Library used for BLE HID:  
➡️ [ESP32-BLE-Keyboard](https://github.com/T-vK/ESP32-BLE-Keyboard)

---

## ✅ Current TODOs
- Enter **deep sleep** after ~30 mins of inactivity.  
- Smarter Bluetooth reconnect handling. 
- Light sleep when any key is not pressed for more than 2 minutes.
- Disconnect from Bluetooth is not possible right now.