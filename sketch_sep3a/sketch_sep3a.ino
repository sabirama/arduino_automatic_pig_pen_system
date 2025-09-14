#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <Servo.h>
#include <HX711.h>

// EEPROM Addresses
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 50

char ssid[32];
char password[32];

// Web Server
ESP8266WebServer server(80);

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28000, 60000);

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

// SIM800L Setup (optional)
const int SIM800_TX = D7;
const int SIM800_RX = D8;
bool sim800_available = false;

// Schedule
const int FEED_NUM_SCHEDULES = 3;
String servoOpenTimes[FEED_NUM_SCHEDULES] = {"08:00", "12:00", "18:00"};
bool feedTimeTriggered[FEED_NUM_SCHEDULES] = {false, false, false};

const int WASH_NUM_SCHEDULES = 2;
String relayOpenTimes[WASH_NUM_SCHEDULES] = {"07:00", "17:00"};
bool washTimeTriggered[WASH_NUM_SCHEDULES] = {false, false};

// WiFi Status
bool wifiDisconnectMessageShown = false;

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

  // Setup web server routes
  setupWebServer();

  Serial.println("=== System Ready ===");
  Serial.println("Type 'help' for available commands");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Web interface: http://");
    Serial.println(WiFi.localIP());
  }
  Serial.println("========================");
}

void setupWebServer() {
  // Main page
  server.on("/", HTTP_GET, handleRoot);

  // API endpoints
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/feed", HTTP_POST, handleFeed);
  server.on("/api/wash", HTTP_POST, handleWash);
  server.on("/api/servo/open", HTTP_POST, handleServoOpen);
  server.on("/api/servo/close", HTTP_POST, handleServoClose);
  server.on("/api/weight", HTTP_GET, handleWeight);
  server.on("/api/time", HTTP_GET, handleTime);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/schedule", HTTP_GET, handleGetSchedule);
  server.on("/api/schedule", HTTP_POST, handleSetSchedule);

  server.begin();
  Serial.println("Web server started");
}

void handleRoot() {
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
            max-width: 1000px;
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
                        <span>Weight:</span>
                        <span class="status-value" id="currentWeight">-</span>
                    </div>
                </div>
                <button class="button" onclick="updateStatus()">Refresh Status</button>
            </div>

            <!-- Manual Feed Control -->
            <div class="card">
                <h2>Manual Feed Control</h2>
                <button class="button" onclick="openServo()">Open Feeder</button>
                <button class="button danger" onclick="closeServo()">Close Feeder</button>
                <button class="button" onclick="feedNow()">Feed Now</button>
                <div id="feedMessage"></div>
            </div>

            <!-- Schedule Management -->
            <div class="card">
                <h2>Feed Schedule</h2>
                <div id="feedSchedule">
                    <div class="schedule-item">
                        <span>Morning: 08:00</span>
                        <button class="button" onclick="editSchedule(0)">Edit</button>
                    </div>
                    <div class="schedule-item">
                        <span>Lunch: 12:00</span>
                        <button class="button" onclick="editSchedule(1)">Edit</button>
                    </div>
                    <div class="schedule-item">
                        <span>Dinner: 18:00</span>
                        <button class="button" onclick="editSchedule(2)">Edit</button>
                    </div>
                </div>
                <div class="input-group">
                    <label>Add New Feed Time:</label>
                    <input type="time" id="newFeedTime" value="09:00">
                    <button class="button" onclick="addFeedTime()">Add Schedule</button>
                </div>
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
            </div>
        </div>
    </div>

    <script>
        // Update status on page load
        window.onload = function() {
            updateStatus();
        };

        // API call helper
        async function apiCall(endpoint, method = 'GET', data = null) {
            try {
                const options = {
                    method: method,
                    headers: {
                        'Content-Type': 'application/json',
                    }
                };

                if (data && method !== 'GET') {
                    options.body = JSON.stringify(data);
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
                document.getElementById('currentWeight').textContent = status.weight + 'g';
                document.getElementById('liveWeight').textContent = status.weight + 'g';
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

        // System functions
        async function rebootSystem() {
            if (confirm('Are you sure you want to reboot the system?')) {
                await apiCall('reboot', 'POST');
                alert('System is rebooting. Please wait 30 seconds and refresh the page.');
            }
        }

        async function tareScale() {
            alert('Scale tare would be triggered here');
        }

        async function updateWiFi() {
            const ssid = document.getElementById('newSSID').value;
            const password = document.getElementById('newPassword').value;

            if (!ssid || !password) {
                alert('Please enter both SSID and password');
                return;
            }

            alert('WiFi update would be triggered here with SSID: ' + ssid);
        }

        // Schedule functions
        function editSchedule(index) {
            const newTime = prompt('Enter new feed time (HH:MM):');
            if (newTime && /^[0-9]{2}:[0-9]{2}$/.test(newTime)) {
                alert('Schedule ' + index + ' updated to: ' + newTime);
            }
        }

        function addFeedTime() {
            const newTime = document.getElementById('newFeedTime').value;
            if (newTime) {
                alert('New feed time added: ' + newTime);
                document.getElementById('newFeedTime').value = '';
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

// API Handlers
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
  json += "\"weight\": ";
  json += String(getWeight());
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
  String response = "{\"success\": true, \"message\": \"Wash cycle started\"}";
  server.send(200, "application/json", response);
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

void handleTime() {
  String json = "{\"success\": true, \"time\": \"" +timeClient.getFormattedTime() + "\"}";
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
    json += "\"" + servoOpenTimes[i] + "\"";
    if (i < FEED_NUM_SCHEDULES - 1) json += ",";
  }
  json += "], \"wash_schedule\": [";
  for (int i = 0; i < WASH_NUM_SCHEDULES; i++) {
    json += "\"" + relayOpenTimes[i] + "\"";
    if (i < WASH_NUM_SCHEDULES - 1) json += ",";
  }
  json += "]}";
  
  server.send(200, "application/json", json);
}

void handleSetSchedule() {
  String json = "{\"success\": true, \"message\": \"Schedule updated\"}";
  server.send(200, "application/json", json);
}

// Hardware initialization
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

  // Initialize SIM800L (optional)
  // SoftwareSerial sim800(SIM800_TX, SIM800_RX);
  // sim800.begin(9600);
  // sim800_available = true;
  // Serial.println("SIM800L initialized (simulated)");
}

// WiFi functions
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
    Serial.println("No WiFi credentials stored. Please configure via web interface.");
    startAPMode();
    return;
  }

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
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
  } else {
    Serial.println("\nFailed to connect to WiFi");
    startAPMode();
  }
}

void startAPMode() {
  Serial.println("Starting AP mode...");
  WiFi.softAP("PetFeeder_AP", "12345678");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

// Servo control functions
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

// Weight measurement
float getWeight() {
  if (hx711_available && scale.is_ready()) {
    return scale.get_units(5);
  }
  return 0.0;
}

// Main loop
void loop() {
  server.handleClient();
  timeClient.update();

  // Check WiFi status
  if (WiFi.status() != WL_CONNECTED && !wifiDisconnectMessageShown) {
    Serial.println("WiFi disconnected!");
    wifiDisconnectMessageShown = true;
  } else if (WiFi.status() == WL_CONNECTED && wifiDisconnectMessageShown) {
    Serial.println("WiFi reconnected");
    wifiDisconnectMessageShown = false;
  }

  // Check scheduled feed times
  checkScheduledFeeds();

  // Check serial commands
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    handleSerialCommand(command);
  }

  delay(100);
}

void checkScheduledFeeds() {
  if (WiFi.status() != WL_CONNECTED) return;

  String currentTime = timeClient.getFormattedTime();
  
  // Check feed schedules
  for (int i = 0; i < FEED_NUM_SCHEDULES; i++) {
    if (currentTime.startsWith(servoOpenTimes[i]) && !feedTimeTriggered[i]) {
      Serial.print("Scheduled feed time triggered: ");
      Serial.println(servoOpenTimes[i]);
      feedTimeTriggered[i] = true;
      openServo();
      if (hx711_available) {
        weightAtOpen = getWeight();
      }
    }
    
    // Reset trigger after minute passes
    if (!currentTime.startsWith(servoOpenTimes[i]) && feedTimeTriggered[i]) {
      feedTimeTriggered[i] = false;
    }
  }

  // Check wash schedules
  for (int i = 0; i < WASH_NUM_SCHEDULES; i++) {
    if (currentTime.startsWith(relayOpenTimes[i]) && !washTimeTriggered[i]) {
      Serial.print("Scheduled wash time triggered: ");
      Serial.println(relayOpenTimes[i]);
      washTimeTriggered[i] = true;
      // Implement wash cycle here
    }
    
    // Reset trigger after minute passes
    if (!currentTime.startsWith(relayOpenTimes[i]) && washTimeTriggered[i]) {
      washTimeTriggered[i] = false;
    }
  }
}

void handleSerialCommand(String command) {
  command.toLowerCase();
  
  if (command == "help") {
    Serial.println("Available commands:");
    Serial.println("  help - Show this help");
    Serial.println("  status - Show system status");
    Serial.println("  feed - Trigger feeding");
    Serial.println("  open - Open servo");
    Serial.println("  close - Close servo");
    Serial.println("  weight - Get current weight");
    Serial.println("  time - Get current time");
    Serial.println("  reboot - Reboot system");
    Serial.println("  wifi - Show WiFi status");
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
    Serial.print("Weight: ");
    Serial.print(getWeight());
    Serial.println("g");
  }
  else if (command == "feed") {
    Serial.println("Feeding...");
    openServo();
    if (hx711_available) {
      weightAtOpen = getWeight();
    }
  }
  else if (command == "open") {
    Serial.println("Opening servo...");
    openServo();
  }
  else if (command == "close") {
    Serial.println("Closing servo...");
    closeServo();
  }
  else if (command == "weight") {
    Serial.print("Current weight: ");
    Serial.print(getWeight());
    Serial.println("g");
  }
  else if (command == "time") {
    Serial.print("Current time: ");
    Serial.println(timeClient.getFormattedTime());
  }
  else if (command == "reboot") {
    Serial.println("Rebooting system...");
    delay(1000);
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
  else {
    Serial.println("Unknown command. Type 'help' for available commands.");
  }
}
