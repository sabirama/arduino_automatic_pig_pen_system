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

#define SERVO_PIN D4
#define SCALE_DOUT_PIN D2
#define SCALE_SCK_PIN D3
#define PUMP_PIN D5
#define GSM_RX_PIN D6
#define GSM_TX_PIN D7

#define EEPROM_SIZE 512
#define WIFI_EEPROM_START 0
#define WIFI_EEPROM_SIZE 96
#define FEED_EEPROM_START 100
#define FEED_EEPROM_SIZE 60  // 10 items * 6 bytes each
#define WASH_EEPROM_START 160
#define WASH_EEPROM_SIZE 60  // 10 items * 6 bytes each
#define WEIGHT_DROP_EEPROM_START 220
#define WEIGHT_DROP_EEPROM_SIZE 4  // float size
#define WASH_DURATION_EEPROM_START 224
#define WASH_DURATION_EEPROM_SIZE 4  // unsigned long size

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

// Weight drop tracking variables
float targetWeightDrop = 0.0;  // grams
float initialWeight = 0.0;
bool isMonitoringWeightDrop = false;
unsigned long weightMonitoringStartTime = 0;
const unsigned long WEIGHT_MONITOR_TIMEOUT = 300000; // 5 minutes timeout

// Wash timer variables
unsigned long washDuration = 5000;  // Default 5 seconds
unsigned long washStartTime = 0;
bool isWashActive = false;

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

// EEPROM functions for weight drop setting
void saveWeightDropSetting() {
  EEPROM.put(WEIGHT_DROP_EEPROM_START, targetWeightDrop);
  EEPROM.commit();
  Serial.println("Weight drop saved to EEPROM: " + String(targetWeightDrop) + "g");
}

void loadWeightDropSetting() {
  EEPROM.get(WEIGHT_DROP_EEPROM_START, targetWeightDrop);
  // If EEPROM is uninitialized, set default to 0
  if (isnan(targetWeightDrop) || targetWeightDrop < 0 || targetWeightDrop > 10000) {
    targetWeightDrop = 0.0;
    saveWeightDropSetting(); // Save default
  }
  Serial.println("Weight drop loaded from EEPROM: " + String(targetWeightDrop) + "g");
}

// EEPROM functions for wash duration
void saveWashDuration() {
  EEPROM.put(WASH_DURATION_EEPROM_START, washDuration);
  EEPROM.commit();
  Serial.println("Wash duration saved to EEPROM: " + String(washDuration) + "ms");
}

void loadWashDuration() {
  EEPROM.get(WASH_DURATION_EEPROM_START, washDuration);
  // If EEPROM is uninitialized, set default to 5 seconds
  if (washDuration == 0 || washDuration > 3600000) { // Sanity check: max 1 hour
    washDuration = 5000;
    saveWashDuration(); // Save default
  }
  Serial.println("Wash duration loaded from EEPROM: " + String(washDuration) + "ms");
}

// Weight drop monitoring functions
void startWeightDropMonitoring() {
  initialWeight = scale.read();
  isMonitoringWeightDrop = true;
  weightMonitoringStartTime = millis();
  Serial.println("Weight drop monitoring started");
  Serial.print("Initial weight: ");
  Serial.println(initialWeight);
  Serial.print("Target drop: ");
  Serial.println(targetWeightDrop);
}

void stopWeightDropMonitoring() {
  isMonitoringWeightDrop = false;
  Serial.println("Weight drop monitoring stopped");
}

void checkWeightDrop() {
  if (!isMonitoringWeightDrop || targetWeightDrop <= 0) return;

  float currentWeight = scale.read();
  float weightDiff = initialWeight - currentWeight;

  Serial.print("Weight monitoring - Current: ");
  Serial.print(currentWeight);
  Serial.print("g, Drop: ");
  Serial.print(weightDiff);
  Serial.print("g, Target: ");
  Serial.println(targetWeightDrop);

  // Check if target drop is reached
  if (weightDiff >= targetWeightDrop) {
    String message = "Pet feeder: Food consumed! Weight drop detected: " +
                     String(weightDiff, 1) + "g (Target: " + String(targetWeightDrop, 1) + "g)";
    smsModule.sendSMS(message);
    stopWeightDropMonitoring();
    Serial.println("Target weight drop reached! SMS sent.");
  }

  // Check timeout (5 minutes)
  if (millis() - weightMonitoringStartTime > WEIGHT_MONITOR_TIMEOUT) {
    Serial.println("Weight monitoring timeout reached");
    stopWeightDropMonitoring();
  }
}

// Wash timer functions
void startWashTimer() {
  relayPump.setHigh();
  isWashActive = true;
  washStartTime = millis();
  Serial.println("Wash timer started");
  Serial.print("Duration: ");
  Serial.print(washDuration);
  Serial.println(" ms");
}

void checkWashTimer() {
  if (!isWashActive) return;

  if (millis() - washStartTime >= washDuration) {
    relayPump.setLow();
    isWashActive = false;
    Serial.println("Wash timer completed - pump turned off");

    // Send SMS notification for scheduled washes
    String nowStr = getCurrentTimeString();
    if (nowStr != "00:00") {
      smsModule.sendSMS("Pet feeder: Wash operation completed at " + nowStr +
                        " (Duration: " + String(washDuration / 1000) + "s)");
    }
  }
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

  // Start weight drop monitoring if target is set
  if (targetWeightDrop > 0) {
    startWeightDropMonitoring();
  }

  delay(300); // Shorter delay
  servo.close();

  // Fixed string concatenation
  String response = "Feed operation completed";
  if (targetWeightDrop > 0) {
    response += " (Weight drop monitoring active)";
  }
  server.send(200, "text/plain", response);
}

void handleWash() {
  if (isWashActive) {
    server.send(200, "text/plain", "Wash operation is already in progress");
    return;
  }

  startWashTimer();
  String response = "Wash operation started for " + String(washDuration / 1000) + " seconds";
  server.send(200, "text/plain", response);
}

void handleSetWashDuration() {
  if (server.hasArg("duration")) {
    String durationStr = server.arg("duration");
    unsigned long newDuration = durationStr.toInt() * 1000; // Convert seconds to milliseconds

    if (newDuration > 0 && newDuration <= 3600000) { // Max 1 hour
      washDuration = newDuration;
      saveWashDuration(); // Save to EEPROM
      server.send(200, "text/plain", "Wash duration set to: " + String(washDuration / 1000) + " seconds");
    } else {
      server.send(400, "text/plain", "Invalid duration (1-3600 seconds)");
    }
  } else {
    server.send(400, "text/plain", "Missing duration parameter");
  }
}

void handleGetWashDuration() {
  String json = "{\"washDuration\":" + String(washDuration / 1000) + ",";
  json += "\"isWashActive\":" + String(isWashActive ? "true" : "false") + ",";
  if (isWashActive) {
    unsigned long elapsed = millis() - washStartTime;
    unsigned long remaining = (washDuration > elapsed) ? (washDuration - elapsed) : 0;
    json += "\"elapsedTime\":" + String(elapsed / 1000) + ",";
    json += "\"remainingTime\":" + String(remaining / 1000);
  } else {
    json += "\"elapsedTime\":0,\"remainingTime\":0";
  }
  json += "}";
  server.send(200, "application/json", json);
}

void handleStopWash() {
  if (isWashActive) {
    relayPump.setLow();
    isWashActive = false;
    server.send(200, "text/plain", "Wash operation stopped manually");
  } else {
    server.send(200, "text/plain", "No wash operation in progress");
  }
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
  if (feedCount > 10) feedCount = 0; // Sanity check

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
  if (washCount > 10) washCount = 0; // Sanity check

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

void handleSetWeightDrop() {
  if (server.hasArg("weight")) {
    String weightStr = server.arg("weight");
    float newWeight = weightStr.toFloat();

    if (newWeight >= 0) {
      targetWeightDrop = newWeight;
      saveWeightDropSetting(); // Save to EEPROM
      server.send(200, "text/plain", "Weight drop target set to: " + String(targetWeightDrop, 1) + "g");
    } else {
      server.send(400, "text/plain", "Invalid weight value");
    }
  } else {
    server.send(400, "text/plain", "Missing weight parameter");
  }
}

void handleGetWeightDrop() {
  String json = "{\"targetWeightDrop\":" + String(targetWeightDrop, 1) + ",";
  json += "\"isMonitoring\":" + String(isMonitoringWeightDrop ? "true" : "false") + ",";
  json += "\"currentWeight\":" + String(scale.read(), 1) + "}";
  server.send(200, "application/json", json);
}

void handleStopMonitoring() {
  stopWeightDropMonitoring();
  server.send(200, "text/plain", "Weight drop monitoring stopped");
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
  status += "Scale reading: " + String(scale.read(), 1) + "g\n";
  status += "Current Time: " + getCurrentTimeString() + "\n";

  // Weight drop monitoring status
  status += "\nWEIGHT DROP MONITORING:\n";
  status += "Target drop: " + String(targetWeightDrop, 1) + "g\n";
  status += "Status: " + String(isMonitoringWeightDrop ? "ACTIVE" : "Inactive") + "\n";
  if (isMonitoringWeightDrop) {
    status += "Initial weight: " + String(initialWeight, 1) + "g\n";
    status += "Current drop: " + String(initialWeight - scale.read(), 1) + "g\n";
  }

  // Wash timer status
  status += "\nWASH TIMER:\n";
  status += "Duration: " + String(washDuration / 1000) + " seconds\n";
  status += "Status: " + String(isWashActive ? "ACTIVE" : "Inactive") + "\n";
  if (isWashActive) {
    unsigned long elapsed = millis() - washStartTime;
    unsigned long remaining = (washDuration > elapsed) ? (washDuration - elapsed) : 0;
    status += "Elapsed: " + String(elapsed / 1000) + "s\n";
    status += "Remaining: " + String(remaining / 1000) + "s\n";
  }

  // FEED SCHEDULES
  status += "\nFEED SCHEDULES (" + String(feedCount) + "):\n";
  if (feedCount > 0) {
    for (uint8_t i = 0; i < feedCount; i++) {
      status += "  " + feedSchedules[i] + "\n";
    }
  } else {
    status += "  No feed times scheduled\n";
  }

  // WASH SCHEDULES
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
  server.on("/stopWash", HTTP_POST, handleStopWash);
  server.on("/setWashDuration", HTTP_POST, handleSetWashDuration);
  server.on("/getWashDuration", HTTP_GET, handleGetWashDuration);
  server.on("/tare", HTTP_POST, handleTare);
  server.on("/addFeed", HTTP_POST, handleAddFeed);
  server.on("/removeFeed", HTTP_POST, handleRemoveFeed);
  server.on("/addWash", HTTP_POST, handleAddWash);
  server.on("/removeWash", HTTP_POST, handleRemoveWash);
  server.on("/setWeightDrop", HTTP_POST, handleSetWeightDrop);
  server.on("/getWeightDrop", HTTP_GET, handleGetWeightDrop);
  server.on("/stopMonitoring", HTTP_POST, handleStopMonitoring);
  server.on("/getFeedTimes", HTTP_GET, handleGetFeedTimes);
  server.on("/getWashTimes", HTTP_GET, handleGetWashTimes);
  server.on("/setPhone", HTTP_POST, handleSetPhone);
  server.on("/sendSMS", HTTP_POST, handleSendSMS);
  server.on("/status", HTTP_GET, handleStatus);

  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(500); // Shorter initial delay

  EEPROM.begin(EEPROM_SIZE);
  SPIFFS.begin();

  servo.begin();
  relayPump.begin();
  scale.begin();
  smsModule.begin();

  wifiConfig.begin();

  // Load settings from EEPROM
  loadFeedSchedules();
  loadWashSchedules();
  loadWeightDropSetting();
  loadWashDuration();

  setupServer();

  if (wifiConfig.isConnected()) {
    timeClient.begin();
    delay(500); // Shorter delay
    timeClient.forceUpdate();
    lastNTPUpdate = millis();
  }

  Serial.println("Pet Feeder Started!");
  Serial.print("Weight drop target: ");
  Serial.print(targetWeightDrop);
  Serial.println("g");
  Serial.print("Wash duration: ");
  Serial.print(washDuration / 1000);
  Serial.println(" seconds");
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

    if (nowStr == "00:00") {
      // Reset daily flags at midnight
      for (uint8_t i = 0; i < 10; i++) {
        feedExecutedToday[i] = false;
        washExecutedToday[i] = false;
      }
      return;
    }

    for (uint8_t i = 0; i < feedCount && i < 10; i++) {
      if (!feedExecutedToday[i] && feedSchedules[i] == nowStr) {
        servo.open();

        // Start weight drop monitoring for scheduled feeds too
        if (targetWeightDrop > 0) {
          startWeightDropMonitoring();
        }

        delay(300); // Shorter delay
        servo.close();
        feedExecutedToday[i] = true;

        // Fixed string concatenation
        String smsMessage = "Pet feeder: Feed operation completed at " + nowStr;
        if (targetWeightDrop > 0) {
          smsMessage += " (Weight drop monitoring active)";
        }
        smsModule.sendSMS(smsMessage);
      }
    }

    for (uint8_t i = 0; i < washCount && i < 10; i++) {
      if (!washExecutedToday[i] && washSchedules[i] == nowStr && !isWashActive) {
        startWashTimer();
        washExecutedToday[i] = true;
      }
    }
  }
}

void loop() {
  server.handleClient();
  wifiConfig.handleClient();

  if (wifiConfig.isConnected() && !wifiConfig.isAPModeActive()) {
    handleScheduledEvents();

    // Check weight drop periodically
    if (isMonitoringWeightDrop) {
      checkWeightDrop();
    }

    // Check wash timer
    if (isWashActive) {
      checkWashTimer();
    }
  }

  delay(300); // Shorter loop delay
}
