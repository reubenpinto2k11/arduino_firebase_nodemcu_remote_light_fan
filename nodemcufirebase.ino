#include "FirebaseESP8266.h"
#include<ESP8266WiFi.h>
#include <SimpleDHT.h>

// Set these to your WIFI settings
#define WIFI_SSID "wifi_ssid"
#define WIFI_PASSWORD "pass"

//Pin setup
#define OP_FAN1_ON_PIN D1
#define OP_LIGHT1_ON_PIN D2
#define OP_LIGHT2_ON_PIN D3


//Sensor Pin Setup
#define IP_LDR_PIN A0
#define IP_DHT_PIN D5
#define IP_MD_PIN D6

//Set these to the Firebase project settings
#define FIREBASE_URL "firebase_db_url"
#define FIREBASE_DB_SECRET "firebase_db_secret"

int lastMotionCounter = 0;
int NO_MOTION_THRESHOLD = 60;

int LDRReading;
int LDRThreshold = 50;
int LDRHighThreshold = 300;

SimpleDHT11 dht11(IP_DHT_PIN);

volatile byte state_auto = LOW;
volatile byte state_fan1 = LOW;
volatile byte state_light1 = LOW;
volatile byte state_light2 = LOW;

FirebaseData firebaseData;

void setup() {
  pinMode(OP_FAN1_ON_PIN, OUTPUT);
  pinMode(OP_LIGHT1_ON_PIN, OUTPUT);
  pinMode(OP_LIGHT2_ON_PIN, OUTPUT);

  pinMode(IP_DHT_PIN, INPUT);
  pinMode(IP_MD_PIN, INPUT);
    
  Serial.begin(115200);
  // connect to wifi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
    

  //begin Firebase
  Firebase.begin(FIREBASE_URL, FIREBASE_DB_SECRET);
  Firebase.reconnectWiFi(true);
  Firebase.setMaxRetry(firebaseData, 3);
  Firebase.setMaxErrorQueue(firebaseData, 30);
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  Firebase.setwriteSizeLimit(firebaseData, "tiny");
}

void loop() {
    //get the event
    if (Firebase.getBool(firebaseData, "/isAuto")) {
      if (firebaseData.dataType() == "boolean") {
        Serial.println("isAuto");
        Serial.println(firebaseData.boolData() == 1 ? "true" : "false");
        if (firebaseData.boolData() == 1) {
          //Automatic state
          if (state_auto == LOW) {
            state_auto = HIGH;
            state_fan1 = LOW;
            state_light1 = LOW;
            state_light2 = LOW;
            digitalWrite(OP_FAN1_ON_PIN, LOW);
            digitalWrite(OP_LIGHT1_ON_PIN, LOW);
            digitalWrite(OP_LIGHT2_ON_PIN, LOW);
            lastMotionCounter = NO_MOTION_THRESHOLD;
            Firebase.setInt(firebaseData, "/temperature", 0);
            Firebase.setInt(firebaseData, "/humidity", 0);
            Firebase.setInt(firebaseData, "/lumens", 0);
            Firebase.setBool(firebaseData, "/isFan1On", false);
            Firebase.setBool(firebaseData, "/isLight1On", false);
            Firebase.setBool(firebaseData, "/isLight2On", false);            
          }

          int hasMotionBeenDetected = digitalRead(IP_MD_PIN);
          Serial.println("Motion detection"); Serial.println(hasMotionBeenDetected);
          // if motion is present reset counter else increment count
          if (hasMotionBeenDetected == HIGH) {
            lastMotionCounter = 0;
          }
          else {
            lastMotionCounter += 1;
          }

          //check count value against threshold (1mins), switch off if no motion for 5 mins
          if (lastMotionCounter < NO_MOTION_THRESHOLD) {
            //check temprature if beyond above threshold and start fan
            byte temperature = 0;
            byte humidity = 0;
            int err = SimpleDHTErrSuccess;
            if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
              Serial.println("Read DHT11 failed, err="); Serial.println(err);        
            }
            else {
              Firebase.setInt(firebaseData, "/temperature", (int)temperature);
              Firebase.setInt(firebaseData, "/humidity", (int)humidity);
              if (((int)temperature > 25) && (state_fan1 == LOW)) {
               digitalWrite(OP_FAN1_ON_PIN, HIGH);
               Firebase.setBool(firebaseData, "/isFan1On", true);
               state_fan1 = HIGH; 
              }

              if (((int)temperature <= 16) && (state_fan1 == HIGH)) {
               digitalWrite(OP_FAN1_ON_PIN, LOW);
               Firebase.setBool(firebaseData, "/isFan1On", false);
               state_fan1 = LOW; 
              }
            }
      
            //check light intensity if above resistive threshold turn on light
            LDRReading = analogRead(IP_LDR_PIN);
            Firebase.setInt(firebaseData, "/lumens", (int)LDRReading);
            Serial.println("LDR reading"); Serial.println(LDRReading);
            if (LDRReading < LDRThreshold) {
              if (state_light1 == LOW) {            
                digitalWrite(OP_LIGHT1_ON_PIN, HIGH);
                Firebase.setBool(firebaseData, "/isLight1On", true);
                state_light1 = HIGH;
              }
              if (state_light2 == LOW) {            
                digitalWrite(OP_LIGHT2_ON_PIN, HIGH);
                Firebase.setBool(firebaseData, "/isLight2On", true);
                state_light2 = HIGH;
              }
            }

            if (LDRReading >= LDRHighThreshold) {
              if (state_light1 == HIGH) {            
                digitalWrite(OP_LIGHT1_ON_PIN, LOW);
                Firebase.setBool(firebaseData, "/isLight1On", false);
                state_light1 = LOW;
              }
              if (state_light2 == HIGH) {            
                digitalWrite(OP_LIGHT2_ON_PIN, LOW);
                Firebase.setBool(firebaseData, "/isLight2On", false);
                state_light2 = LOW;
              }
            }
          }
          else {
            if (state_fan1 == HIGH) {
              digitalWrite(OP_FAN1_ON_PIN, LOW);
              Firebase.setBool(firebaseData, "/isFan1On", false);
              state_fan1 = LOW;
            }
            if (state_light1 == HIGH) {
              digitalWrite(OP_LIGHT1_ON_PIN, LOW);
              Firebase.setBool(firebaseData, "/isLight1On", false);
              state_light1 = LOW;
            }
            if (state_light2 == HIGH) {
              digitalWrite(OP_LIGHT2_ON_PIN, LOW);
              Firebase.setBool(firebaseData, "/isLight2On", false);
              state_light2 = LOW;
            }
          }
        }
        else {
          // Manual State
          if (state_auto == HIGH) {
            state_auto = LOW;            
          }

          if (Firebase.getBool(firebaseData, "/isFan1On")) {
            if (firebaseData.dataType() == "boolean") {
              Serial.println("isFanOn");
              Serial.println(firebaseData.boolData() == 1 ? "true" : "false");
              digitalWrite(OP_FAN1_ON_PIN, (firebaseData.boolData() == 1 ? HIGH : LOW));
            }
          }

          if (Firebase.getBool(firebaseData, "/isLight1On")) {
            if (firebaseData.dataType() == "boolean") {
              Serial.println("isLight1On");
              Serial.println(firebaseData.boolData() == 1 ? "true" : "false");
              digitalWrite(OP_LIGHT1_ON_PIN, (firebaseData.boolData() == 1 ? HIGH : LOW));
            }            
          }
          if (Firebase.getBool(firebaseData, "/isLight2On")) {
            if (firebaseData.dataType() == "boolean") {
              Serial.println("isLight2On");
              Serial.println(firebaseData.boolData() == 1 ? "true" : "false");
              digitalWrite(OP_LIGHT2_ON_PIN, (firebaseData.boolData() == 1 ? HIGH : LOW));
            }            
          }
        }
      }
    }
    delay(500);
}
