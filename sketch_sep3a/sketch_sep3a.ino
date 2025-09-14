#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <Servo.h>
#include <HX711.h>
#include <DNSServer.h>

// EEPROM Addresses
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 50
#define FEED_SCHEDULE_ADDR 100
#define WASH_SCHEDULE_ADDR 120

char ssid[32];
char password[32];

// Web Server
ESP8266WebServer server(80);

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// DNS Server for Captive Portal
DNSServer dnsServer;

// HX711 Pins and Setup
const int LOADCELL_DOUT_PIN = D3;
const int LOADCELL_SCK_PIN = D2;
HX711 scale;
const float scaleFactor = 1.0;
const float dropAmount = 50.0;
bool hx711_available = false;

// Servo Setup
const int SERVO_PIN = D6;
Servo servo;
const int servoOpenPos = 90;
const int servoClosedPos = 0;
bool isServoOpen = false;
bool servo_available = false;
float weightAtOpen = 0;

// Wash Relay Setup
const int RELAY_PIN = D5;
const int WASH_DURATION = 30000; // 30 seconds wash duration
bool washInProgress = false;
unsigned long washStartTime = 0;

// Schedules
const int FEED_NUM_SCHEDULES = 3;
String feedSchedules[FEED_NUM_SCHEDULES] = {"08:00", "12:00", "18:00"};
bool feedTimeTriggered[FEED_NUM_SCHEDULES] = {false, false, false};

const int WASH_NUM_SCHEDULES = 2;
String washSchedules[WASH_NUM_SCHEDULES] = {"07:00", "17:00"};
bool washTimeTriggered[WASH_NUM_SCHEDULES] = {false, false};

// WiFi Status
bool wifiDisconnectMessageShown = false;
bool apMode = false;

// AP Mode Static IP
IPAddress apIP(192, 168, 4, 1);
IPAddress apGateway(192, 168, 4, 1);
IPAddress apSubnet(255, 255, 255, 0);

void initializeEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  
  // Check if EEPROM needs initialization
  bool needsInit = false;
  for (int i = FEED_SCHEDULE_ADDR; i < FEED_SCHEDULE_ADDR + 20; i++) {
    if (EEPROM.read(i) == 255) {
      needsInit = true;
      break;
    }
  }
  
  if (needsInit) {
    Serial.println("Initializing EEPROM with default schedules...");
    String defaultFeedTimes[FEED_NUM_SCHEDULES] = {"08:00", "12:00", "18:00"};
    String defaultWashTimes[WASH_NUM_SCHEDULES] = {"07:00", "17:00"};
    
    // Save feed schedules
    for (int i = 0; i < FEED_NUM_SCHEDULES; i++) {
      for (int j = 0; j < 5; j++) {
        if (j < defaultFeedTimes[i].length()) {
          EEPROM.write(FEED_SCHEDULE_ADDR + i * 5 + j, defaultFeedTimes[i][j]);
        } else {
          EEPROM.write(FEED_SCHEDULE_ADDR + i * 5 + j, 0);
        }
      }
    }
    
    // Save wash schedules
    for (int i = 0; i < WASH_NUM_SCHEDULES; i++) {
      for (int j = 0; j < 5; j++) {
        if (j < defaultWashTimes[i].length()) {
          EEPROM.write(WASH_SCHEDULE_ADDR + i * 5 + j, defaultWashTimes[i][j]);
        } else {
          EEPROM.write(WASH_SCHEDULE_ADDR + i * 5 + j, 0);
        }
      }
    }
    
    EEPROM.commit();
    Serial.println("EEPROM initialized with default schedules");
  }
}

void loadSchedules() {
  String defaultFeedTimes[FEED_NUM_SCHEDULES] = {"08:00", "12:00", "18:00"};
  String defaultWashTimes[WASH_NUM_SCHEDULES] = {"07:00", "17:00"};
  
  // Load feed schedules
  for (int i = 0; i < FEED_NUM_SCHEDULES; i++) {
    String time = "";
    for (int j = 0; j < 5; j++) {
      char c = EEPROM.read(FEED_SCHEDULE_ADDR + i * 5 + j);
      if (c == 0 || c == 255) break;
      time += c;
    }
    if (time.length() == 5 && time.indexOf(':') != -1) {
      feedSchedules[i] = time;
    } else {
      feedSchedules[i] = defaultFeedTimes[i];
    }
  }
  
  // Load wash schedules
  for (int i = 0; i < WASH_NUM_SCHEDULES; i++) {
    String time = "";
    for (int j = 0; j < 5; j++) {
      char c = EEPROM.read(WASH_SCHEDULE_ADDR + i * 5 + j);
      if (c == 0 || c == 255) break;
      time += c;
    }
    if (time.length() == 5 && time.indexOf(':') != -1) {
      washSchedules[i] = time;
    } else {
      washSchedules[i] = defaultWashTimes[i];
    }
  }
  
  Serial.println("Loaded schedules:");
  for (int i = 0; i < FEED_NUM_SCHEDULES; i++) {
    Serial.print("Feed "); Serial.print(i); Serial.print(": "); Serial.println(feedSchedules[i]);
  }
  for (int i = 0; i < WASH_NUM_SCHEDULES; i++) {
    Serial.print("Wash "); Serial.print(i); Serial.print(": "); Serial.println(washSchedules[i]);
  }
}

void saveSchedules() {
  // Save feed schedules
  for (int i = 0; i < FEED_NUM_SCHEDULES; i++) {
    for (int j = 0; j < 5; j++) {
      if (j < feedSchedules[i].length()) {
        EEPROM.write(FEED_SCHEDULE_ADDR + i * 5 + j, feedSchedules[i][j]);
      } else {
        EEPROM.write(FEED_SCHEDULE_ADDR + i * 5 + j, 0);
      }
    }
  }
  
  // Save wash schedules
  for (int i = 0; i < WASH_NUM_SCHEDULES; i++) {
    for (int j = 0; j < 5; j++) {
      if (j < washSchedules[i].length()) {
        EEPROM.write(WASH_SCHEDULE_ADDR + i * 5 + j, washSchedules[i][j]);
      } else {
        EEPROM.write(WASH_SCHEDULE_ADDR + i * 5 + j, 0);
      }
    }
  }
  
  if (EEPROM.commit()) {
    Serial.println("Schedules saved to EEPROM");
  } else {
    Serial.println("ERROR: Failed to save schedules to EEPROM");
  }
}

void loadWiFiCredentials() {
  // Read SSID
  for (int i = 0; i < 32; i++) {
    ssid[i] = EEPROM.read(SSID_ADDR + i);
    if (ssid[i] == 0) break;
  }
  ssid[31] = 0;

  // Read Password
  for (int i = 0; i < 32; i++) {
    password[i] = EEPROM.read(PASS_ADDR + i);
    if (password[i] == 0) break;
  }
  password[31] = 0;

  Serial.print("Loaded WiFi: ");
  Serial.println(ssid);
}

void saveWiFiCredentials() {
  // Write SSID
  for (int i = 0; i < 32; i++) {
    EEPROM.write(SSID_ADDR + i, ssid[i]);
    if (ssid[i] == 0) break;
  }

  // Write Password
  for (int i = 0; i < 32; i++) {
    EEPROM.write(PASS_ADDR + i, password[i]);
    if (password[i] == 0) break;
  }

  EEPROM.commit();
  Serial.println("WiFi credentials saved to EEPROM");
}

void connectToWiFi() {
  if (strlen(ssid) == 0) {
    Serial.println("No WiFi credentials stored. Starting AP mode...");
    startAPMode();
    return;
  }

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  // Use DHCP instead of static IP
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    wifiDisconnectMessageShown = false;
    apMode = false;
  } else {
    Serial.println("\nFailed to connect to WiFi");
    startAPMode();
  }
}

void startAPMode() {
  Serial.println("Starting AP mode...");
  
  // Configure AP with static IP
  WiFi.softAPConfig(apIP, apGateway, apSubnet);
  WiFi.softAP("PetFeeder_Setup", "");
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Setup DNS redirect for captive portal
  dnsServer.start(53, "*", WiFi.softAPIP());
  apMode = true;
  
  Serial.println("Captive portal started. Connect to 'PetFeeder_Setup' network");
  Serial.println("and go to http://192.168.4.1 to setup WiFi");
}

void initializeHardware() {
  // Initialize Servo
  if (servo.attach(SERVO_PIN)) {
    servo_available = true;
    closeServo();
    Serial.println("Servo initialized successfully");
  } else {
    Serial.println("Servo initialization failed");
  }

  // Initialize HX711
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  if (scale.wait_ready_timeout(1000)) {
    hx711_available = true;
    scale.set_scale(scaleFactor);
    scale.tare();
    Serial.println("HX711 scale initialized successfully");
  } else {
    Serial.println("HX711 scale initialization failed");
  }

  // Initialize Relay for Wash Cycle
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("Wash relay initialized");
}

void openServo() {
  if (servo_available) {
    servo.write(servoOpenPos);
    isServoOpen = true;
    Serial.println("Servo opened");
  }
}

void closeServo() {
  if (servo_available) {
    servo.write(servoClosedPos);
    isServoOpen = false;
    Serial.println("Servo closed");
  }
}

void startWashCycle() {
  digitalWrite(RELAY_PIN, HIGH);
  washInProgress = true;
  washStartTime = millis();
  Serial.println("Wash cycle started");
}

void stopWashCycle() {
  digitalWrite(RELAY_PIN, LOW);
  washInProgress = false;
  Serial.println("Wash cycle stopped");
}

float getWeight() {
  if (hx711_available && scale.wait_ready_timeout(1000)) {
    return scale.get_units(10);
  }
  return 0.0;
}

void checkSchedules() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  timeClient.update();
  String currentTime = timeClient.getFormattedTime();
  
  // Check feed schedules
  for (int i = 0; i < FEED_NUM_SCHEDULES; i++) {
    if (currentTime == feedSchedules[i] && !feedTimeTriggered[i]) {
      Serial.print("Feed schedule triggered: ");
      Serial.println(feedSchedules[i]);
      openServo();
      if (hx711_available) {
        weightAtOpen = getWeight();
      }
      feedTimeTriggered[i] = true;
    }
    
    // Reset trigger after 1 minute
    if (currentTime != feedSchedules[i]) {
      feedTimeTriggered[i] = false;
    }
  }
  
  // Check wash schedules
  for (int i = 0; i < WASH_NUM_SCHEDULES; i++) {
    if (currentTime == washSchedules[i] && !washTimeTriggered[i]) {
      Serial.print("Wash schedule triggered: ");
      Serial.println(washSchedules[i]);
      startWashCycle();
      washTimeTriggered[i] = true;
    }
    
    // Reset trigger after 1 minute
    if (currentTime != washSchedules[i]) {
      washTimeTriggered[i] = false;
    }
  }
}

void checkAutoClose() {
  if (isServoOpen && hx711_available) {
    float currentWeight = getWeight();
    float weightDropped = weightAtOpen - currentWeight;
    
    if (weightDropped >= dropAmount) {
      closeServo();
      Serial.print("Auto-closed after dropping ");
      Serial.print(weightDropped);
      Serial.println("g");
    }
  }
}

void checkWashTimeout() {
  if (washInProgress && (millis() - washStartTime >= WASH_DURATION)) {
    stopWashCycle();
    Serial.println("Wash cycle completed automatically");
  }
}

void checkWiFiStatus() {
  if (WiFi.status() != WL_CONNECTED && !apMode && !wifiDisconnectMessageShown) {
    Serial.println("WiFi disconnected! Attempting to reconnect...");
    wifiDisconnectMessageShown = true;
    connectToWiFi();
  } else if (WiFi.status() == WL_CONNECTED) {
    wifiDisconnectMessageShown = false;
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "help") {
      Serial.println("Available commands:");
      Serial.println("  help - Show this help");
      Serial.println("  status - Show system status");
      Serial.println("  feed - Start feeding");
      Serial.println("  wash - Start wash cycle");
      Serial.println("  washstop - Stop wash cycle");
      Serial.println("  open - Open servo");
      Serial.println("  close - Close servo");
      Serial.println("  weight - Get current weight");
      Serial.println("  tare - Tare the scale");
      Serial.println("  time - Get current time");
      Serial.println("  reboot - Reboot system");
      Serial.println("  wifi - Show WiFi status");
      Serial.println("  resetschedules - Reset to default schedules");
    }
    else if (command == "status") {
      Serial.print("WiFi: ");
      Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
      Serial.print("Time: ");
      Serial.println(timeClient.getFormattedTime());
      Serial.print("Scale: ");
      Serial.println(hx711_available ? "Available" : "Disabled");
      Serial.print("Servo: ");
      Serial.println(servo_available ? (isServoOpen ? "Open" : "Closed") : "Disabled");
      Serial.print("Wash: ");
      Serial.println(washInProgress ? "In Progress" : "Ready");
      Serial.print("Weight: ");
      Serial.print(getWeight());
      Serial.println("g");
      
      Serial.println("Feed Schedules:");
      for (int i = 0; i < FEED_NUM_SCHEDULES; i++) {
        Serial.print("  "); Serial.print(i); Serial.print(": "); Serial.println(feedSchedules[i]);
      }
      Serial.println("Wash Schedules:");
      for (int i = 0; i < WASH_NUM_SCHEDULES; i++) {
        Serial.print("  "); Serial.print(i); Serial.print(": "); Serial.println(washSchedules[i]);
      }
    }
    else if (command == "feed") {
      openServo();
      if (hx711_available) {
        weightAtOpen = getWeight();
      }
      Serial.println("Feeding started");
    }
    else if (command == "wash") {
      startWashCycle();
      Serial.println("Wash cycle started");
    }
    else if (command == "washstop") {
      stopWashCycle();
      Serial.println("Wash cycle stopped");
    }
    else if (command == "open") {
      openServo();
      Serial.println("Servo opened");
    }
    else if (command == "close") {
      closeServo();
      Serial.println("Servo closed");
    }
    else if (command == "weight") {
      Serial.print("Weight: ");
      Serial.print(getWeight());
      Serial.println("g");
    }
    else if (command == "tare") {
      if (hx711_available) {
        scale.tare();
        Serial.println("Scale tared");
      } else {
        Serial.println("Scale not available");
      }
    }
    else if (command == "time") {
      Serial.print("Time: ");
      Serial.println(timeClient.getFormattedTime());
    }
    else if (command == "reboot") {
      Serial.println("Rebooting...");
      ESP.restart();
    }
    else if (command == "wifi") {
      Serial.print("SSID: ");
      Serial.println(ssid);
      Serial.print("Status: ");
      Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
      }
    }
    else if (command == "resetschedules") {
      for (int i = FEED_SCHEDULE_ADDR; i < WASH_SCHEDULE_ADDR + 10; i++) {
        EEPROM.write(i, 255);
      }
      EEPROM.commit();
      loadSchedules();
      Serial.println("Schedules reset to defaults");
    }
    else {
      Serial.println("Unknown command. Type 'help' for available commands.");
    }
  }
}

void handleStatus() {
  String json = "{";
  json += "\"success\": true,";
  json += "\"wifi\": \"";
  json += (WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  json += "\",";
  json += "\"time\": \"";
  json += (WiFi.status() == WL_CONNECTED ? timeClient.getFormattedTime() : "No WiFi");
  json += "\",";
  json += "\"scale\": \"";
  json += (hx711_available ? "Available" : "Disabled");
  json += "\",";
  json += "\"servo\": \"";
  json += (servo_available ? (isServoOpen ? "Open" : "Closed") : "Disabled");
  json += "\",";
  json += "\"wash\": \"";
  json += (washInProgress ? "In Progress" : "Ready");
  json += "\",";
  json += "\"weight\": ";
  json += String(getWeight());
  json += ",";
  json += "\"lastFeedAmount\": ";
  json += String(isServoOpen ? weightAtOpen - getWeight() : 0);
  json += "}";

  server.send(200, "application/json", json);
}

void handleFeed() {
  String response = "{\"success\": true, \"message\": \"";

  if (servo_available) {
    openServo();
    if (hx711_available) {
      weightAtOpen = getWeight();
    }
    response += "Feeding started successfully\"}";
  } else {
    response = "{\"success\": false, \"message\": \"Servo not available\"}";
  }

  server.send(200, "application/json", response);
}

void handleWash() {
  if (!washInProgress) {
    startWashCycle();
    String response = "{\"success\": true, \"message\": \"Wash cycle started for 30 seconds\"}";
    server.send(200, "application/json", response);
  } else {
    String response = "{\"success\": false, \"message\": \"Wash cycle already in progress\"}";
    server.send(200, "application/json", response);
  }
}

void handleWashStop() {
  if (washInProgress) {
    stopWashCycle();
    String response = "{\"success\": true, \"message\": \"Wash cycle stopped\"}";
    server.send(200, "application/json", response);
  } else {
    String response = "{\"success\": false, \"message\": \"No wash cycle in progress\"}";
    server.send(200, "application/json", response);
  }
}

void handleServoOpen() {
  String response = "{\"success\": true, \"message\": \"";

  if (servo_available) {
    openServo();
    response += "Servo opened successfully\"}";
  } else {
    response = "{\"success\": false, \"message\": \"Servo not available\"}";
  }

  server.send(200, "application/json", response);
}

void handleServoClose() {
  String response = "{\"success\": true, \"message\": \"";

  if (servo_available) {
    closeServo();
    response += "Servo closed successfully\"}";
  } else {
    response = "{\"success\": false, \"message\": \"Servo not available\"}";
  }

  server.send(200, "application/json", response);
}

void handleWeight() {
  String json = "{\"success\": true, \"weight\": " + String(getWeight()) + "}";
  server.send(200, "application/json", json);
}

void handleTare() {
  if (hx711_available) {
    scale.tare();
    String response = "{\"success\": true, \"message\": \"Scale tared successfully\"}";
    server.send(200, "application/json", response);
  } else {
    String response = "{\"success\": false, \"message\": \"Scale not available\"}";
    server.send(200, "application/json", response);
  }
}

void handleTime() {
  String json = "{\"success\": true, \"time\": \"" + timeClient.getFormattedTime() + "\"}";
  server.send(200, "application/json", json);
}

void handleReboot() {
  String json = "{\"success\": true, \"message\": \"Rebooting system...\"}";
  server.send(200, "application/json", json);
  delay(1000);
  ESP.restart();
}

void handleGetSchedule() {
  String json = "{\"success\": true, \"feed_schedule\": [";
  for (int i = 0; i < FEED_NUM_SCHEDULES; i++) {
    json += "\"" + feedSchedules[i] + "\"";
    if (i < FEED_NUM_SCHEDULES - 1) json += ",";
  }
  json += "], \"wash_schedule\": [";
  for (int i = 0; i < WASH_NUM_SCHEDULES; i++) {
    json += "\"" + washSchedules[i] + "\"";
    if (i < WASH_NUM_SCHEDULES - 1) json += ",";
  }
  json += "]}";
  
  server.send(200, "application/json", json);
}

void handleSetSchedule() {
  if (server.hasArg("type") && server.hasArg("index") && server.hasArg("time")) {
    String typeStr = server.arg("type");
    int index = server.arg("index").toInt();
    String timeStr = server.arg("time");
    
    Serial.print("Schedule update: type=");
    Serial.print(typeStr);
    Serial.print(", index=");
    Serial.print(index);
    Serial.print(", time=");
    Serial.println(timeStr);
    
    // Validate time format
    if (timeStr.length() == 5 && timeStr.indexOf(':') == 2 && 
        timeStr.substring(0, 2).toInt() < 24 && 
        timeStr.substring(3, 5).toInt() < 60) {
      
      if (typeStr == "feed" && index >= 0 && index < FEED_NUM_SCHEDULES) {
        feedSchedules[index] = timeStr;
        Serial.print("Updated feed schedule ");
        Serial.print(index);
        Serial.print(" to ");
        Serial.println(timeStr);
      } 
      else if (typeStr == "wash" && index >= 0 && index < WASH_NUM_SCHEDULES) {
        washSchedules[index] = timeStr;
        Serial.print("Updated wash schedule ");
        Serial.print(index);
        Serial.print(" to ");
        Serial.println(timeStr);
      }
      else {
        String response = "{\"success\": false, \"message\": \"Invalid schedule type or index\"}";
        server.send(400, "application/json", response);
        return;
      }
      
      saveSchedules();
      String response = "{\"success\": true, \"message\": \"Schedule updated successfully\"}";
      server.send(200, "application/json", response);
      return;
    }
  }
  
  String response = "{\"success\": false, \"message\": \"Invalid request format\"}";
  server.send(400, "application/json", response);
}

void handleWiFiConfigPage() {
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>WiFi Configuration</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background: #f0f0f0; }
        .container { max-width: 400px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
        h2 { text-align: center; color: #333; margin-bottom: 20px; }
        input { width: 100%; padding: 12px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; font-size: 16px; }
        button { background: #4CAF50; color: white; padding: 12px; border: none; width: 100%; border-radius: 5px; font-size: 16px; cursor: pointer; }
        button:hover { background: #45a049; }
        .message { padding: 10px; margin: 10px 0; border-radius: 5px; text-align: center; }
        .success { background: #d4edda; color: #155724; }
        .error { background: #f8d7da; color: #721c24; }
    </style>
</head>
<body>
    <div class="container">
        <h2>Pet Feeder WiFi Setup</h2>
        <form id="wifiForm">
            <input type="text" name="ssid" placeholder="WiFi SSID" required>
            <input type="password" name="password" placeholder="WiFi Password" required>
            <button type="submit">Save & Connect</button>
        </form>
        <div id="message"></div>
    </div>
    <script>
        document.getElementById('wifiForm').addEventListener('submit', async function(e) {
            e.preventDefault();
            const formData = new FormData(this);
            const data = {
                ssid: formData.get('ssid'),
                password: formData.get('password')
            };
            
            try {
                const response = await fetch('/api/wifi/set', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });
                const result = await response.json();
                
                if (result.success) {
                    document.getElementById('message').innerHTML = 
                        '<div class="message success">WiFi credentials saved! Rebooting...</div>';
                    setTimeout(() => { 
                        alert('Device is rebooting. Connect to your WiFi network and access the device at its new IP address.');
                    }, 2000);
                } else {
                    document.getElementById('message').innerHTML = 
                        '<div class="message error">Error: ' + result.message + '</div>';
                }
            } catch (error) {
                document.getElementById('message').innerHTML = 
                    '<div class="message error">Connection error: ' + error + '</div>';
            }
        });
    </script>
</body>
</html>
)=====";
  server.send(200, "text/html", html);
}

void handleWiFiSet() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    int ssidIndex = body.indexOf("\"ssid\":");
    int passIndex = body.indexOf("\"password\":");
    
    if (ssidIndex != -1 && passIndex != -1) {
      String newSSID = body.substring(ssidIndex + 8, body.indexOf(',', ssidIndex));
      newSSID.replace("\"", "");
      newSSID.trim();
      
      String newPassword = body.substring(passIndex + 12, body.indexOf('}', passIndex));
      newPassword.replace("\"", "");
      newPassword.trim();
      
      newSSID.toCharArray(ssid, 32);
      newPassword.toCharArray(password, 32);
      saveWiFiCredentials();
      
      String response = "{\"success\": true, \"message\": \"WiFi credentials saved. Rebooting...\"}";
      server.send(200, "application/json", response);
      
      delay(2000);
      ESP.restart();
      return;
    }
  }
  
  String response = "{\"success\": false, \"message\": \"Invalid data\"}";
  server.send(400, "application/json", response);
}

void setupWebServer() {
  // Main page and captive portal
  server.on("/", HTTP_GET, handleRoot);
  server.on("/wifi", HTTP_GET, handleWiFiConfigPage);

  // API endpoints
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/feed", HTTP_POST, handleFeed);
  server.on("/api/wash", HTTP_POST, handleWash);
  server.on("/api/wash/stop", HTTP_POST, handleWashStop);
  server.on("/api/servo/open", HTTP_POST, handleServoOpen);
  server.on("/api/servo/close", HTTP_POST, handleServoClose);
  server.on("/api/weight", HTTP_GET, handleWeight);
  server.on("/api/tare", HTTP_POST, handleTare);
  server.on("/api/time", HTTP_GET, handleTime);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/schedule", HTTP_GET, handleGetSchedule);
  server.on("/api/schedule", HTTP_POST, handleSetSchedule);
  server.on("/api/wifi/set", HTTP_POST, handleWiFiSet);

  server.begin();
  Serial.println("Web server started");
}

void handleRoot() {
  // Redirect to WiFi config if in AP mode
  if (apMode) {
    handleWiFiConfigPage();
    return;
  }

  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Pet Feeder Control</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: Arial, sans-serif;
            background: #f0f0f0;
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 10px;
            padding: 20px;
            box-shadow: 0 4px 8px rgba(0,0,0,0.1);
        }
        h1 {
            text-align: center;
            color: #333;
            margin-bottom: 20px;
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }
        .card {
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            border: 1px solid #ddd;
        }
        .card h2 {
            color: #444;
            margin-bottom: 15px;
            font-size: 1.2em;
            border-bottom: 2px solid #4CAF50;
            padding-bottom: 8px;
        }
        .button {
            background: #4CAF50;
            color: white;
            border: none;
            padding: 10px 15px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 14px;
            margin: 5px;
            transition: background 0.3s;
        }
        .button:hover {
            background: #45a049;
        }
        .button.danger {
            background: #f44336;
        }
        .button.danger:hover {
            background: #d32f2f;
        }
        .status-display {
            background: #f9f9f9;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            border-left: 4px solid #4CAF50;
        }
        .status-item {
            margin: 5px 0;
            display: flex;
            justify-content: space-between;
        }
        .status-value {
            font-weight: bold;
            color: #333;
        }
        .input-group {
            margin: 10px 0;
        }
        .input-group label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
            color: #555;
        }
        .input-group input {
            width: 100%;
            padding: 8px;
            border: 1px solid #ddd;
            border-radius: 5px;
            font-size: 14px;
        }
        .schedule-item {
            background: #f9f9f9;
            padding: 8px;
            margin: 5px 0;
            border-radius: 5px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .message {
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            font-weight: bold;
        }
        .success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .info {
            background: #d1ecf1;
            color: #0c5460;
            border: 1px solid #bee5eb;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Pet Feeder Control Panel</h1>
        
        <div class="grid">
            <!-- System Status -->
            <div class="card">
                <h2>System Status</h2>
                <div class="status-display" id="systemStatus">
                    <div class="status-item">
                        <span>WiFi:</span>
                        <span class="status-value" id="wifiStatus">-</span>
                    </div>
                    <div class="status-item">
                        <span>Current Time:</span>
                        <span class="status-value" id="currentTime">-</span>
                    </div>
                    <div class="status-item">
                        <span>Scale:</span>
                        <span class="status-value" id="scaleStatus">-</span>
                    </div>
                    <div class="status-item">
                        <span>Servo:</span>
                        <span class="status-value" id="servoStatus">-</span>
                    </div>
                    <div class="status-item">
                        <span>Wash:</span>
                        <span class="status-value" id="washStatus">-</span>
                    </div>
                    <div class="status-item">
                        <span>Weight:</span>
                        <span class="status-value" id="currentWeight">-</span>
                    </div>
                </div>
                <button class="button" onclick="updateStatus()">Refresh Status</button>
            </div>

            <!-- System Controls -->
            <div class="card">
                <h2>System Controls</h2>
                <button class="button" onclick="rebootSystem()">Reboot System</button>
                <div class="input-group">
                    <label>WiFi SSID:</label>
                    <input type="text" id="newSSID" placeholder="Enter WiFi name">
                </div>
                <div class="input-group">
                    <label>WiFi Password:</label>
                    <input type="password" id="newPassword" placeholder="Enter WiFi password">
                </div>
                <button class="button" onclick="updateWiFi()">Update WiFi</button>
                <div id="wifiMessage"></div>
            </div>

            <!-- Manual Feed Control -->
            <div class="card">
                <h2>Manual Feed Control</h2>
                <button class="button" onclick="openServo()">Open Feeder</button>
                <button class="button danger" onclick="closeServo()">Close Feeder</button>
                <button class="button" onclick="feedNow()">Feed Now</button>
                <div id="feedMessage"></div>
            </div>

            <!--Manual Wash Control -->
            <div class="card">
                <h2>Manual Wash Control</h2>
                <button class="button" onclick="startWash()">Start Wash Cycle</button>
                <button class="button danger" onclick="stopWash()">Stop Wash Cycle</button>
                <div class="status-item">
                    <span>Wash Status:</span>
                    <span class="status-value" id="washStatus">-</span>
                </div>
                <div id="washMessage"></div>
            </div>

            <!-- Feed Schedule -->
            <div class="card">
                <h2>Feed Schedule</h2>
                <div id="feedSchedule">
                    <div class="schedule-item">
                        <span>Morning: <span id="feed0">08:00</span></span>
                        <button class="button" onclick="editSchedule('feed', 0)">Edit</button>
                    </div>
                    <div class="schedule-item">
                        <span>Lunch: <span id="feed1">12:00</span></span>
                        <button class="button" onclick="editSchedule('feed', 1)">Edit</button>
                    </div>
                    <div class="schedule-item">
                        <span>Dinner: <span id="feed2">18:00</span></span>
                        <button class="button" onclick="editSchedule('feed', 2)">Edit</button>
                    </div>
                </div>
                <div id="feedScheduleMessage"></div>
            </div>

            <!-- Wash Schedule -->
            <div class="card">
                <h2>Wash Schedule</h2>
                <div class="schedule-item">
                    <span>Schedule 1: <span id="wash0">07:00</span></span>
                    <button class="button" onclick="editSchedule('wash', 0)">Edit</button>
                </div>
                <div class="schedule-item">
                    <span>Schedule 2: <span id="wash1">17:00</span></span>
                    <button class="button" onclick="editSchedule('wash', 1)">Edit</button>
                </div>
                <div id="washScheduleMessage"></div>
            </div>

            <!-- Weight Monitor -->
            <div class="card">
                <h2>Weight Monitor</h2>
                <div class="status-display">
                    <div class="status-item">
                        <span>Current Weight:</span>
                        <span class="status-value" id="liveWeight">-</span>
                    </div>
                    <div class="status-item">
                        <span>Last Feed Amount:</span>
                        <span class="status-value" id="lastFeedAmount">-</span>
                    </div>
                </div>
                <button class="button" onclick="tareScale()">Tare Scale</button>
                <div id="tareMessage"></div>
            </div>
        </div>
    </div>

    <script>
        // Update status on page load
        window.onload = function() {
            updateStatus();
            loadSchedules();
        };

        // API call helper
        async function apiCall(endpoint, method = 'GET', data = null) {
            try {
                const options = {
                    method: method,
                    headers: {}
                };

                if (data && method !== 'GET') {
                    if (data instanceof FormData) {
                        options.body = data;
                    } else {
                        options.headers['Content-Type'] = 'application/json';
                        options.body = JSON.stringify(data);
                    }
                }

                const response = await fetch('/api/' + endpoint, options);
                return await response.json();
            } catch (error) {
                console.error('API call failed:', error);
                return { success: false, error: error.message };
            }
        }

        // Status functions
        async function updateStatus() {
            const status = await apiCall('status');
            if (status.success) {
                document.getElementById('wifiStatus').textContent = status.wifi;
                document.getElementById('currentTime').textContent = status.time;
                document.getElementById('scaleStatus').textContent = status.scale;
                document.getElementById('servoStatus').textContent = status.servo;
                document.getElementById('washStatus').textContent = status.wash;
                document.getElementById('currentWeight').textContent = status.weight + 'g';
                document.getElementById('liveWeight').textContent = status.weight + 'g';
                document.getElementById('lastFeedAmount').textContent = status.lastFeedAmount + 'g';
            }
        }

        async function loadSchedules() {
            const schedule = await apiCall('schedule');
            if (schedule.success) {
                schedule.feed_schedule.forEach((time, index) => {
                    document.getElementById('feed' + index).textContent = time;
                });
                schedule.wash_schedule.forEach((time, index) => {
                    document.getElementById('wash' + index).textContent = time;
                });
            }
        }

        // Feed functions
        async function openServo() {
            showMessage('feedMessage', 'Opening feeder...', 'info');
            const result = await apiCall('servo/open', 'POST');
            showMessage('feedMessage', result.message, result.success ? 'success' : 'error');
            updateStatus();
        }

        async function closeServo() {
            showMessage('feedMessage', 'Closing feeder...', 'info');
            const result = await apiCall('servo/close', 'POST');
            showMessage('feedMessage', result.message, result.success ? 'success' : 'error');
            updateStatus();
        }

        async function feedNow() {
            showMessage('feedMessage', 'Feeding pet...', 'info');
            const result = await apiCall('feed', 'POST');
            showMessage('feedMessage', result.message, result.success ? 'success' : 'error');
            updateStatus();
        }

        // Wash functions
        async function startWash() {
            showMessage('washMessage', 'Starting wash cycle...', 'info');
            const result = await apiCall('wash', 'POST');
            showMessage('washMessage', result.message, result.success ? 'success' : 'error');
            updateStatus();
        }

        async function stopWash() {
            showMessage('washMessage', 'Stopping wash cycle...', 'info');
            const result = await apiCall('wash/stop', 'POST');
            showMessage('washMessage', result.message, result.success ? 'success' : 'error');
            updateStatus();
        }

        // Schedule functions
        async function editSchedule(type, index) {
            const currentElement = document.getElementById(type + index);
            const currentTime = currentElement.textContent;
            const newTime = prompt('Enter new time (HH:MM):', currentTime);
            
            if (newTime && /^[0-9]{2}:[0-9]{2}$/.test(newTime)) {
                const formData = new FormData();
                formData.append('type', type);
                formData.append('index', index.toString());
                formData.append('time', newTime);
                
                const result = await apiCall('schedule', 'POST', formData);
                
                if (result.success) {
                    currentElement.textContent = newTime;
                    showMessage(type + 'ScheduleMessage', 'Schedule updated successfully', 'success');
                } else {
                    showMessage(type + 'ScheduleMessage', 'Error: ' + result.message, 'error');
                }
            } else if (newTime) {
                showMessage(type + 'ScheduleMessage', 'Invalid time format. Use HH:MM (e.g., 08:00)', 'error');
            }
        }

        // System functions
        async function rebootSystem() {
            if (confirm('Are you sure you want to reboot the system?')) {
                await apiCall('reboot', 'POST');
                alert('System is rebooting. Please wait 30 seconds and refresh the page.');
            }
        }

        async function tareScale() {
            showMessage('tareMessage', 'Taring scale...', 'info');
            const result = await apiCall('tare', 'POST');
            showMessage('tareMessage', result.message, result.success ? 'success' : 'error');
            updateStatus();
        }

        async function updateWiFi() {
            const ssid = document.getElementById('newSSID').value;
            const password = document.getElementById('newPassword').value;

            if (!ssid) {
                alert('Please enter SSID');
                return;
            }

            showMessage('wifiMessage', 'Updating WiFi...', 'info');
            const result = await apiCall('wifi/set', 'POST', { ssid, password });
            showMessage('wifiMessage', result.message, result.success ? 'success' : 'error');
            
            if (result.success) {
                setTimeout(() => {
                    alert('WiFi updated. System will reboot and connect to new network.');
                }, 1000);
            }
        }

        // Utility functions
        function showMessage(elementId, message, type) {
            const element = document.getElementById(elementId);
            element.innerHTML = '<div class="message ' + type + '">' + message + '</div>';
            setTimeout(function() {
                element.innerHTML = '';
            }, 3000);
        }
    </script>
</body>
</html>
)=====";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== Pet Feeder System Starting ===");
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Initialize EEPROM
  initializeEEPROM();

  // Initialize hardware
  initializeHardware();

  // Load WiFi credentials and connect
  loadWiFiCredentials();
  loadSchedules();
  
  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  if (strlen(ssid) == 0) {
    Serial.println("No WiFi credentials stored. Starting AP mode...");
    startAPMode();
  } else {
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(500);
      Serial.print(".");
      attempts++;
      if (attempts % 10 == 0) Serial.println(); // New line every 10 attempts
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected successfully!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Gateway: ");
      Serial.println(WiFi.gatewayIP());
      Serial.print("Subnet: ");
      Serial.println(WiFi.subnetMask());
      
      wifiDisconnectMessageShown = false;
      apMode = false;
    } else {
      Serial.println("\nFailed to connect to WiFi. Starting AP mode...");
      startAPMode();
    }
  }

  // Initialize NTP
  timeClient.begin();

  // Setup web server routes
  setupWebServer();

  Serial.println("=== System Ready ===");
  Serial.println("Type 'help' for available commands");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Web interface: http://");
    Serial.println(WiFi.localIP());
  } else if (apMode) {
    Serial.print("AP Mode: http://");
    Serial.println(WiFi.softAPIP());
    Serial.println("Connect to WiFi: PetFeeder_Setup");
    Serial.println("No password required");
  }
  
  Serial.println("========================");
}

void loop() {
  // Handle DNS requests in AP mode
  if (apMode) {
    dnsServer.processNextRequest();
  }
  
  // Handle web server requests
  server.handleClient();
  
  // Update NTP time if connected to WiFi
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
  }
  
  // Check schedules
  checkSchedules();
  
  // Check auto-close condition
  checkAutoClose();
  
  // Check wash cycle timeout
  checkWashTimeout();
  
  // Check WiFi status
  checkWiFiStatus();
  
  // Handle serial commands
  handleSerialCommands();
  
  delay(100);
}
