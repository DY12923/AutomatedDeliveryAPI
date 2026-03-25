#include <Arduino.h>
#include <WiFi.h>
#include "esp_wpa2.h"

const char* ssid = "uwosecure-v2";
const char* username = "dyau23";
const char* password = "Darern02232007!";

void setup() {
    Serial.begin(115200);
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password, strlen(password));
    esp_wifi_sta_wpa2_ent_enable();
    
    WiFi.begin(ssid);
    
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("");
    Serial.println("WiFi connected!");
    Serial.println(WiFi.localIP());
}

void callPending(String student_id){


  
}

void loop() {
}