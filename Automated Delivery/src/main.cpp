#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_wpa2.h"
#include <ESP32Servo.h>
#include <SPI.h>
#include <MFRC522.h>

// Campus WPA2-Enterprise credentials and backend API base URL.
const char* ssid = "uwosecure-v2";
const char* username = "dyau23@uwo.ca";
const char* password = "Darren02232007!";
const char* apiBase = "http://172.30.54.75:5000";

// One servo per physical locker box.
Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

// GPIO mapping for servo outputs.
const int SERVO1_PIN = 14;
const int SERVO2_PIN = 32;
const int SERVO3_PIN = 15;
const int SERVO4_PIN = 25;

// RFID reader pins (RC522).
const int RST_PIN = 22;
const int SS_PIN = 21;
MFRC522 rfid(SS_PIN, RST_PIN);
 
// Status LEDs for success/failure feedback.
const int GREEN_LED = 26;
const int RED_LED = 27;

void setup() {
    // Initialize serial logging for troubleshooting.
    Serial.begin(115200);

    // Prepare status LEDs.
    pinMode(GREEN_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    
    // Configure station mode and WPA2-Enterprise auth values.
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password, strlen(password));
    esp_wifi_sta_wpa2_ent_enable();
    
    WiFi.begin(ssid);
    
    // Wait for Wi-Fi connect with timeout then reboot to retry.
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

    // Attach all servo outputs.
    servo1.attach(SERVO1_PIN);
    servo2.attach(SERVO2_PIN);
    servo3.attach(SERVO3_PIN);
    servo4.attach(SERVO4_PIN);

    // Initialize SPI bus and RFID reader.
    SPI.begin(18, 19, 13, 21);
    rfid.PCD_Init();
    Serial.println("RFID ready - scan a card!");
}

String readRFID() {
    // Return empty string when no card is present/readable.
    if (!rfid.PICC_IsNewCardPresent()) return "";
    if (!rfid.PICC_ReadCardSerial()) return "";
    
    // Convert UID bytes to uppercase hex string.
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(rfid.uid.uidByte[i], HEX);
    }
    
    // End RFID transaction cleanly before returning.
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    
    uid.toUpperCase();
    Serial.println("Card scanned: " + uid);
    return uid;
}

int callPending(String student_id) {
    // Ask backend for the student's pending locker box number.
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
    // Confirm claim so backend marks delivery complete and frees box.
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
    // Route box number to its corresponding servo.
    Servo* target = nullptr;
    
    if (box_number == 1) target = &servo1;
    if (box_number == 2) target = &servo2;
    if (box_number == 3) target = &servo3;
    if (box_number == 4) target = &servo4;
    
    if (target == nullptr) {
        Serial.println("Invalid box number!");
        return;
    }
    
    // Open briefly, then close automatically.
    Serial.println("Opening box " + String(box_number));
    target->write(90);
    delay(5000);
    target->write(0);
    Serial.println("Box " + String(box_number) + " closed");
}

void checkRegistration() {
    // Poll backend for an armed registration request.
    HTTPClient http;
    String url = String(apiBase) + "/register/pending";
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) { http.end(); return; }
        
        const char* student_id = doc["student_id"];
        const char* name = doc["name"];
        
        if (student_id == nullptr) { http.end(); return; }
        
        http.end();
        
        // Prompt operator to scan card for the pending student.
        Serial.println("Registration pending for: " + String(student_id));
        Serial.println("Waiting for card scan...");
        
        String uid = readRFID();
        if (uid == "") return;
        
        HTTPClient http2;
        http2.begin(String(apiBase) + "/register");
        http2.addHeader("Content-Type", "application/json");
        
        String body = "{\"uid\":\"" + uid + "\",\"student_id\":\"" + String(student_id) + "\",\"name\":\"" + String(name) + "\"}";
        int code = http2.POST(body);
        http2.end();
        
        if (code == 200) {
            // Green LED indicates successful registration write.
            Serial.println("Registration successful!");
            digitalWrite(GREEN_LED, HIGH);
            delay(2000);
            digitalWrite(GREEN_LED, LOW);
        } else {
            // Red LED indicates backend failure.
            Serial.println("Registration failed!");
            digitalWrite(RED_LED, HIGH);
            delay(2000);
            digitalWrite(RED_LED, LOW);
        }
    }
    http.end();
}

void loop() {
    // Main loop handles registration polling and card-based pickup flow.
    if (WiFi.status() == WL_CONNECTED) {
        checkRegistration();
        
        String uid = readRFID();
        
        if (uid != "") {
            // Resolve scanned UID to student identity.
            HTTPClient http;
            String url = String(apiBase) + "/lookup?uid=" + uid;
            http.begin(url);
            int httpCode = http.GET();
            
            if (httpCode == 200) {
                String payload = http.getString();
                StaticJsonDocument<512> doc;
                deserializeJson(doc, payload);
                
                const char* student_id = doc["student_id"];
                
                if (student_id == nullptr) {
                    // Card exists physically but is unknown to registration table.
                    Serial.println("Card not registered!");
                    digitalWrite(RED_LED, HIGH);
                    delay(2000);
                    digitalWrite(RED_LED, LOW);
                } else {
                    // Check if this student has a pending delivery.
                    int box_number = callPending(String(student_id));
                    
                    if (box_number > 0) {
                        // Mark claim first, then actuate locker.
                        bool claimed = callClaim(String(student_id), box_number);
                        if (claimed) {
                            digitalWrite(GREEN_LED, HIGH);
                            openBox(box_number);
                            digitalWrite(GREEN_LED, LOW);
                        } else {
                            // Claim failed, do not open locker.
                            digitalWrite(RED_LED, HIGH);
                            delay(2000);
                            digitalWrite(RED_LED, LOW);
                        }
                    } else {
                        // No active pending delivery for this card.
                        Serial.println("No delivery found for this card");
                        digitalWrite(RED_LED, HIGH);
                        delay(2000);
                        digitalWrite(RED_LED, LOW);
                    }
                }
            }
            http.end();
        }
    }
    delay(500);
}