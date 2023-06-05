#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include "DHT.h"
#include "time.h"

#include "secrets.h"

#define CO_PIN 34
#define CO_A 99.042
#define CO_B -1.518

#define H4_PIN 32
#define H4_A 1163.8
#define H4_B -3.874

#define DUST_PIN 35
#define DUST_A 0.046318
#define DUST_B -19.804
#define DUST_S 280
#define DUST_D 40

#define FAN_PIN 13
DHT dht(15, DHT11);
#define DUST_LED 14

#define TEMP_A 0.56930
#define TEMP_B 13.725

#define HUMI_A 0.11147
#define HUMI_B 64.406

FirebaseData fbdo;
FirebaseAuth fbau;
FirebaseConfig fbco;

String uid;
String path_timestamp;
String path_readings;

void setup() {
  Serial.begin(115200);

  // WiFi Initialization
  Serial.print("Connecting to Wifi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println(".");
  }
  Serial.println();
  Serial.print("Connected to network with IP: ");
  Serial.println(WiFi.localIP());

  // Configure Time
  configTime(0, 0, "pool.ntp.org");

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  fbco.host = FIRE_HOST;
  fbco.api_key = FIRE_AUTH;
  fbau.user.email = USER_NAME;
  fbau.user.password = USER_PASS;
  Firebase.begin(&fbco, &fbau);
  Firebase.reconnectWiFi(true);
  Firebase.setMaxRetry(fbdo, 3);
  Firebase.setMaxErrorQueue(fbdo, 30);
  Serial.print("Getting UID");
  while (fbau.token.uid == "") {
    Serial.println(".");
  }
  Serial.println();
  uid = fbau.token.uid.c_str();
  path_timestamp = "/devices/" + uid + "/latest";
  path_readings = "/readings/" + uid;
  Serial.print("UID: ");
  Serial.println(uid);

  // Configure pins
  pinMode(CO_PIN, INPUT);
  pinMode(DUST_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(DUST_LED, OUTPUT);
  dht.begin();

  digitalWrite(DUST_LED, 1);
}

time_t now;


void loop() {

  digitalWrite(FAN_PIN, 1);

  delay(10000); // 10 secs to fan air in

  digitalWrite(FAN_PIN, 0);

  int coRead = analogRead(CO_PIN);
  float coVin = (coRead * 5.0 / 4095.0);
  float coRat = (5 - coVin) / coVin; // needed for all MQ-X
  float co = CO_A * pow(coRat, CO_B);

  int h4Read = analogRead(H4_PIN);
  float h4Vin = (h4Read * 5.0 / 4095.0);
  float h4Rat = (5 - h4Vin) / h4Vin; // needed for all MQ-X
  float h4 = H4_A * pow(h4Rat, H4_B);

  digitalWrite(DUST_LED, 0);
  delayMicroseconds(DUST_S);

  int dustRead = analogRead(DUST_PIN);
  float dust = DUST_A * ((dustRead * 5.0 / 4095.0)) - DUST_B;
  
  delayMicroseconds(DUST_D);
  digitalWrite(DUST_LED, 1);

  float temp = dht.readTemperature();
  float humi = dht.readHumidity();

  if(!isnan(temp))temp = TEMP_A*temp+TEMP_B;
  if(!isnan(humi))humi = HUMI_A*humi+HUMI_B;

  if(isnan(temp))temp=0;
  if(isnan(humi))humi=0;
  if(isnan(co))co=0;
  if(isnan(h4))h4=0;
  if(isnan(dust))dust=0;
  if(co<0)co=0;
  if(h4<0)h4=0;
  if(dust<0)dust=0;

  Serial.print("CO: ");
  Serial.print(co);

  Serial.print(" H4: ");
  Serial.print(h4);

  Serial.print(" Dust: ");
  Serial.print(dust);

  Serial.print(" Temp: ");
  Serial.print(temp);

  Serial.print(" Humi: ");
  Serial.println(humi);
  while(!Firebase.ready());
    time(&now);
    FirebaseJson fbjs;
    fbjs.add("timestamp", now);
    fbjs.add("co", co);
    fbjs.add("h4", h4);
    fbjs.add("temp", temp);
    fbjs.add("humi", humi);
    fbjs.add("particulate", dust);
    Firebase.pushJSON(fbdo, (path_readings).c_str(), fbjs);
    Serial.print(fbdo.errorReason().c_str());
    Firebase.setInt(fbdo, path_timestamp.c_str(), now);
  
  delay(50000); // 1min sampling rate
}
