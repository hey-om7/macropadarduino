#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <BleKeyboard.h>

#define EEPROM_SIZE 1024
#define MAPPING_COUNT 9
#define MAX_KEYS_PER_MAPPING 3
#define MAX_KEY_NAME_LEN 16

// BLE Keyboard instance
BleKeyboard bleKeyboard("DLS_MPAD", "Domestic Labs", 100);

// WiFi credentials (AP Mode)
const char* ssid = "DLS_MPAD";
const char* password = "12345678";

WebServer server(80);
String keyMappings[MAPPING_COUNT][MAX_KEYS_PER_MAPPING];

// Matrix keypad setup
const byte numRows = 3;
const byte numCols = 3;
byte rowPins[numRows] = {6, 1, 2};
byte colPins[numCols] = {3, 4, 5};
unsigned long lastPressTime[numRows][numCols] = {0};
const unsigned long debounceDelay = 300;

void saveMappingsToEEPROM() {
  int addr = 0;
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
  Serial.println("Saved to EEPROM");
}

void loadMappingsFromEEPROM() {
  int addr = 0;
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
  Serial.println("Loaded from EEPROM");
}

void handleSave() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Body not found");
    return;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));

  if (err) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  if (!doc.is<JsonArray>() || doc.size() != MAPPING_COUNT) {
    server.send(400, "text/plain", "Expected 9 mappings");
    return;
  }

  for (int i = 0; i < MAPPING_COUNT; i++) {
    JsonArray inner = doc[i];
    for (int j = 0; j < MAX_KEYS_PER_MAPPING; j++) {
      keyMappings[i][j] = (j < inner.size()) ? String((const char*)inner[j]) : "";
    }
  }

  saveMappingsToEEPROM();
  server.send(200, "text/plain", "Saved");
}

void handleGet() {
  StaticJsonDocument<1024> doc;
  for (int i = 0; i < MAPPING_COUNT; i++) {
    JsonArray mapping = doc.createNestedArray();
    for (int j = 0; j < MAX_KEYS_PER_MAPPING; j++) {
      if (keyMappings[i][j].length() > 0) {
        mapping.add(keyMappings[i][j]);
      }
    }
  }
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
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

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  pSecurity->setCapability(ESP_IO_CAP_NONE);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  bleKeyboard.begin();
  loadMappingsFromEEPROM();

  for (byte r = 0; r < numRows; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], HIGH);
  }
  for (byte c = 0; c < numCols; c++) {
    pinMode(colPins[c], INPUT_PULLUP);
  }

  WiFi.softAP(ssid, password);
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/save", HTTP_POST, handleSave);
  server.on("/getkeymapping", HTTP_GET, handleGet);
  server.begin();
  Serial.println("HTTP Server started");
}

void loop() {
  server.handleClient();
  unsigned long now = millis();

  for (byte r = 0; r < numRows; r++) {
    digitalWrite(rowPins[r], LOW);
    for (byte c = 0; c < numCols; c++) {
      if (digitalRead(colPins[c]) == LOW && (now - lastPressTime[r][c] > debounceDelay)) {
        int keyIndex = r * numCols + c;
        Serial.print("Pressed key index: ");
        Serial.println(keyIndex);
        pressMappedKeys(keyIndex);
        lastPressTime[r][c] = now;
      }
    }
    digitalWrite(rowPins[r], HIGH);
  }
  delay(10);
}
