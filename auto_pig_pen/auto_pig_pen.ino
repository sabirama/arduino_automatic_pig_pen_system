#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <FS.h>
#include <EEPROM.h>

#include "PinControl.h"
#include "ServoControl.h"
#include "ScaleModule.h"
#include "SMSModule.h"
#include "WiFiConfig.h"

#define SERVO_PIN D5
#define SCALE_DOUT_PIN D6
#define SCALE_SCK_PIN D7
#define PUMP_PIN D8
#define GSM_RX_PIN D1
#define GSM_TX_PIN D2

#define EEPROM_SIZE 512
#define WIFI_EEPROM_START 0
#define WIFI_EEPROM_SIZE 96
#define FEED_EEPROM_START 100
#define FEED_EEPROM_SIZE 60  // 10 items * 6 bytes each
#define WASH_EEPROM_START 160
#define WASH_EEPROM_SIZE 60  // 10 items * 6 bytes each

WiFiConfig wifiConfig("PetFeederAP", "12345678", WIFI_EEPROM_START, WIFI_EEPROM_SIZE);
ESP8266WebServer server(80);

ServoControl servo(SERVO_PIN);
ScaleModule scale(SCALE_DOUT_PIN, SCALE_SCK_PIN);
PinControl relayPump(PUMP_PIN);
SMSModule smsModule(GSM_RX_PIN, GSM_TX_PIN);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 8 * 3600, 60000);

bool feedExecutedToday[10] = { false };
bool washExecutedToday[10] = { false };
unsigned long lastTimeUpdate = 0;
unsigned long lastNTPUpdate = 0;

// Feed and wash schedule storage
String feedSchedules[10];
String washSchedules[10];
uint8_t feedCount = 0;
uint8_t washCount = 0;

String getCurrentTimeString() {
  if (wifiConfig.isConnected() && !wifiConfig.isAPModeActive()) {
    if (millis() - lastNTPUpdate > 3600000) {
      timeClient.forceUpdate();
      lastNTPUpdate = millis();
    }
    timeClient.update();
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", timeClient.getHours(), timeClient.getMinutes());
    return String(buf);
  }
  return "00:00";
}

bool serveFile(const char* filepath, const char* contentType) {
  if (SPIFFS.exists(filepath)) {
    File file = SPIFFS.open(filepath, "r");
    if (file) {
      server.streamFile(file, contentType);
      file.close();
      return true;
    }
  }
  return false;
}

void handleRoot() {
  if (!serveFile("/index.html", "text/html")) {
    server.send(404, "text/plain", "Index file not found");
  }
}

void handleCSS() {
  if (!serveFile("/style.css", "text/css")) {
    server.send(404, "text/plain", "CSS file not found");
  }
}

void handleJS() {
  if (!serveFile("/script.js", "application/javascript")) {
    server.send(404, "text/plain", "JavaScript file not found");
  }
}

void handleFeed() {
  servo.open();
  delay(3000);
  servo.close();
  server.send(200, "text/plain", "Feed operation completed");
}

void handleWash() {
  relayPump.setHigh();
  delay(5000);
  relayPump.setLow();
  server.send(200, "text/plain", "Wash operation completed");
}

void handleTare() {
  scale.tare();
  server.send(200, "text/plain", "Scale tared");
}

// Save schedules to EEPROM
void saveFeedSchedules() {
  int addr = FEED_EEPROM_START;
  EEPROM.write(addr++, feedCount);  // Store count first
  
  for (uint8_t i = 0; i < feedCount; i++) {
    const char* timeStr = feedSchedules[i].c_str();
    for (uint8_t j = 0; j < 5; j++) {
      EEPROM.write(addr++, timeStr[j]);
    }
    EEPROM.write(addr++, 0);  // Null terminator
  }
  EEPROM.commit();
}

void saveWashSchedules() {
  int addr = WASH_EEPROM_START;
  EEPROM.write(addr++, washCount);  // Store count first
  
  for (uint8_t i = 0; i < washCount; i++) {
    const char* timeStr = washSchedules[i].c_str();
    for (uint8_t j = 0; j < 5; j++) {
      EEPROM.write(addr++, timeStr[j]);
    }
    EEPROM.write(addr++, 0);  // Null terminator
  }
  EEPROM.commit();
}

// Load schedules from EEPROM
void loadFeedSchedules() {
  int addr = FEED_EEPROM_START;
  feedCount = EEPROM.read(addr++);
  
  for (uint8_t i = 0; i < feedCount && i < 10; i++) {
    char timeBuf[6] = {0};
    for (uint8_t j = 0; j < 5; j++) {
      timeBuf[j] = EEPROM.read(addr++);
    }
    addr++;  // Skip null terminator
    feedSchedules[i] = String(timeBuf);
  }
}

void loadWashSchedules() {
  int addr = WASH_EEPROM_START;
  washCount = EEPROM.read(addr++);
  
  for (uint8_t i = 0; i < washCount && i < 10; i++) {
    char timeBuf[6] = {0};
    for (uint8_t j = 0; j < 5; j++) {
      timeBuf[j] = EEPROM.read(addr++);
    }
    addr++;  // Skip null terminator
    washSchedules[i] = String(timeBuf);
  }
}

bool addFeedTime(const String& timeStr) {
  if (feedCount >= 10) return false;
  
  // Check if time already exists
  for (uint8_t i = 0; i < feedCount; i++) {
    if (feedSchedules[i] == timeStr) return false;
  }
  
  feedSchedules[feedCount++] = timeStr;
  saveFeedSchedules();
  return true;
}

bool removeFeedTime(const String& timeStr) {
  for (uint8_t i = 0; i < feedCount; i++) {
    if (feedSchedules[i] == timeStr) {
      // Shift remaining items
      for (uint8_t j = i; j < feedCount - 1; j++) {
        feedSchedules[j] = feedSchedules[j + 1];
      }
      feedCount--;
      saveFeedSchedules();
      return true;
    }
  }
  return false;
}

bool addWashTime(const String& timeStr) {
  if (washCount >= 10) return false;
  
  // Check if time already exists
  for (uint8_t i = 0; i < washCount; i++) {
    if (washSchedules[i] == timeStr) return false;
  }
  
  washSchedules[washCount++] = timeStr;
  saveWashSchedules();
  return true;
}

bool removeWashTime(const String& timeStr) {
  for (uint8_t i = 0; i < washCount; i++) {
    if (washSchedules[i] == timeStr) {
      // Shift remaining items
      for (uint8_t j = i; j < washCount - 1; j++) {
        washSchedules[j] = washSchedules[j + 1];
      }
      washCount--;
      saveWashSchedules();
      return true;
    }
  }
  return false;
}

void handleAddFeed() {
  if (server.hasArg("time")) {
    String timeStr = server.arg("time");
    if (timeStr.length() == 5 && timeStr[2] == ':') {
      if (addFeedTime(timeStr)) {
        server.send(200, "text/plain", "Feed time added: " + timeStr);
      } else {
        server.send(400, "text/plain", "Failed to add feed time");
      }
    } else {
      server.send(400, "text/plain", "Invalid time format");
    }
  } else {
    server.send(400, "text/plain", "Missing time parameter");
  }
}

void handleRemoveFeed() {
  if (server.hasArg("time")) {
    String timeStr = server.arg("time");
    if (removeFeedTime(timeStr)) {
      server.send(200, "text/plain", "Feed time removed: " + timeStr);
    } else {
      server.send(400, "text/plain", "Failed to remove feed time");
    }
  } else {
    server.send(400, "text/plain", "Missing time parameter");
  }
}

void handleAddWash() {
  if (server.hasArg("time")) {
    String timeStr = server.arg("time");
    if (timeStr.length() == 5 && timeStr[2] == ':') {
      if (addWashTime(timeStr)) {
        server.send(200, "text/plain", "Wash time added: " + timeStr);
      } else {
        server.send(400, "text/plain", "Failed to add wash time");
      }
    } else {
      server.send(400, "text/plain", "Invalid time format");
    }
  } else {
    server.send(400, "text/plain", "Missing time parameter");
  }
}

void handleRemoveWash() {
  if (server.hasArg("time")) {
    String timeStr = server.arg("time");
    if (removeWashTime(timeStr)) {
      server.send(200, "text/plain", "Wash time removed: " + timeStr);
    } else {
      server.send(400, "text/plain", "Failed to remove wash time");
    }
  } else {
    server.send(400, "text/plain", "Missing time parameter");
  }
}

void handleGetFeedTimes() {
  String json = "[";
  for (uint8_t i = 0; i < feedCount; i++) {
    json += "\"" + feedSchedules[i] + "\"";
    if (i < feedCount - 1) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleGetWashTimes() {
  String json = "[";
  for (uint8_t i = 0; i < washCount; i++) {
    json += "\"" + washSchedules[i] + "\"";
    if (i < washCount - 1) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleSetPhone() {
  if (server.hasArg("number")) {
    String number = server.arg("number");
    smsModule.setStoredPhoneNumber(number.c_str());
    server.send(200, "text/plain", "Phone number set to: " + number);
  } else {
    server.send(400, "text/plain", "Missing number parameter");
  }
}

void handleSendSMS() {
  String message = server.hasArg("message") ? server.arg("message") : "Alert from pet feeder";
  smsModule.sendSMS(message);
  server.send(200, "text/plain", "SMS sent: " + message);
}

void handleStatus() {
  String status = "System Status:\n";
  status += "WiFi: " + String(wifiConfig.isConnected() ? "Connected" : "Disconnected") + "\n";
  status += "IP: " + wifiConfig.getIP().toString() + "\n";
  status += "AP Mode: " + String(wifiConfig.isAPModeActive() ? "Active" : "Inactive") + "\n";
  status += "Scale reading: " + String(scale.read()) + "\n";
  status += "Current Time: " + getCurrentTimeString() + "\n";
  
  // FEED SCHEDULES - SHOW THEM FUCKING CLEARLY
  status += "\nFEED SCHEDULES (" + String(feedCount) + "):\n";
  if (feedCount > 0) {
    for (uint8_t i = 0; i < feedCount; i++) {
      status += "  " + feedSchedules[i] + "\n";
    }
  } else {
    status += "  No feed times scheduled\n";
  }
  
  // WASH SCHEDULES - SHOW THEM FUCKING CLEARLY  
  status += "\nWASH SCHEDULES (" + String(washCount) + "):\n";
  if (washCount > 0) {
    for (uint8_t i = 0; i < washCount; i++) {
      status += "  " + washSchedules[i] + "\n";
    }
  } else {
    status += "  No wash times scheduled\n";
  }
  
  status += "\nPhone: " + String(smsModule.getStoredPhoneNumber()) + "\n";
  
  server.send(200, "text/plain", status);
}

void setupServer() {
  server.on("/", handleRoot);
  server.on("/style.css", handleCSS);
  server.on("/script.js", handleJS);
  
  server.on("/feed", HTTP_POST, handleFeed);
  server.on("/wash", HTTP_POST, handleWash);
  server.on("/tare", HTTP_POST, handleTare);
  server.on("/addFeed", HTTP_POST, handleAddFeed);
  server.on("/removeFeed", HTTP_POST, handleRemoveFeed);
  server.on("/addWash", HTTP_POST, handleAddWash);
  server.on("/removeWash", HTTP_POST, handleRemoveWash);
  server.on("/getFeedTimes", HTTP_GET, handleGetFeedTimes);
  server.on("/getWashTimes", HTTP_GET, handleGetWashTimes);
  server.on("/setPhone", HTTP_POST, handleSetPhone);
  server.on("/sendSMS", HTTP_POST, handleSendSMS);
  server.on("/status", HTTP_GET, handleStatus);
  
  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  EEPROM.begin(EEPROM_SIZE);
  SPIFFS.begin();

  servo.begin();
  relayPump.begin();
  scale.begin();
  smsModule.begin();

  wifiConfig.begin();
  
  // Load schedules from EEPROM
  loadFeedSchedules();
  loadWashSchedules();

  setupServer();

  if (wifiConfig.isConnected()) {
    timeClient.begin();
    delay(2000);
    timeClient.forceUpdate();
    lastNTPUpdate = millis();
  }
  
  Serial.println("Pet Feeder Started!");
  Serial.println("Feed Schedules:");
  for (uint8_t i = 0; i < feedCount; i++) {
    Serial.println("  " + feedSchedules[i]);
  }
  Serial.println("Wash Schedules:");
  for (uint8_t i = 0; i < washCount; i++) {
    Serial.println("  " + washSchedules[i]);
  }
}

void handleScheduledEvents() {
  if (!wifiConfig.isConnected() || wifiConfig.isAPModeActive()) {
    return;
  }

  if (millis() - lastTimeUpdate > 60000) {
    String nowStr = getCurrentTimeString();
    lastTimeUpdate = millis();
    
    if (nowStr == "00:00") return;

    for (uint8_t i = 0; i < feedCount && i < 10; i++) {
      if (!feedExecutedToday[i] && feedSchedules[i] == nowStr) {
        servo.open();
        delay(3000);
        servo.close();
        feedExecutedToday[i] = true;
        smsModule.sendSMS("Pet feeder: Feed operation completed at " + nowStr);
      } else if (feedSchedules[i] != nowStr) {
        feedExecutedToday[i] = false;
      }
    }

    for (uint8_t i = 0; i < washCount && i < 10; i++) {
      if (!washExecutedToday[i] && washSchedules[i] == nowStr) {
        relayPump.setHigh();
        delay(5000);
        relayPump.setLow();
        washExecutedToday[i] = true;
        smsModule.sendSMS("Pet feeder: Wash operation completed at " + nowStr);
      } else if (washSchedules[i] != nowStr) {
        washExecutedToday[i] = false;
      }
    }
  }
}

void loop() {
  server.handleClient();
  wifiConfig.handleClient();
  
  if (wifiConfig.isConnected() && !wifiConfig.isAPModeActive()) {
    handleScheduledEvents();
  }
  
  delay(1000);
}
