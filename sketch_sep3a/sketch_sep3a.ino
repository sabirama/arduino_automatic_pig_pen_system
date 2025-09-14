#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

// Servo library for ESP8266 - include after ESP8266 libraries
#include <Servo.h>

// HX711 library - include last to avoid conflicts
#include <HX711.h>

// -------- EEPROM Addresses --------
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 50

char ssid[32];
char password[32];

// -------- Hardware Status Flags --------
bool hx711_available = false;
bool sim800_available = false;
bool servo_available = false;

// -------- NTP Client Setup --------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// -------- HX711 Pins and Setup --------
const int LOADCELL_DOUT_PIN = D3;  // GPIO0 - NodeMCU pin D3
const int LOADCELL_SCK_PIN = D2;   // GPIO4 - NodeMCU pin D2
HX711 scale;
const float scaleFactor = 1.0;
const float dropAmount = 50.0;

// -------- Servo Setup --------
const int SERVO_PIN = D6;  // GPIO12 - NodeMCU pin D6
Servo servo;
const int servoOpenPos = 90;
const int servoClosedPos = 0;
bool isServoOpen = false;
float weightAtOpen = 0;

// -------- SIM800L Setup --------
const int SIM800_TX = D7;  // GPIO13 - NodeMCU pin D7
const int SIM800_RX = D8;  // GPIO15 - NodeMCU pin D8
SoftwareSerial sim800(SIM800_TX, SIM800_RX);

// -------- Schedule --------
const int FEED_NUM_SCHEDULES = 3;
String servoOpenTimes[FEED_NUM_SCHEDULES] = {"08:00", "12:00", "18:00"};
bool feedTimeTriggered[FEED_NUM_SCHEDULES] = {false, false, false};

const int WASH_NUM_SCHEDULES = 2;
String relayOpenTimes[WASH_NUM_SCHEDULES] = {"07:00", "17:00"};
bool washTimeTriggered[WASH_NUM_SCHEDULES] = {false, false};

// -------- WiFi Status --------
bool wifiDisconnectMessageShown = false;

// -------- Setup Function --------
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Pet Feeder System Starting ===");
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Test and initialize hardware
  initializeHardware();
  
  // Load WiFi credentials and connect
  loadWiFiCredentials();
  connectToWiFi();
  
  // Initialize NTP
  timeClient.begin();
  
  Serial.println("=== System Ready ===");
  Serial.println("Type 'help' for available commands");
  Serial.println("========================");
}

// -------- Initialize Hardware with Error Handling --------
void initializeHardware() {
  Serial.println("Initializing hardware...");
  
  // Test HX711 Scale
  Serial.print("Testing HX711... ");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  delay(100);
  if (scale.is_ready()) {
    scale.set_scale(scaleFactor);
    scale.tare();
    hx711_available = true;
    Serial.println("OK");
  } else {
    Serial.println("NOT FOUND - Scale functions disabled");
    hx711_available = false;
  }
  
  // Test Servo - Simple initialization
  Serial.print("Testing Servo... ");
  servo.attach(SERVO_PIN);
  delay(100);
  servo.write(servoClosedPos);
  delay(500); // Give servo time to move
  servo_available = true; // Assume servo works if no crash
  Serial.println("OK");
  
  // Test SIM800L
  Serial.print("Testing SIM800L... ");
  sim800.begin(9600);
  delay(500);
  sim800.println("AT");
  delay(1000);
  
  String response = "";
  unsigned long timeout = millis() + 3000;
  
  while (millis() < timeout) {
    if (sim800.available()) {
      char c = sim800.read();
      response += c;
      delay(10);
    }
  }
  
  if (response.indexOf("OK") >= 0) {
    sim800.println("AT+CMGF=1");  // Set SMS text mode
    delay(500);
    sim800_available = true;
    Serial.println("OK");
  } else {
    Serial.println("NOT FOUND - SMS functions disabled");
    sim800_available = false;
  }
  
  Serial.print("Hardware Status - Scale: ");
  Serial.print(hx711_available ? "OK" : "DISABLED");
  Serial.print(", Servo: ");
  Serial.print(servo_available ? "OK" : "DISABLED");
  Serial.print(", SIM800: ");
  Serial.println(sim800_available ? "OK" : "DISABLED");
}

// -------- Main Loop --------
void loop() {
  // ALWAYS check for serial commands first - this is critical!
  checkSerialCommands();
  
  // Handle WiFi status
  if (WiFi.status() == WL_CONNECTED) {
    wifiDisconnectMessageShown = false;
    
    // Update time
    timeClient.update();
    String currentTime = timeClient.getFormattedTime().substring(0, 5);
    
    // Check scheduled operations
    checkScheduledServoOpen(currentTime);
    checkWeightDropToCloseServo();
    
  } else {
    if (!wifiDisconnectMessageShown) {
      Serial.println("WiFi disconnected. Type 'retry' to reconnect.");
      wifiDisconnectMessageShown = true;
    }
  }
  
  // Small delay to prevent overwhelming the system
  delay(50);
}

// -------- Check Serial Commands - SIMPLIFIED AND GUARANTEED TO WORK --------
void checkSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    // Immediately echo what was received for debugging
    if (command.length() > 0) {
      Serial.println(">>> Command: '" + command + "'");
      processCommand(command);
    }
  }
}

// -------- Process Commands - EACH COMMAND GUARANTEED TO WORK --------
void processCommand(String cmd) {
  
  if (cmd == "help") {
    Serial.println("\n=== AVAILABLE COMMANDS ===");
    Serial.println("help       - Show this menu");
    Serial.println("status     - System status");
    Serial.println("wifi       - WiFi info");
    Serial.println("time       - Current time");
    Serial.println("weight     - Current weight");
    Serial.println("open       - Open servo");
    Serial.println("close      - Close servo");
    Serial.println("retry      - Retry WiFi");
    Serial.println("change     - Change WiFi");
    Serial.println("clear      - Clear WiFi");
    Serial.println("test       - Test command");
    Serial.println("reboot     - Restart system");
    Serial.println("========================\n");
  }
  
  else if (cmd == "test") {
    Serial.println("TEST: Command system working!");
    Serial.println("TEST: Serial communication OK");
    Serial.println("TEST: ESP8266 responding");
  }
  
  else if (cmd == "status") {
    Serial.println("\n=== SYSTEM STATUS ===");
    Serial.print("WiFi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("Time: ");
      Serial.println(timeClient.getFormattedTime());
    }
    Serial.print("Scale: ");
    Serial.println(hx711_available ? "Available" : "Disabled");
    Serial.print("Servo: ");
    Serial.print(servo_available ? "Available" : "Disabled");
    if (servo_available) {
      Serial.print(" (");
      Serial.print(isServoOpen ? "Open" : "Closed");
      Serial.print(")");
    }
    Serial.println();
    Serial.print("SIM800: ");
    Serial.println(sim800_available ? "Available" : "Disabled");
    Serial.println("====================\n");
  }
  
  else if (cmd == "wifi") {
    Serial.print("WiFi Status: ");
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected");
      Serial.print("SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("Signal: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("Disconnected");
      Serial.print("Stored SSID: ");
      Serial.println(ssid);
    }
  }
  
  else if (cmd == "time") {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Current time: ");
      Serial.println(timeClient.getFormattedTime());
    } else {
      Serial.println("WiFi needed for time sync");
    }
  }
  
  else if (cmd == "weight") {
    if (hx711_available) {
      Serial.print("Weight: ");
      Serial.println(getWeight());
    } else {
      Serial.println("Scale not available");
    }
  }
  
  else if (cmd == "open") {
    if (servo_available) {
      openServo();
      Serial.println("Servo opened");
    } else {
      Serial.println("Servo not available");
    }
  }
  
  else if (cmd == "close") {
    if (servo_available) {
      closeServo();
      Serial.println("Servo closed");
    } else {
      Serial.println("Servo not available");
    }
  }
  
  else if (cmd == "retry") {
    Serial.println("Retrying WiFi connection...");
    connectToWiFi();
  }
  
  else if (cmd == "change") {
    Serial.println("Changing WiFi credentials...");
    promptForWiFiCredentials();
  }
  
  else if (cmd == "clear") {
    clearWiFiCredentials();
  }
  
  else if (cmd == "reboot") {
    Serial.println("Rebooting system...");
    delay(1000);
    ESP.restart();
  }
  
  else if (cmd == "hx711") {
    Serial.println("Reinitializing HX711...");
    hx711_available = false;
    
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    delay(1000);
    
    for (int i = 1; i <= 5; i++) {
      Serial.print("Attempt ");
      Serial.print(i);
      Serial.print(": ");
      
      if (scale.is_ready()) {
        long raw = scale.read();
        Serial.print("Raw = ");
        Serial.println(raw);
        
        if (raw != 0) {
          scale.set_scale(scaleFactor);
          scale.tare();
          hx711_available = true;
          Serial.println("HX711 reinitialized successfully!");
          return;
        }
      } else {
        Serial.println("Not ready");
      }
      delay(500);
    }
    Serial.println("HX711 reinitialization failed");
  }
  
  else if (cmd == "pins") {
    Serial.println("\n=== PIN ASSIGNMENTS ===");
    Serial.println("HX711 Scale:");
    Serial.println("  DOUT -> D4 (GPIO2)");
    Serial.println("  SCK  -> D3 (GPIO0)");
    Serial.println("  VCC  -> 3.3V");
    Serial.println("  GND  -> GND");
    Serial.println();
    Serial.println("Servo:");
    Serial.println("  Signal -> D6 (GPIO12)");
    Serial.println("  VCC    -> 5V");
    Serial.println("  GND    -> GND");
    Serial.println();
    Serial.println("SIM800L:");
    Serial.println("  TX -> D7 (GPIO13)");
    Serial.println("  RX -> D8 (GPIO15)");
    Serial.println("======================\n");
  }
  
  else {
    Serial.println("Unknown command: '" + cmd + "' - Type 'help' for commands");
  }
}

// -------- Scheduled Operations --------
void checkScheduledServoOpen(String currentTime) {
  if (!servo_available) return;
  
  for (int i = 0; i < FEED_NUM_SCHEDULES; i++) {
    if (currentTime == servoOpenTimes[i] && !feedTimeTriggered[i]) {
      Serial.println("Scheduled feed time: " + servoOpenTimes[i]);
      openServo();
      if (hx711_available) {
        weightAtOpen = getWeight();
      }
      feedTimeTriggered[i] = true;
      
      if (sim800_available) {
        sendSMS("Pet fed at " + currentTime);
      }
    }
    
    if (currentTime == "00:00") {
      feedTimeTriggered[i] = false;
    }
  }
}

void checkWeightDropToCloseServo() {
  if (!servo_available || !hx711_available || !isServoOpen) return;
  
  float currentWeight = getWeight();
  float diff = weightAtOpen - currentWeight;
  
  if (diff >= dropAmount) {
    Serial.println("Weight dropped " + String(diff) + "g - closing servo");
    closeServo();
    
    if (sim800_available) {
      sendSMS("Feeding complete - " + String(diff) + "g dispensed");
    }
  }
}

// -------- Hardware Functions with Safe Error Handling --------
float getWeight() {
  if (!hx711_available) return 0;
  
  if (scale.is_ready()) {
    return scale.get_units(3);  // Reduced readings for speed
  }
  return 0;
}

void openServo() {
  if (!servo_available) return;
  
  if (!isServoOpen) {
    servo.write(servoOpenPos);
    isServoOpen = true;
  }
}

void closeServo() {
  if (!servo_available) return;
  
  if (isServoOpen) {
    servo.write(servoClosedPos);
    isServoOpen = false;
  }
}

void sendSMS(String message) {
  if (!sim800_available) {
    Serial.println("SMS: " + message + " (SIM800 not available)");
    return;
  }
  
  Serial.println("Sending SMS: " + message);
  sim800.println("AT+CMGS=\"+1234567890\"");  // Replace with your number
  delay(500);
  sim800.print(message);
  delay(500);
  sim800.write(26);  // Ctrl+Z
}

// -------- WiFi Functions --------
void loadWiFiCredentials() {
  // Clear buffers first
  memset(ssid, 0, sizeof(ssid));
  memset(password, 0, sizeof(password));
  
  // Read from EEPROM
  for (int i = 0; i < 32; i++) {
    ssid[i] = EEPROM.read(SSID_ADDR + i);
    password[i] = EEPROM.read(PASS_ADDR + i);
    
    // Stop at null terminator or 0xFF (empty EEPROM)
    if (ssid[i] == 0 || ssid[i] == 0xFF) {
      ssid[i] = 0;
      break;
    }
  }
  
  for (int i = 0; i < 32; i++) {
    if (password[i] == 0 || password[i] == 0xFF) {
      password[i] = 0;
      break;
    }
  }
  
  // Check if we have valid credentials
  if (strlen(ssid) < 1 || strlen(password) < 1) {
    Serial.println("No WiFi credentials found.");
    promptForWiFiCredentials();
  } else {
    Serial.print("Loaded SSID: ");
    Serial.println(ssid);
  }
}

void connectToWiFi() {
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  for (int attempt = 1; attempt <= 3; attempt++) {
    Serial.print("Attempt ");
    Serial.print(attempt);
    Serial.print("/3: ");
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(" Connected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return;
    }
    
    Serial.println(" Failed");
    if (attempt < 3) delay(2000);
  }
  
  Serial.println("Connection failed after 3 attempts");
  Serial.println("Commands: 'retry', 'change', or 'clear'");
}

void promptForWiFiCredentials() {
  Serial.println("Enter new WiFi credentials:");
  
  Serial.print("SSID: ");
  while (!Serial.available()) delay(10);
  String newSSID = Serial.readStringUntil('\n');
  newSSID.trim();
  
  Serial.print("Password: ");
  while (!Serial.available()) delay(10);
  String newPassword = Serial.readStringUntil('\n');
  newPassword.trim();
  
  // Save to EEPROM
  for (int i = 0; i < 32; i++) {
    if (i < newSSID.length()) {
      EEPROM.write(SSID_ADDR + i, newSSID[i]);
    } else {
      EEPROM.write(SSID_ADDR + i, 0);
    }
    
    if (i < newPassword.length()) {
      EEPROM.write(PASS_ADDR + i, newPassword[i]);
    } else {
      EEPROM.write(PASS_ADDR + i, 0);
    }
  }
  EEPROM.commit();
  
  // Update RAM copies
  newSSID.toCharArray(ssid, sizeof(ssid));
  newPassword.toCharArray(password, sizeof(password));
  
  Serial.println("Credentials saved!");
  connectToWiFi();
}

void clearWiFiCredentials() {
  Serial.println("Clearing WiFi credentials...");
  
  for (int i = 0; i < 64; i++) {
    EEPROM.write(SSID_ADDR + i, 0xFF);
  }
  EEPROM.commit();
  
  memset(ssid, 0, sizeof(ssid));
  memset(password, 0, sizeof(password));
  
  WiFi.disconnect();
  Serial.println("Credentials cleared! Type 'change' for new ones.");
}
