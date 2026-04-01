#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_wpa2.h"
#include <ESP32Servo.h>
#include <SPI.h>
#include <MFRC522.h>

const char* ssid = "uwosecure-v2";
const char* username = "dyau23@uwo.ca";
const char* password = "YOUR_PASSWORD";
const char* apiBase = "http://YOUR_LAPTOP_IP:5000";

Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

const int SERVO1_PIN = 13;
const int SERVO2_PIN = 14;
const int SERVO3_PIN = 15;
const int SERVO4_PIN = 25;

const int RST_PIN = 22;
const int SS_PIN = 21;
MFRC522 rfid(SS_PIN, RST_PIN);

const int GREEN_LED = 26;
const int RED_LED = 27;

void setup() {
    Serial.begin(115200);

    pinMode(GREEN_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password, strlen(password));
    esp_wifi_sta_wpa2_ent_enable();
    
    WiFi.begin(ssid);
    
    Serial.println("Connecting to WiFi...");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi failed! Restarting...");
        ESP.restart();
    }
    
    Serial.println("WiFi connected!");
    Serial.println(WiFi.localIP());

    servo1.attach(SERVO1_PIN);
    servo2.attach(SERVO2_PIN);
    servo3.attach(SERVO3_PIN);
    servo4.attach(SERVO4_PIN);

    SPI.begin(18, 19, 13, 21);
    rfid.PCD_Init();
    Serial.println("RFID ready - scan a card!");
}

String readRFID() {
    if (!rfid.PICC_IsNewCardPresent()) return "";
    if (!rfid.PICC_ReadCardSerial()) return "";
    
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(rfid.uid.uidByte[i], HEX);
    }
    
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    
    uid.toUpperCase();
    Serial.println("Card scanned: " + uid);
    return uid;
}

int callPending(String student_id) {
    HTTPClient http;
    String url = String(apiBase) + "/pending?student_id=" + student_id;
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        Serial.println("Response: " + payload);
        
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.println("JSON parse failed!");
            http.end();
            return 0;
        }
        
        int box_number = doc["box_number"];
        http.end();
        return box_number;
    }
    
    http.end();
    return 0;
}

bool callClaim(String student_id, int box_number) {
    HTTPClient http;
    String url = String(apiBase) + "/claim";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    String body = "{\"student_id\":\"" + student_id + "\",\"box_number\":" + box_number + "}";
    int httpCode = http.POST(body);
    http.end();

    if (httpCode == 200) {
        Serial.println("Claimed box " + String(box_number) + " for " + student_id);
        return true;
    }

    Serial.println("Claim failed! HTTP code: " + String(httpCode));
    return false;
}

void openBox(int box_number) {
    Servo* target = nullptr;
    
    if (box_number == 1) target = &servo1;
    if (box_number == 2) target = &servo2;
    if (box_number == 3) target = &servo3;
    if (box_number == 4) target = &servo4;
    
    if (target == nullptr) {
        Serial.println("Invalid box number!");
        return;
    }
    
    Serial.println("Opening box " + String(box_number));
    target->write(90);
    delay(5000);
    target->write(0);
    Serial.println("Box " + String(box_number) + " closed");
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        String uid = readRFID();
        
        if (uid != "") {
            int box_number = callPending(uid);
            
            if (box_number > 0) {
                bool claimed = callClaim(uid, box_number);
                if (claimed) {
                    digitalWrite(GREEN_LED, HIGH);
                    openBox(box_number);
                    digitalWrite(GREEN_LED, LOW);
                } else {
                    digitalWrite(RED_LED, HIGH);
                    delay(2000);
                    digitalWrite(RED_LED, LOW);
                }
            } else {
                Serial.println("No delivery found for this card");
                digitalWrite(RED_LED, HIGH);
                delay(2000);
                digitalWrite(RED_LED, LOW);
            }
        }
    }
    delay(500);
}