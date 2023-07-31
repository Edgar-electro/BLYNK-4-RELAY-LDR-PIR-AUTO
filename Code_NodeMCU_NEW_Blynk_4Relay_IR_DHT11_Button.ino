#define BLYNK_TEMPLATE_ID "TMPL5lPXsC035"
#define BLYNK_TEMPLATE_NAME "pir test"
#define BLYNK_AUTH_TOKEN "hT1s37PfB0cZqQDZDWO-9s-D8EMfo9HH"



const char * ssid = "SFR_EDD0";
const char * password = "vigen201402";

#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp8266.h>
#include <Preferences.h>
#include <DHT.h>
#include <WiFiManager.h> 
#include <ESP8266WiFi.h> 
#include <AceButton.h>




Preferences pref;

// define the GPIO connected with Relays and switches
#define RelayPin1 5  //d1
#define RelayPin2 4 //d2
#define RelayPin3 0  //d3 
#define RelayPin4 2  //D4

#define SwitchPin1 15  //d8
#define SwitchPin2 14  //D5
#define SwitchPin3 13  //D7
#define SwitchPin4 16

#define wifiLed   16   //D2
#define DHTPIN   3 //D18  pin connected with DHT
#define LDR_PIN   A0 // VN pin connected with LDR
#define PIR_PIN   12



//Change the virtual pins according yours
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
// Uncomment whatever type you're using!
//#define DHTTYPE DHT11     // DHT 11
#define DHTTYPE DHT22   // DHT 22, AM2302, AM2321
//#define DHTTYPE DHT21   // DHT 21, AM2301


// Relay State

bool toggleState_1 = LOW; //Define integer to remember the toggle state for relay 1
bool toggleState_2 = LOW; //Define integer to remember the toggle state for relay 2
bool toggleState_3 = LOW; //Define integer to remember the toggle state for relay 3
bool toggleState_4 = LOW; //Define integer to remember the toggle state for relay 4
bool pirButtState  = LOW;
// Switch State
bool SwitchState_1 = LOW;
bool SwitchState_2 = LOW;
bool SwitchState_3 = LOW;
bool SwitchState_4 = LOW;

bool lastSwitchState_1 = HIGH;
bool lastSwitchState_2 = HIGH;
bool lastSwitchState_3 = HIGH;
bool lastSwitchState_4 = HIGH;



float temperature1 = 0;
float humidity1   = 0;
float ldrVal = 0;
int wifiFlag = 0;
float pirVal = 0;

const int maxLight = 50;
const int minLight = 7;





DHT dht(DHTPIN, DHTTYPE);

char auth[] = BLYNK_AUTH_TOKEN;

BlynkTimer timer;

// When App button is pushed - switch the state
BLYNK_WRITE(VPIN_PIR_ACTIV) {
  pirButtState = param.asInt();
digitalWrite(RelayPin3, !toggleState_3);
  pref.putBool("Relay3", toggleState_3);
}

BLYNK_WRITE(VPIN_BUTTON_1) {
  toggleState_1 = param.asInt();
  digitalWrite(RelayPin1, !toggleState_1);
  pref.putBool("Relay1", toggleState_1);
}

BLYNK_WRITE(VPIN_BUTTON_2) {
  toggleState_2 = param.asInt();
  digitalWrite(RelayPin2, !toggleState_2);
  pref.putBool("Relay2", toggleState_2);
}

BLYNK_WRITE(VPIN_BUTTON_3) {
  toggleState_3 = param.asInt();
  digitalWrite(RelayPin3, !toggleState_3);
  pref.putBool("Relay3", toggleState_3);
}

BLYNK_WRITE(VPIN_BUTTON_4) {
  toggleState_4 = param.asInt();
  digitalWrite(RelayPin4, !toggleState_4);
  pref.putBool("Relay4", toggleState_4);
}

BLYNK_WRITE(VPIN_BUTTON_C) {
  all_SwitchOff();
}

void checkBlynkStatus() { // called every 3 seconds by SimpleTimer

  bool isconnected = Blynk.connected();
  if (isconnected == false) {
    wifiFlag = 1;
    digitalWrite(wifiLed,HIGH );
    Serial.println("Blynk Not Connected");
  }
  if (isconnected == true) {
    wifiFlag = 0;
    digitalWrite(wifiLed,LOW );
    Serial.println("Blynk Connected");
  }
}

BLYNK_CONNECTED() {
  // update the latest state to the server
  Blynk.virtualWrite(VPIN_BUTTON_1, toggleState_1);
  Blynk.virtualWrite(VPIN_BUTTON_2, toggleState_2);
  Blynk.virtualWrite(VPIN_BUTTON_3, toggleState_3);
  Blynk.virtualWrite(VPIN_BUTTON_4, toggleState_4);
                     
  Blynk.syncVirtual(VPIN_TEMPERATURE);
  Blynk.syncVirtual(VPIN_HUMIDITY);
  Blynk.syncVirtual(VPIN_LDR);
  Blynk.syncVirtual (VPIN_PIR);
}

void readSensor() {
  ldrVal = analogRead(LDR_PIN);
 ldrVal = map(ldrVal, 0, 1023, 0, 100);
  Serial.print("LDR - "); Serial.println(ldrVal);
 pirVal = digitalRead(PIR_PIN);
  
  float h = dht.readHumidity();
  float t = dht.readTemperature(); // or dht.readTemperature(true) for Fahrenheit

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  else {
    humidity1 = h;
    temperature1 = t;
    Serial.print("Temperature - "); Serial.println(t);
    Serial.print("Humidity - "); Serial.println(h);
    Serial.print("PIRSensor - "); Serial.println(pirVal);
  }
}

void sendSensor()
{
  readSensor();
  // You can send any value at any time.
  // Please don't send more that 10 values per second.
  Blynk.virtualWrite(VPIN_TEMPERATURE, temperature1);
  Blynk.virtualWrite(VPIN_HUMIDITY, humidity1);
  Blynk.virtualWrite(VPIN_LDR, ldrVal);
  Blynk.virtualWrite(VPIN_PIR, pirVal);   
}

void manual_control(){
  
SwitchState_2 = digitalRead(SwitchPin2);
  if (SwitchState_2 == LOW && lastSwitchState_2 == HIGH) {
    toggleState_2 = !toggleState_2;
    digitalWrite(RelayPin2, toggleState_2);
 } 
  lastSwitchState_2 = SwitchState_2;


  // Обработка второй кнопки
  SwitchState_3 = digitalRead(SwitchPin3);
  if (SwitchState_3 == LOW && lastSwitchState_3 == HIGH) {
    toggleState_3 = !toggleState_3;
    digitalWrite(RelayPin3, toggleState_3);
 } 
  lastSwitchState_3 = SwitchState_3;


SwitchState_4 = digitalRead(SwitchPin4);
  if (SwitchState_4 == LOW && lastSwitchState_4 == HIGH) {
    toggleState_4 = !toggleState_4;
    digitalWrite(RelayPin4, toggleState_4);
  
   }
  lastSwitchState_4 = SwitchState_4;
  
 
}


void all_SwitchOff() {
  toggleState_1 = 0; digitalWrite(RelayPin1, HIGH); pref.putBool("Relay1", toggleState_1); Blynk.virtualWrite(VPIN_BUTTON_1, toggleState_1); delay(100);
  toggleState_2 = 0; digitalWrite(RelayPin2, HIGH); pref.putBool("Relay2", toggleState_2); Blynk.virtualWrite(VPIN_BUTTON_2, toggleState_2); delay(100);
  toggleState_3 = 0; digitalWrite(RelayPin3, HIGH); pref.putBool("Relay3", toggleState_3); Blynk.virtualWrite(VPIN_BUTTON_3, toggleState_3); delay(100);
  toggleState_4 = 0; digitalWrite(RelayPin4, HIGH); pref.putBool("Relay4", toggleState_4); Blynk.virtualWrite(VPIN_BUTTON_4, toggleState_4); delay(100);
  Blynk.virtualWrite(VPIN_TEMPERATURE, temperature1);
  Blynk.virtualWrite(VPIN_HUMIDITY, humidity1);
  Blynk.virtualWrite(VPIN_LDR, ldrVal);
}

void getRelayState()
{
  //Serial.println("reading data from NVS");
  toggleState_1 = pref.getBool("Relay1", 0);
  digitalWrite(RelayPin1, !toggleState_1);
  Blynk.virtualWrite(VPIN_BUTTON_1, toggleState_1);
  delay(200);
  toggleState_2 = pref.getBool("Relay2", 0);
  digitalWrite(RelayPin2, !toggleState_2);
  Blynk.virtualWrite(VPIN_BUTTON_2, toggleState_2);
  delay(200);
  toggleState_3 = pref.getBool("Relay3", 0);
  digitalWrite(RelayPin3, !toggleState_3);
  Blynk.virtualWrite(VPIN_BUTTON_3, toggleState_3);
  delay(200);
  toggleState_4 = pref.getBool("Relay4", 0);
  digitalWrite(RelayPin4, !toggleState_4);
  Blynk.virtualWrite(VPIN_BUTTON_4, toggleState_4);
  delay(200);

  




}

void setup()
{
  Serial.begin(115200);;
  //Open namespace in read-write mode
  pref.begin("Relay_State", false);
  pinMode(PIR_PIN, INPUT);
  pinMode(RelayPin1, OUTPUT);
  pinMode(RelayPin2, OUTPUT);
  pinMode(RelayPin3, OUTPUT);
  pinMode(RelayPin4, OUTPUT);

  pinMode(wifiLed, OUTPUT);

  pinMode(SwitchPin1, INPUT_PULLUP);
  pinMode(SwitchPin2, INPUT_PULLUP);
  pinMode(SwitchPin3, INPUT_PULLUP);
  

  //During Starting all Relays should TURN OFF
  digitalWrite(RelayPin1, !toggleState_1);
  digitalWrite(RelayPin2, !toggleState_2);
  digitalWrite(RelayPin3, !toggleState_3);
  digitalWrite(RelayPin4, !toggleState_4);

  dht.begin();    // Enabling DHT sensor
  digitalWrite(wifiLed, LOW);  

  
  Serial.begin(115200);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    delay(5000);{
    WiFiManager wifiManager;
    wifiManager.autoConnect("BLYNK-IOT");
    Serial.println(WiFi.localIP());  
    }
   WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  //Blynk.begin(auth, ssid, pass);

    
  timer.setInterval(2000L, checkBlynkStatus); // check if Blynk server is connected every 2 seconds
  timer.setInterval(1000L, sendSensor); // Sending Sensor Data to Blynk Cloud every 1 second
  Blynk.config(auth);
  delay(1000);

  getRelayState(); //fetch data from NVS Flash Memory
  //  delay(1000);
}

void loop()
{
  //LDR control Relay 2
      if( ldrVal < minLight){
        if(toggleState_4 == 0){
          digitalWrite(RelayPin4,LOW ); // turn ON relay 2
          toggleState_4 = 1;
          // Update Button Widget
          Blynk.virtualWrite(VPIN_BUTTON_4, toggleState_4);
        }
      }
      else if (ldrVal > maxLight){
        if(toggleState_4 == 1){
           digitalWrite(RelayPin4,HIGH ); // turn OFF relay 2
           toggleState_4 = 0;
           // Update Button Widget
           Blynk.virtualWrite(VPIN_BUTTON_4, toggleState_4);
          }
      } 
    
  if (pirButtState == 1) {
    if (pirVal == 0) {
      digitalWrite(RelayPin3, HIGH);
    Blynk.virtualWrite(VPIN_BUTTON_3, pirVal);
    
    } else if (pirVal == 1) {
       
      digitalWrite(RelayPin3, LOW);
    Blynk.virtualWrite(VPIN_BUTTON_3, pirVal);
    }
  }
  Blynk.run();
  timer.run(); // Initiates SimpleTimer

  manual_control();
}
