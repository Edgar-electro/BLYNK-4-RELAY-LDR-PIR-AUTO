#define BLYNK_TEMPLATE_ID "TMPL5lPXsC035"
#define BLYNK_TEMPLATE_NAME "pir test"
#define BLYNK_AUTH_TOKEN "hT1s37PfB0cZqQDZDWO-9s-D8EMfo9HH"
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <EEPROM.h>

// -------------------- Blynk --------------------
char auth[] = BLYNK_AUTH_TOKEN;
BlynkTimer timer;

// -------------------- Pins --------------------
// Relay pins (active LOW)
#define RelayPin1 5    // D1
#define RelayPin2 4    // D2
#define RelayPin3 0    // D3
#define RelayPin4 2    // D4

// 2 physical buttons
#define SwitchPinA 14  // D5 -> Relay2
#define SwitchPinB 13  // D7 -> Relay3

#define DHTPIN   3
#define LDR_PIN  A0
#define PIR_PIN  12

#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// -------------------- Virtual Pins --------------------
#define VPIN_BUTTON_1    V1
#define VPIN_BUTTON_2    V2
#define VPIN_BUTTON_3    V3
#define VPIN_BUTTON_4    V4
#define VPIN_BUTTON_C    V5
#define VPIN_TEMPERATURE V6
#define VPIN_HUMIDITY    V7
#define VPIN_LDR         V0
#define VPIN_PIR         V8
#define VPIN_PIR_ACTIV   V9

// -------------------- Config --------------------
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 60000; // 1 min
const unsigned long DEBOUNCE_MS = 70;
const int minLight = 7;
const int maxLight = 50;

// -------------------- State --------------------
const uint8_t relayPins[4]  = {RelayPin1, RelayPin2, RelayPin3, RelayPin4};
const uint8_t relayVpins[4] = {VPIN_BUTTON_1, VPIN_BUTTON_2, VPIN_BUTTON_3, VPIN_BUTTON_4};
bool relayState[4] = {false, false, false, false}; // false=OFF, true=ON

const uint8_t buttonPins[2] = {SwitchPinA, SwitchPinB};
const uint8_t buttonRelayIdx[2] = {1, 2}; // A->Relay2, B->Relay3
unsigned long btnDebounceTs[2] = {0, 0};

bool pirModeEnabled = false;
float temperature1 = 0.0f;
float humidity1 = 0.0f;
int ldrVal = 0;
int pirVal = 0;

unsigned long lastBlynkRetry = 0;
unsigned long lastWiFiRetry = 0;

bool lastWiFiConnected = false;
bool lastBlynkConnected = false;

// -------------------- Debug helpers --------------------
void printRelayStates() {
  Serial.print("[RELAYS] R1=");
  Serial.print(relayState[0] ? "ON" : "OFF");
  Serial.print(" R2=");
  Serial.print(relayState[1] ? "ON" : "OFF");
  Serial.print(" R3=");
  Serial.print(relayState[2] ? "ON" : "OFF");
  Serial.print(" R4=");
  Serial.println(relayState[3] ? "ON" : "OFF");
}

void printWiFiInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WIFI] Connected SSID: ");
    Serial.print(WiFi.SSID());
    Serial.print(" | IP: ");
    Serial.print(WiFi.localIP());
    Serial.print(" | RSSI: ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("[WIFI] Not connected");
  }
}

void printSystemStatus() {
  Serial.print("[STATUS] WiFi=");
  Serial.print(WiFi.status() == WL_CONNECTED ? "OK" : "DOWN");
  Serial.print(" Blynk=");
  Serial.print(Blynk.connected() ? "OK" : "DOWN");
  Serial.print(" PIR_MODE=");
  Serial.print(pirModeEnabled ? "ON" : "OFF");
  Serial.print(" PIR=");
  Serial.print(pirVal);
  Serial.print(" LDR=");
  Serial.print(ldrVal);
  Serial.print(" T=");
  Serial.print(temperature1, 1);
  Serial.print("C H=");
  Serial.print(humidity1, 1);
  Serial.println("%");
}

// -------------------- EEPROM --------------------
void eepromSaveBool(int addr, bool value) {
  EEPROM.write(addr, value ? 1 : 0);
  EEPROM.commit();
}

bool eepromReadBool(int addr, bool def = false) {
  uint8_t v = EEPROM.read(addr);
  if (v == 0xFF) return def;
  return (v == 1);
}

// -------------------- Relay helpers --------------------
// Active LOW relay: LOW = ON, HIGH = OFF
void writeRelayHardware(uint8_t idx) {
  digitalWrite(relayPins[idx], relayState[idx] ? LOW : HIGH);
}

void setRelay(uint8_t idx, bool state, bool saveToEeprom, bool syncToApp) {
  bool changed = (relayState[idx] != state);
  relayState[idx] = state;
  writeRelayHardware(idx);

  if (saveToEeprom && changed) {
    eepromSaveBool(idx, state); // addresses 0..3
  }

  if (syncToApp && Blynk.connected()) {
    Blynk.virtualWrite(relayVpins[idx], state ? 1 : 0);
  }

  if (changed) {
    Serial.print("[RELAY] R");
    Serial.print(idx + 1);
    Serial.print(" -> ");
    Serial.println(state ? "ON" : "OFF");
  }
}

void allSwitchOff() {
  Serial.println("[CMD] All relays OFF");
  for (uint8_t i = 0; i < 4; i++) {
    setRelay(i, false, true, true);
    delay(10);
  }
  printRelayStates();
}

// -------------------- WiFi --------------------
// 1) Try saved WiFi for 60s
// 2) If fail -> open WiFiManager AP portal (blocking, no timeout)
bool connectWiFiStable() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(); // connect to last saved credentials

  Serial.println("[BOOT] Trying saved WiFi...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(300);
    Serial.print(".");
    yield();
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[BOOT] Connected to saved WiFi");
    printWiFiInfo();
    return true;
  }

  Serial.println("[BOOT] Saved WiFi not available");
  Serial.println("[BOOT] Starting AP portal: BLYNK-IOT");
  WiFiManager wifiManager;
  wifiManager.setConnectTimeout(60);
  wifiManager.setConfigPortalTimeout(0); // wait until user configures
  bool ok = wifiManager.autoConnect("BLYNK-IOT");

  if (ok && WiFi.status() == WL_CONNECTED) {
    Serial.println("[BOOT] WiFi configured via portal");
    printWiFiInfo();
    return true;
  }

  return false;
}

void maintainConnectivity() {
  bool wifiNow = (WiFi.status() == WL_CONNECTED);
  bool blynkNow = Blynk.connected();

  if (wifiNow != lastWiFiConnected) {
    Serial.println(wifiNow ? "[WIFI] Connection restored" : "[WIFI] Connection lost");
    if (wifiNow) printWiFiInfo();
    lastWiFiConnected = wifiNow;
  }

  if (blynkNow != lastBlynkConnected) {
    Serial.println(blynkNow ? "[BLYNK] Connected" : "[BLYNK] Disconnected");
    lastBlynkConnected = blynkNow;
  }

  if (!wifiNow) {
    if (millis() - lastWiFiRetry > 10000) {
      lastWiFiRetry = millis();
      Serial.println("[WIFI] Reconnect attempt...");
      WiFi.reconnect();
    }
    return;
  }

  if (!blynkNow) {
    if (millis() - lastBlynkRetry > 5000) {
      lastBlynkRetry = millis();
      Serial.println("[BLYNK] Reconnect attempt...");
      Blynk.connect(1000);
    }
  }
}

// -------------------- Blynk handlers --------------------
BLYNK_WRITE(VPIN_PIR_ACTIV) {
  pirModeEnabled = param.asInt();
  eepromSaveBool(10, pirModeEnabled); // address 10
  Serial.print("[APP] PIR mode -> ");
  Serial.println(pirModeEnabled ? "ON" : "OFF");
}

BLYNK_WRITE(VPIN_BUTTON_1) { setRelay(0, param.asInt(), true, false); }
BLYNK_WRITE(VPIN_BUTTON_2) { setRelay(1, param.asInt(), true, false); }
BLYNK_WRITE(VPIN_BUTTON_3) { setRelay(2, param.asInt(), true, false); }
BLYNK_WRITE(VPIN_BUTTON_4) { setRelay(3, param.asInt(), true, false); }

BLYNK_WRITE(VPIN_BUTTON_C) {
  allSwitchOff();
}

BLYNK_CONNECTED() {
  Serial.println("[BLYNK] Sync virtual states");
  for (uint8_t i = 0; i < 4; i++) {
    Blynk.virtualWrite(relayVpins[i], relayState[i] ? 1 : 0);
  }
  Blynk.virtualWrite(VPIN_PIR_ACTIV, pirModeEnabled ? 1 : 0);

  Blynk.virtualWrite(VPIN_TEMPERATURE, temperature1);
  Blynk.virtualWrite(VPIN_HUMIDITY, humidity1);
  Blynk.virtualWrite(VPIN_LDR, ldrVal);
  Blynk.virtualWrite(VPIN_PIR, pirVal);

  printRelayStates();
}

// -------------------- Sensors / automation --------------------
void readSensor() {
  ldrVal = map(analogRead(LDR_PIN), 0, 1023, 0, 100);
  pirVal = digitalRead(PIR_PIN);

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    humidity1 = h;
    temperature1 = t;
  }
}

void sendSensor() {
  readSensor();

  if (Blynk.connected()) {
    Blynk.virtualWrite(VPIN_TEMPERATURE, temperature1);
    Blynk.virtualWrite(VPIN_HUMIDITY, humidity1);
    Blynk.virtualWrite(VPIN_LDR, ldrVal);
    Blynk.virtualWrite(VPIN_PIR, pirVal);
  }
}

void runAutomation() {
  // LDR controls Relay4
  if (ldrVal < minLight && !relayState[3]) {
    setRelay(3, true, true, true);
  } else if (ldrVal > maxLight && relayState[3]) {
    setRelay(3, false, true, true);
  }

  // PIR controls Relay3 when mode enabled
  if (pirModeEnabled) {
    bool target = (pirVal == 1);
    if (relayState[2] != target) {
      setRelay(2, target, true, true);
    }
  }
}

// -------------------- Manual control (fixed debounce) --------------------
void manualControl() {
  static bool lastRawState[2] = {HIGH, HIGH};
  static bool stableState[2]  = {HIGH, HIGH};

  for (uint8_t i = 0; i < 2; i++) {
    bool raw = digitalRead(buttonPins[i]);

    if (raw != lastRawState[i]) {
      btnDebounceTs[i] = millis();
      lastRawState[i] = raw;
    }

    if ((millis() - btnDebounceTs[i]) > DEBOUNCE_MS) {
      if (stableState[i] != raw) {
        stableState[i] = raw;

        // Press event: HIGH -> LOW (INPUT_PULLUP)
        if (stableState[i] == LOW) {
          uint8_t relayIdx = buttonRelayIdx[i];
          Serial.print("[BTN] Button ");
          Serial.print(i == 0 ? "A" : "B");
          Serial.println(" pressed");
          setRelay(relayIdx, !relayState[relayIdx], true, true);
        }
      }
    }
  }
}

// -------------------- Setup / Loop --------------------
void setup() {
  Serial.begin(115200);
  EEPROM.begin(64);

  Serial.println();
  Serial.println("========== BOOT ==========");

  pinMode(PIR_PIN, INPUT);
  

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
  }

  pinMode(SwitchPinA, INPUT_PULLUP);
  pinMode(SwitchPinB, INPUT_PULLUP);

  // Restore saved states
  for (uint8_t i = 0; i < 4; i++) {
    relayState[i] = eepromReadBool(i, false);
    writeRelayHardware(i);
  }
  pirModeEnabled = eepromReadBool(10, false);

  Serial.println("[BOOT] Restored states from EEPROM");
  printRelayStates();
  Serial.print("[BOOT] PIR mode: ");
  Serial.println(pirModeEnabled ? "ON" : "OFF");

  dht.begin();

  bool wifiOk = connectWiFiStable();
  if (!wifiOk) {
    Serial.println("[BOOT] WiFi failed, restarting...");
    delay(2000);
    ESP.restart();
  }

  Blynk.config(auth);
  Blynk.connect(5000);

  lastWiFiConnected = (WiFi.status() == WL_CONNECTED);
  lastBlynkConnected = Blynk.connected();

  timer.setInterval(1000L, sendSensor);
  timer.setInterval(500L, runAutomation);
  timer.setInterval(5000L, printSystemStatus); // периодический мониторинг

  Serial.println("[BOOT] Setup complete");
  Serial.println("==========================");
}

void loop() {
  manualControl();      // buttons first
  maintainConnectivity();
  timer.run();

  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
}
