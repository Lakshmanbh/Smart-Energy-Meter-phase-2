#define BLYNK_TEMPLATE_ID "TMPL3ayG2JFsS"
#define BLYNK_TEMPLATE_NAME "SMART ENERGY METER"
#define BLYNK_PRINT Serial

#include "EmonLib.h"
#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <base64.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD Setup
LiquidCrystal_I2C lcd(0x27, 20, 4);

// WiFi Credentials
const char auth[] = "QV08fZSuwm4IpTXyhMPzwHS51-tjsYq6";
const char ssid[] = "Oppo";
const char pass[] = "12345678";

// Twilio API Credentials
const char* TWILIO_ACCOUNT_SID = "ACa538db22824fa573f2f423464d752df6";
const char* TWILIO_AUTH_TOKEN = "12de51cf5d689ac1100e5c9f9aac4b3e";
const char* TWILIO_PHONE_NUMBER = "+16163845029";
const char* RECEIVER_PHONE_NUMBER = "+918431557044";

// Energy Monitor Setup
EnergyMonitor emon;
BlynkTimer timer;
float kWh = 0.0, bill = 0.0;
unsigned long lastMillis = millis();
const int addrKWh = 12;

void connectToWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.disconnect(true);
    delay(1000);
    WiFi.begin(ssid, pass);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n Connected to WiFi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n WiFi connection failed! Restarting ESP32...");
        ESP.restart();
    }
}

void setup() {
    Serial.begin(115200);
    connectToWiFi();
    Blynk.begin(auth, ssid, pass);
    lcd.init();
    lcd.backlight();
    EEPROM.begin(32);
    readEnergyDataFromEEPROM();
    emon.voltage(35, 41.5, 1.7);
    emon.current(34, 0.15);
    timer.setInterval(5000L, sendEnergyData); // Update every 5 sec
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectToWiFi();
    }
    Blynk.run();
    timer.run();
}

void sendEnergyData() {
    emon.calcVI(20, 2000);
    unsigned long currentMillis = millis();
    kWh += emon.apparentPower * (currentMillis - lastMillis) / 3600000000.0;
    lastMillis = currentMillis;
    
    calculateBilling();
    saveEnergyDataToEEPROM();
    updateLCD();
    sendToBlynk();
    sendSMSTwilio();
}

void sendToBlynk() {
    Blynk.virtualWrite(V0, emon.Vrms);
    Blynk.virtualWrite(V1, emon.Irms);
    Blynk.virtualWrite(V2, emon.apparentPower);
    Blynk.virtualWrite(V3, kWh);
    Blynk.virtualWrite(V4, bill);
    Serial.println(" Data sent to Blynk via WiFi.");
}

void sendSMSTwilio() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(" No WiFi! Skipping SMS.");
        return;
    }
    String message = "Energy Usage: " + String(kWh, 4) + " kWh, Bill: Rs. " + String(bill, 2) + "/-";
    String url = "https://api.twilio.com/2010-04-01/Accounts/" + String(TWILIO_ACCOUNT_SID) + "/Messages.json";
    
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    
    String authHeader = "Basic " + base64::encode(String(TWILIO_ACCOUNT_SID) + ":" + String(TWILIO_AUTH_TOKEN));
    http.addHeader("Authorization", authHeader);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    String postData = "To=" + String(RECEIVER_PHONE_NUMBER) + "&From=" + String(TWILIO_PHONE_NUMBER) + "&Body=" + message;
    int httpResponseCode = http.POST(postData);
    
    if (httpResponseCode > 0) {
        Serial.println(" SMS Sent via Twilio.");
    } else {
        Serial.println(" SMS Failed! Error: " + httpResponseCode);
    }
    http.end();
}

void updateLCD() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Vrms: ");
    lcd.print(emon.Vrms, 2);
    lcd.print(" V");
    lcd.setCursor(0, 1);
    lcd.print("Irms: ");
    lcd.print(emon.Irms, 4);
    lcd.print(" A");
    lcd.setCursor(0, 2);
    lcd.print("Power: ");
    lcd.print(emon.apparentPower, 4);
    lcd.print(" W");
    lcd.setCursor(0, 3);
    lcd.print("kWh: ");
    lcd.print(kWh, 5);
    lcd.print(" kWh");
}

void calculateBilling() {
    bill = 105;
    if (kWh <= 100) bill += 3.36 * kWh;
    else if (kWh <= 300) bill += 7.34 * kWh;
    else if (kWh <= 500) bill += 10.37 * kWh;
    else bill += 11.86 * kWh;
}

void readEnergyDataFromEEPROM() {
    EEPROM.get(addrKWh, kWh);
    if (isnan(kWh)) {
        kWh = 0.0;
        saveEnergyDataToEEPROM();
    }
}

void saveEnergyDataToEEPROM() {
    static float lastSavedkWh = -1;
    if (abs(kWh - lastSavedkWh) > 0.01) {
        EEPROM.put(addrKWh, kWh);
        EEPROM.commit();
        lastSavedkWh = kWh;
        Serial.println(" Energy data saved to EEPROM.");
    } else {
        Serial.println(" No significant energy change, skipping EEPROM write.");
    }
}
