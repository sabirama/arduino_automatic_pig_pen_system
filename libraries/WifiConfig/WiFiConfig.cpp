#include "WiFiConfig.h"

WiFiConfig::WiFiConfig(const char* apName, const char* apPassword, 
                       int eepromStart, int eepromSize)
  : _apName(apName), _apPassword(apPassword), 
    _eepromStart(eepromStart), _eepromSize(eepromSize), _server(80) {}

void WiFiConfig::begin() {
  EEPROM.begin(_eepromSize);
  loadCredentials();
  delay(500);

  if (_ssid.length() > 0) {
    tryConnectToWiFi();
  } else {
    Serial.println("No saved WiFi credentials. Starting AP mode...");
    startConfigPortal();
  }
}

void WiFiConfig::handleClient() {
  if (_apModeActive) {
    _server.handleClient();
  }
}

bool WiFiConfig::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

IPAddress WiFiConfig::getIP() {
  if (_apModeActive) {
    return WiFi.softAPIP();
  }
  return WiFi.localIP();
}

void WiFiConfig::clearCredentials() {
  for (int i = 0; i < _eepromSize; i++) {
    EEPROM.write(_eepromStart + i, 0);
  }
  EEPROM.commit();
  ESP.restart();
}

void WiFiConfig::loadCredentials() {
  char ssidBuf[32];
  char passBuf[64];

  for (int i = 0; i < 32; ++i)
    ssidBuf[i] = EEPROM.read(_eepromStart + i);
  for (int i = 0; i < 64; ++i)
    passBuf[i] = EEPROM.read(_eepromStart + 32 + i);

  _ssid = String(ssidBuf);
  _password = String(passBuf);
  _ssid.trim();
  _password.trim();
}

void WiFiConfig::saveCredentials(const String& newSsid, const String& newPassword) {
  for (int i = 0; i < 32; ++i)
    EEPROM.write(_eepromStart + i, i < newSsid.length() ? newSsid[i] : 0);
  for (int i = 0; i < 64; ++i)
    EEPROM.write(_eepromStart + 32 + i, i < newPassword.length() ? newPassword[i] : 0);
  EEPROM.commit();
}

void WiFiConfig::startConfigPortal() {
  _apModeActive = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(_apName, _apPassword);

  _server.on("/", [this]() {
    _server.send(200, "text/html",
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "<meta name='viewport' content='width=device-width, initial-scale=1'>"
      "<style>"
      "body { font-family: Arial, sans-serif; text-align: center; padding: 30px; background: #f2f2f2; }"
      "form { background: white; display: inline-block; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }"
      "input[type='text'], input[type='password'] { width: 80%; padding: 10px; margin: 10px; border-radius: 5px; border: 1px solid #ccc; }"
      "input[type='submit'] { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }"
      "input[type='submit']:hover { background: #45a049; }"
      "</style>"
      "</head>"
      "<body>"
      "<h2>WiFi Setup</h2>"
      "<form action=\"/save\" method=\"POST\">"
      "SSID:<br><input type=\"text\" name=\"ssid\"><br>"
      "Password:<br><input type=\"password\" name=\"pass\"><br><br>"
      "<input type=\"submit\" value=\"Save\">"
      "</form>"
      "<p><a href=\"/clear\">Clear saved credentials</a></p>"
      "</body>"
      "</html>"
    );
  });

  _server.on("/save", [this]() {
    _ssid = _server.arg("ssid");
    _password = _server.arg("pass");
    saveCredentials(_ssid, _password);
    _server.send(200, "text/html", 
      "<html><body><h1>Credentials Saved!</h1><p>Rebooting and attempting to connect...</p></body></html>");
    delay(2000);
    ESP.restart();
  });

  _server.on("/clear", [this]() {
    clearCredentials();
  });

  _server.begin();
  Serial.println("=======================================");
  Serial.println("AP Mode started!");
  Serial.print("Connect to: ");
  Serial.println(_apName);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Open http://" + WiFi.softAPIP().toString() + " in your browser");
  Serial.println("=======================================");
}

void WiFiConfig::tryConnectToWiFi() {
  Serial.println("Attempting to connect to WiFi with static IP...");
  Serial.print("SSID: ");
  Serial.println(_ssid);
  
  // First try with DHCP to get gateway info
  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid.c_str(), _password.c_str());

  Serial.print("Getting network info via DHCP");
  int dhcpTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && dhcpTimeout < 20) {
    delay(500);
    Serial.print(".");
    dhcpTimeout++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ Failed to get network info via DHCP");
    Serial.println("Starting Configuration Portal...");
    startConfigPortal();
    return;
  }

  // Successfully connected via DHCP, now get gateway and set static IP
  IPAddress gateway = WiFi.gatewayIP();
  Serial.print("\nGateway IP: ");
  Serial.println(gateway);

  // Use static IP 192.168.0.200 (you can change this if needed)
  IPAddress staticIP(192, 168, 0, 200);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns = gateway; // Use gateway as DNS

  Serial.print("Setting static IP to: ");
  Serial.println(staticIP);

  // Disconnect and reconnect with static IP
  WiFi.disconnect();
  delay(1000);

  // Configure static IP
  WiFi.config(staticIP, dns, gateway, subnet);
  
  Serial.println("Reconnecting with static IP...");
  WiFi.begin(_ssid.c_str(), _password.c_str());

  int staticTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && staticTimeout < 20) {
    delay(500);
    Serial.print(".");
    staticTimeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected with static IP!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet: ");
    Serial.println(WiFi.subnetMask());
    _apModeActive = false;
  } else {
    Serial.println("\n❌ Failed to connect with static IP");
    Serial.println("Starting Configuration Portal...");
    startConfigPortal();
  }
}

bool WiFiConfig::isAPModeActive() {
  return _apModeActive;
}
