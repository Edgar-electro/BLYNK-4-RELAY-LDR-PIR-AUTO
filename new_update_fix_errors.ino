#define BLYNK_TEMPLATE_ID "TMPL5lPXsC03"
#define BLYNK_TEMPLATE_NAME "pir test"
#define BLYNK_AUTH_TOKEN "hT1s37PfB0cZqQDZDWO9s-D8EMfo9HH"
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <EEPROM.h>

// -------------------- Blynk --------------------
char auth[] = BLYNK_AUTH_TOKEN;
BlynkTimer timer;
WiFiManager wm;

// -------------------- Pins --------------------
#define RelayPin1 5    // D1
#define RelayPin2 4    // D2
#define RelayPin3 0    // D3
#define RelayPin4 2    // D4

// Физически используются 2 кнопки
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
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 60000; // 1 мин
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

unsigned long lastWifiRetryMs = 0;
unsigned long lastBlynkRetryMs = 0;

// -------------------- EEPROM --------------------
void eepromSaveBool(int addr, bool value) {
  EEPROM.write(addr, value ? 1 : 0);
  EEPROM.commit();
}

bool eepromReadBool(int addr, bool def = false) {
  uint8_t v = EEPROM.read(addr);
  if (v == 0xFF) return def;
  return v == 1;
}

// -------------------- Relay --------------------
// Active LOW relay: LOW = ON, HIGH = OFF
void writeRelayHardware(uint8_t idx) {
  digitalWrite(relayPins[idx], relayState[idx] ? LOW : HIGH);
}

void setRelay(uint8_t idx, bool state, bool saveToEeprom, bool syncToApp) {
  relayState[idx] = state;
  writeRelayHardware(idx);

  if (saveToEeprom) {
    eepromSaveBool(idx, state); // 0..3
  }

  if (syncToApp && Blynk.connected()) {
    Blynk.virtualWrite(relayVpins[idx], state ? 1 : 0);
  }
}

void allSwitchOff() {
  for (uint8_t i = 0; i < 4; i++) {
    setRelay(i, false, true, true);
    delay(10);
  }
}

// -------------------- Wi-Fi --------------------
bool connectToSavedWiFi(unsigned long timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(); // последняя сохраненная сеть

  Serial.println("Trying saved WiFi...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(300);
    Serial.print(".");
    yield();
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Saved WiFi not available.");
  return false;
}

bool openConfigPortalBlocking() {
  // 0 = без таймаута, ждать пока пользователь настроит
  wm.setConfigPortalTimeout(0);
  wm.setBreakAfterConfig(true);

  Serial.println("Starting AP portal: BLYNK-IOT");
  bool ok = wm.startConfigPortal("BLYNK-IOT");

  if (ok && WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi configured. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Portal finished without WiFi.");
  return false;
}

// -------------------- Blynk handlers --------------------
BLYNK_WRITE(VPIN_PIR_ACTIV) {
  pirModeEnabled = param.asInt();
  eepromSaveBool(10, pirModeEnabled);
}

BLYNK_WRITE(VPIN_BUTTON_1) { setRelay(0, param.asInt(), true, false); }
BLYNK_WRITE(VPIN_BUTTON_2) { setRelay(1, param.asInt(), true, false); }
BLYNK_WRITE(VPIN_BUTTON_3) { setRelay(2, param.asInt(), true, false); }
BLYNK_WRITE(VPIN_BUTTON_4) { setRelay(3, param.asInt(), true, false); }

BLYNK_WRITE(VPIN_BUTTON_C) {
  allSwitchOff();
}

BLYNK_CONNECTED() {
  for (uint8_t i = 0; i < 4; i++) {
    Blynk.virtualWrite(relayVpins[i], relayState[i] ? 1 : 0);
  }
  Blynk.virtualWrite(VPIN_PIR_ACTIV, pirModeEnabled ? 1 : 0);

  Blynk.virtualWrite(VPIN_TEMPERATURE, temperature1);
  Blynk.virtualWrite(VPIN_HUMIDITY, humidity1);
  Blynk.virtualWrite(VPIN_LDR, ldrVal);
  Blynk.virtualWrite(VPIN_PIR, pirVal);
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
  // LDR -> Relay4
  if (ldrVal < minLight && !relayState[3]) {
    setRelay(3, true, true, true);
  } else if (ldrVal > maxLight && relayState[3]) {
    setRelay(3, false, true, true);
  }

  // PIR mode -> Relay3
  if (pirModeEnabled) {
    bool target = (pirVal == 1);
    if (relayState[2] != target) {
      setRelay(2, target, true, true);
    }
  }
}

// -------------------- Manual buttons --------------------
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

        // Нажатие: HIGH -> LOW (INPUT_PULLUP)
        if (stableState[i] == LOW) {
          uint8_t relayIdx = buttonRelayIdx[i];
          setRelay(relayIdx, !relayState[relayIdx], true, true);
        }
      }
    }
  }

}
// -------------------- Connectivity --------------------
void handleConnectivity() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiRetryMs > 10000) {
      lastWifiRetryMs = millis();
      WiFi.reconnect();
    }
    return;
  }

  if (!Blynk.connected()) {
    if (millis() - lastBlynkRetryMs > 5000) {
      lastBlynkRetryMs = millis();
      Blynk.connect(1000);
    }
  }
}

// -------------------- Setup / Loop --------------------
void setup() {
  Serial.begin(115200);
  EEPROM.begin(64);

  pinMode(PIR_PIN, INPUT);
 

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
  }
  pinMode(SwitchPinA, INPUT_PULLUP);
  pinMode(SwitchPinB, INPUT_PULLUP);

  // Восстановление состояний
  for (uint8_t i = 0; i < 4; i++) {
    relayState[i] = eepromReadBool(i, false);
    writeRelayHardware(i);
  }
  pirModeEnabled = eepromReadBool(10, false);

  dht.begin();

  // 1) Пробуем старую сеть 60 сек
  bool wifiOk = connectToSavedWiFi(WIFI_CONNECT_TIMEOUT_MS);

  // 2) Если не подключились - AP портал без таймаута (ждет настройки)
  if (!wifiOk) {
    wifiOk = openConfigPortalBlocking();
  }

  // Blynk
  Blynk.config(auth);
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.connect(5000);
  }

  timer.setInterval(1000L, sendSensor);
  timer.setInterval(500L, runAutomation);
}

void loop() {
  manualControl();     // кнопки всегда обрабатываются
  handleConnectivity();
  timer.run();

  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
}
