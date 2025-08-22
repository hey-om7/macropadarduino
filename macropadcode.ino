#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <BleKeyboard.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "frontend_html.h"
#include "esp_sleep.h"

#define EEPROM_SIZE 3048
#define MAPPING_COUNT 9
#define MAX_KEYS_PER_MAPPING 6
#define VERSION_EEPROM_ADDR 0  // reserve 50 bytes max
#define WIFI_EEPROM_ADDR 50    // SSID at 50, password at 85
#define KEYMAPPINGS_ADDR 120

// Keymapping Names
#define KEYNAMES_ADDR 2048
#define KEYNAME_MAXLEN 20
#define KEYNAME_COUNT 9
String keyNames[KEYNAME_COUNT];

constexpr int WAKE_MODE_TIMEOUT = 60;  // 60 seconds for wake mode


BleKeyboard bleKeyboard("DLS_MPAD", "Domestic Labs", 100);

const char *fallbackSSID = "DLS_MPAD";
const char *fallbackPassword = "12345678";

// GPIO pin for the buzzer
const int buzzerPin = 7;

// Frequencies for simple beep pattern
// int frequencies[] = { 1000, 1500, 2000 }; // in Hz
// int duration = 200; // milliseconds

WebServer server(80);
String keyMappings[MAPPING_COUNT][MAX_KEYS_PER_MAPPING];

const byte numRows = 3;
const byte numCols = 3;
byte rowPins[numRows] = { 6, 1, 2 };
byte colPins[numCols] = { 3, 4, 5 };
unsigned long lastPressTime[numRows][numCols] = { 0 };
const unsigned long debounceDelay = 300;

String savedSSID = "";
String savedPassword = "";

bool serverStarted = false;
bool keyHeld = false;
unsigned long keyPressStartTime = 0;
bool isWakeModeActive = false;      // Toggle sending mode
unsigned long lastKeySendTime = 0;  // Timer for sending 'a' every 5s

const char *ipServer = "52.66.6.129";
String firmwareBinUrl;
String versionCheckUrl;
String CURRENT_VERSION;

unsigned long keyPressStartTime_BT = 0;
bool keyHeld_BT = false;
bool bluetoothConnected = false;

unsigned long keyPressStartTime_AP = 0;
bool keyHeld_AP = false;

// ---------- EEPROM ----------
void writeStringToEEPROM(int addr, const String &data) {
  byte len = data.length();
  EEPROM.write(addr++, len);
  for (byte i = 0; i < len; i++) {
    EEPROM.write(addr++, data[i]);
  }
}

String readStringFromEEPROM(int addr) {
  byte len = EEPROM.read(addr++);
  String value = "";
  for (byte i = 0; i < len; i++) {
    value += (char)EEPROM.read(addr++);
  }
  return value;
}

void saveKeyNamesToEEPROM(const String names[], int count) {
  int addr = KEYNAMES_ADDR;
  for (int i = 0; i < count; i++) {
    String name = names[i];
    if (name.length() > KEYNAME_MAXLEN) {
      name = name.substring(0, KEYNAME_MAXLEN); // truncate if too long
    }
    byte len = name.length();
    EEPROM.write(addr++, len);
    for (int j = 0; j < len; j++) {
      EEPROM.write(addr++, name[j]);
    }
    // pad with zeros if shorter
    for (int j = len; j < KEYNAME_MAXLEN; j++) {
      EEPROM.write(addr++, 0);
    }
  }
  EEPROM.commit();
}

void loadKeyNamesFromEEPROM(String names[], int count) {
  int addr = KEYNAMES_ADDR;
  for (int i = 0; i < count; i++) {
    byte len = EEPROM.read(addr++);
    if (len > KEYNAME_MAXLEN) len = KEYNAME_MAXLEN; // avoid overflow
    String name = "";
    for (int j = 0; j < len; j++) {
      name += (char)EEPROM.read(addr++);
    }
    addr += (KEYNAME_MAXLEN - len); // skip padding
    names[i] = name;
  }
}

void playBeep(int tuneFrequency = 1000) {
  tone(buzzerPin, tuneFrequency);  // 1000 Hz tone
  delay(80);                       // Beep duration (ms)
  noTone(buzzerPin);               // Stop tone
}

void saveWiFiCredentials(const String &ssid, const String &password) {
  writeStringToEEPROM(WIFI_EEPROM_ADDR, ssid);
  writeStringToEEPROM(WIFI_EEPROM_ADDR + 32, password);
  EEPROM.commit();
}

void loadWiFiCredentials() {
  savedSSID = readStringFromEEPROM(WIFI_EEPROM_ADDR);
  savedPassword = readStringFromEEPROM(WIFI_EEPROM_ADDR + 32);
}

void saveVersionToEEPROM(const String &version) {
  writeStringToEEPROM(VERSION_EEPROM_ADDR, version);
  EEPROM.commit();
}

void loadVersionFromEEPROM() {
  // VERSION_EEPROM_ADDR = 850, 40 bytes reserved
  CURRENT_VERSION = readStringFromEEPROM(VERSION_EEPROM_ADDR);
  if (CURRENT_VERSION.length() == 0 || CURRENT_VERSION.length() > 20) {
    CURRENT_VERSION = "0.0.0";  // default if not stored yet
  }
}

// ---------- Version Comparison ----------
bool isVersionNewer(String current, String latest) {
  int currentMajor, currentMinor, currentPatch;
  int latestMajor, latestMinor, latestPatch;

  sscanf(current.c_str(), "%d.%d.%d", &currentMajor, &currentMinor, &currentPatch);
  sscanf(latest.c_str(), "%d.%d.%d", &latestMajor, &latestMinor, &latestPatch);

  if (latestMajor > currentMajor) return true;
  if (latestMajor == currentMajor && latestMinor > currentMinor) return true;
  if (latestMajor == currentMajor && latestMinor == currentMinor && latestPatch > currentPatch) return true;

  return false;
}

void goToLightSleep(int seconds) {
  Serial.println("Going into light sleep for some seconds...");
  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  esp_light_sleep_start();

  Serial.println("Woke up from light sleep!");
  delay(10000);  // Buffer time for wakeup and reconnect to the BLE device
  reconnectToBLE();
}

void reconnectToBLE() {
  if (!bleKeyboard.isConnected()) {
    while (!bleKeyboard.isConnected()) {
      Serial.println("Reconnecting to BLE device...");
      bleKeyboard.begin();
      delay(2000);  // Wait for reconnection
      if (bleKeyboard.isConnected()) {
        Serial.println("Reconnected to BLE device successfully.");
        playBeep(1200);
      } else {
        Serial.println("Failed to reconnect to BLE device.");
        playBeep(1500);
      }
    }

  } else {
    playBeep(1000);
    delay(1000);
    playBeep(1000);
  }
}


// ---------- OTA Check ----------
void checkForOTAUpdate() {
  WiFiClient client;
  HTTPClient http;

  Serial.println("Checking for latest firmware version...");
  http.begin(client, versionCheckUrl);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    int dataIndex = payload.indexOf("\"data\":\"");
    if (dataIndex != -1) {
      int start = dataIndex + 8;
      int end = payload.indexOf("\"", start);
      String latestVersion = payload.substring(start, end);
      bool canUpdate = isVersionNewer(CURRENT_VERSION, latestVersion);
      Serial.println("Current: " + CURRENT_VERSION + ", Latest: " + latestVersion);
      if (canUpdate) {
        Serial.println("Firmware update available. Starting OTA...");
        httpUpdate.rebootOnUpdate(false);
        t_httpUpdate_return result = httpUpdate.update(client, firmwareBinUrl);

        switch (result) {
          case HTTP_UPDATE_FAILED:
            Serial.printf("Update failed: %s\n", httpUpdate.getLastErrorString().c_str());
            break;
          case HTTP_UPDATE_NO_UPDATES:
            Serial.println("No update available.");
            break;
          case HTTP_UPDATE_OK:
            saveVersionToEEPROM(latestVersion);
            Serial.println("Update successful. Rebooting...");
            delay(500);
            ESP.restart();
            break;
        }
      } else {
        Serial.println("Firmware is already up to date.");
      }
    } else {
      Serial.println("Version parsing failed.");
    }
  } else {
    Serial.printf("Version check failed. HTTP code: %d\n", httpCode);
  }

  http.end();
}

// ---------- WiFi ----------
bool connectToSavedWiFi() {
  if (savedSSID.isEmpty() || savedPassword.isEmpty()) return false;

  WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

  for (int i = 0; i < 10; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to saved WiFi.");
      Serial.println(WiFi.localIP());
      return true;
    }
    delay(1000);
    Serial.print(".");
  }

  Serial.println("Failed to connect to saved WiFi.");
  return false;
}

// ---------- Web Server ----------
void handleSave() {
  if (!server.hasArg("plain")) return server.send(400, "text/plain", "Body not found");

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) return server.send(400, "text/plain", "Invalid JSON");

  if (!doc.is<JsonArray>() || doc.size() != MAPPING_COUNT) {
    return server.send(400, "text/plain", "Expected 9 mappings");
  }

  for (int i = 0; i < MAPPING_COUNT; i++) {
    JsonArray inner = doc[i];
    for (int j = 0; j < MAX_KEYS_PER_MAPPING; j++) {
      keyMappings[i][j] = (j < inner.size()) ? String((const char *)inner[j]) : "";
    }
  }

  saveMappingsToEEPROM();
  server.send(200, "text/plain", "Saved. Restarting...");
  delay(500);     // give response time to send
  ESP.restart();  // restart device
}

void handleGet() {
  StaticJsonDocument<1024> doc;
  for (int i = 0; i < MAPPING_COUNT; i++) {
    JsonArray mapping = doc.createNestedArray();
    for (int j = 0; j < MAX_KEYS_PER_MAPPING; j++) {
      if (keyMappings[i][j].length() > 0) mapping.add(keyMappings[i][j]);
    }
  }
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleWiFiSave() {
  if (!server.hasArg("plain")) return server.send(400, "text/plain", "No JSON");

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) return server.send(400, "text/plain", "Invalid JSON");

  String ssid = doc["ssid"];
  String password = doc["password"];

  if (ssid == "" || password == "") return server.send(400, "text/plain", "Missing ssid/password");

  saveWiFiCredentials(ssid, password);
  server.send(200, "text/plain", "WiFi credentials saved");

  WiFi.begin(ssid.c_str(), password.c_str());
}

void startHotspotAndServer() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(fallbackSSID, fallbackPassword);
  Serial.println("Hotspot started");
  Serial.println("AP IP: " + WiFi.softAPIP().toString());

  // Serve the HTML UI
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "max-age=3600");
    server.send_P(200, "text/html", FRONTEND_HTML);
  });

  // Existing API endpoints
  server.on("/savekeymapping", HTTP_POST, handleSave);
  server.on("/getkeymapping", HTTP_GET, handleGet);
  server.on("/wifi", HTTP_POST, handleWiFiSave);
  server.on("/getkeynames", HTTP_GET, handleGetKeyNames);
  server.on("/savekeynames", HTTP_POST, handleSaveKeyNames);
  server.begin();
  Serial.println("HTTP Server started");
}

void handleSaveKeyNames() {
  if (!server.hasArg("plain")) return server.send(400, "text/plain", "No body");

  StaticJsonDocument<512> doc;  // adjust size if needed
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) return server.send(400, "text/plain", "Invalid JSON");

  if (!doc.is<JsonArray>() || doc.size() != KEYNAME_COUNT) {
    return server.send(400, "text/plain", "Expected JSON array of size 9");
  }

  for (int i = 0; i < KEYNAME_COUNT; i++) {
    String value = String((const char*)doc[i]);
    if (value.length() > KEYNAME_MAXLEN) {
      return server.send(400, "text/plain", "Key name too long (max 20 chars): index " + String(i));
    }
    keyNames[i] = value;
  }

  saveKeyNamesToEEPROM(keyNames, KEYNAME_COUNT);
  server.send(200, "text/plain", "Key names saved successfully");
}

void handleGetKeyNames() {
  loadKeyNamesFromEEPROM(keyNames, KEYNAME_COUNT);

  StaticJsonDocument<512> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < KEYNAME_COUNT; i++) {
    arr.add(keyNames[i]);
  }

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ---------- Key Mapping ----------
void saveMappingsToEEPROM() {
  int addr = KEYMAPPINGS_ADDR;  // start at 120
  for (int i = 0; i < MAPPING_COUNT; i++) {
    for (int j = 0; j < MAX_KEYS_PER_MAPPING; j++) {
      String key = keyMappings[i][j];
      byte len = key.length();
      EEPROM.write(addr++, len);
      for (int k = 0; k < len; k++) {
        EEPROM.write(addr++, key[k]);
      }
    }
  }
  EEPROM.commit();
}

void loadMappingsFromEEPROM() {
  int addr = KEYMAPPINGS_ADDR;  // start at 120
  for (int i = 0; i < MAPPING_COUNT; i++) {
    for (int j = 0; j < MAX_KEYS_PER_MAPPING; j++) {
      byte len = EEPROM.read(addr++);
      String key = "";
      for (int k = 0; k < len; k++) {
        key += (char)EEPROM.read(addr++);
      }
      keyMappings[i][j] = key;
    }
  }
}

void pressMappedKeys(int index) {
  if (!bleKeyboard.isConnected()) return;
  for (int j = 0; j < MAX_KEYS_PER_MAPPING; j++) {
    String key = keyMappings[index][j];
    if (key == "") continue;
    if (key == "KEY_LEFT_GUI") bleKeyboard.press(KEY_LEFT_GUI);
    else if (key == "KEY_LEFT_SHIFT") bleKeyboard.press(KEY_LEFT_SHIFT);
    else if (key == "KEY_LEFT_CTRL") bleKeyboard.press(KEY_LEFT_CTRL);
    else if (key == "KEY_LEFT_ALT") bleKeyboard.press(KEY_LEFT_ALT);
    else if (key.length() == 1) bleKeyboard.press(key[0]);
  }
  delay(10);
  bleKeyboard.releaseAll();
}


void playPowerOnTone() {
  int toneSequence[] = { 800, 1000, 1200, 1400 };  // Increasing pitch
  int toneDuration = 120;                          // duration of each tone (ms)
  int gap = 30;                                    // short gap between tones

  for (int i = 0; i < sizeof(toneSequence) / sizeof(toneSequence[0]); i++) {
    tone(buzzerPin, toneSequence[i]);
    delay(toneDuration);
    noTone(buzzerPin);
    delay(gap);
  }
}

void playTone_SharpBlips() {
  int tones[] = { 1000, 1500, 1000, 1800 };
  for (int i = 0; i < 4; i++) {
    tone(buzzerPin, tones[i]);
    delay(80);
    noTone(buzzerPin);
    delay(50);
  }
}

void playTone_LongLowBeep() {
  tone(buzzerPin, 400);
  delay(600);
  noTone(buzzerPin);
  delay(50);
}

// Service start tone: faster, brighter
void wakeFunctionStartBeep() {
  int tones[] = { 800, 1100, 1500, 1800 };
  for (int i = 0; i < 4; i++) {
    tone(buzzerPin, tones[i]);
    delay(100);
    noTone(buzzerPin);
    delay(40);
  }
}

// Service stop tone: descending beeps
void wakeFunctionStopBeep() {
  int tones[] = { 1200, 700 };
  for (int i = 0; i < 2; i++) {
    tone(buzzerPin, tones[i]);
    delay(200);
    noTone(buzzerPin);
    delay(100);
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
  pinMode(buzzerPin, OUTPUT);
  playPowerOnTone();
  delay(1000);
  EEPROM.begin(EEPROM_SIZE);
  bleKeyboard.begin();
  loadMappingsFromEEPROM();
  loadWiFiCredentials();
  loadVersionFromEEPROM();

  WiFi.mode(WIFI_STA);

  String macAddress = WiFi.macAddress();
  firmwareBinUrl = "http://" + String(ipServer) + ":8080/api/v1/device/firmware?macAddress=" + macAddress;
  versionCheckUrl = "http://" + String(ipServer) + ":8080/api/v1/device/firmware/version?macAddress=" + macAddress;
  Serial.println("Mac address: " + macAddress);

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  pSecurity->setCapability(ESP_IO_CAP_NONE);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  for (byte r = 0; r < numRows; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], HIGH);
  }
  for (byte c = 0; c < numCols; c++) {
    pinMode(colPins[c], INPUT_PULLUP);
  }

  if (connectToSavedWiFi()) {
    checkForOTAUpdate();
    // WiFi.setSleep(true);
    WiFi.disconnect(true);  // disconnect & erase config
    WiFi.mode(WIFI_OFF);    // shut down radio
  }
  setCpuFrequencyMhz(80);
}

char generateRandomLetter() {
  int randomNumber = random(1, 27);        // 1 to 26
  char letter = 'a' + (randomNumber - 1);  // Map 1→'a', 2→'b', ..., 26→'z'
  return letter;
}


int nameToggle = 0;
const String baseName = "DLS_MPAD";

void disconnectBluetooth() {
  if (bleKeyboard.isConnected()) {
    Serial.println("Disconnecting current device...");

    bleKeyboard.end();  // Stop BLE HID service and disconnect
    delay(200);         // Small delay to ensure disconnect

    // Change advertised name to avoid caching issues on hosts
    nameToggle++;
    String newName = baseName + "-" + String(nameToggle);

    // Reinitialize bleKeyboard with new name
    bleKeyboard = BleKeyboard(newName.c_str(), "Domestic Labs", 100);
    bleKeyboard.begin();

    Serial.println("Restarted advertising as: " + newName);
  } else {
    Serial.println("Not connected, just starting advertising...");
    bleKeyboard.begin();
  }
}

// ---------- Loop ----------
void loop() {
  server.handleClient();
  unsigned long now = millis();
  bluetoothConnected = bleKeyboard.isConnected();
  for (byte r = 0; r < numRows; r++) {
    digitalWrite(rowPins[r], LOW);
    for (byte c = 0; c < numCols; c++) {
      bool keyPressed = (digitalRead(colPins[c]) == LOW);

      // --- Special keys with long press actions ---
      if (r == 0 && c == 0) {
        if (keyPressed) {
          if (!keyHeld_BT) {
            keyPressStartTime_BT = now;
            keyHeld_BT = true;
          } else if ((now - keyPressStartTime_BT >= 5000) && bluetoothConnected) {
            Serial.println("5s key hold detected. Disconnecting Bluetooth.");
            playTone_SharpBlips();
            disconnectBluetooth();  // <-- Your function to end BLE/BT
            bluetoothConnected = false;
          }
        } else {
          // Short press: send mapped key
          if (keyHeld_BT && (now - keyPressStartTime_BT < 5000) && (now - lastPressTime[r][c] > debounceDelay)) {
            int keyIndex = r * numCols + c;
            Serial.print("Short press on special key (BT): ");
            Serial.println(keyIndex);
            pressMappedKeys(keyIndex);
            lastPressTime[r][c] = now;
          }
          keyHeld_BT = false;
        }

        // ---- Second button: Start hotspot/server after 5s hold ----
      } else if (r == 1 && c == 0) {
        if (keyPressed) {
          if (!keyHeld_AP) {
            keyPressStartTime_AP = now;
            keyHeld_AP = true;
          } else if ((now - keyPressStartTime_AP >= 5000) && !serverStarted) {
            Serial.println("5s key hold detected. Starting hotspot and server.");
            playTone_SharpBlips();
            startHotspotAndServer();
            serverStarted = true;
          }
        } else {
          // Short press: send mapped key
          if (keyHeld_AP && (now - keyPressStartTime_AP < 5000) && (now - lastPressTime[r][c] > debounceDelay)) {
            int keyIndex = r * numCols + c;
            Serial.print("Short press on special key (AP): ");
            Serial.println(keyIndex);
            pressMappedKeys(keyIndex);
            lastPressTime[r][c] = now;
          }
          keyHeld_AP = false;
        }
      }
      // ---- Third button: Toggle Wake mode ----
      else if (r == 2 && c == 0) {
        if (keyPressed) {
          if (!keyHeld) {
            keyPressStartTime = now;
            keyHeld = true;
          } else if ((now - keyPressStartTime >= 5000)) {
            // Long press: toggle wake mode
            isWakeModeActive = !isWakeModeActive;
            if (isWakeModeActive) {
              wakeFunctionStartBeep();
            } else {
              wakeFunctionStopBeep();
            }
            Serial.println(isWakeModeActive ? "Wake mode activated" : "Wake mode deactivated");
            lastPressTime[r][c] = now;
            keyHeld = false;  // Reset hold state after long press
          }
        } else {
          // Short press: send mapped key
          if (keyHeld && (now - keyPressStartTime < 5000) && (now - lastPressTime[r][c] > debounceDelay)) {
            int keyIndex = r * numCols + c;
            Serial.print("Short press on special key (Wake): ");
            Serial.println(keyIndex);
            pressMappedKeys(keyIndex);
            lastPressTime[r][c] = now;
          }
          keyHeld = false;
        }
      } else {
        // Normal keys
        if (keyPressed && (now - lastPressTime[r][c] > debounceDelay)) {
          int keyIndex = r * numCols + c;
          Serial.print("Key Pressed Index: ");
          Serial.println(keyIndex);
          pressMappedKeys(keyIndex);
          lastPressTime[r][c] = now;
        }
      }
    }
    digitalWrite(rowPins[r], HIGH);
  }

  if (isWakeModeActive && bleKeyboard.isConnected()) {
    char randomLetter = generateRandomLetter();
    bleKeyboard.press(randomLetter);
    delay(10);  // Small delay to simulate keypress
    bleKeyboard.release(randomLetter);
    Serial.println("Sent key: " + String(randomLetter));
    delay(500);
    lastKeySendTime = millis();
    goToLightSleep(WAKE_MODE_TIMEOUT);
  }

  delay(10);  // Small delay to avoid excessive CPU usage
}
