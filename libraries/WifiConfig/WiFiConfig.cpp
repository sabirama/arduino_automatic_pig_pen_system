#include "WiFiConfig.h"

WiFiConfig::WiFiConfig(const char* apName, const char* apPassword, 
                       int eepromStart, int eepromSize)
  : _apName(apName), _apPassword(apPassword), 
    _eepromStart(eepromStart), _eepromSize(eepromSize) {}

void WiFiConfig::begin() {
  loadCredentials();
  delay(500);

  if (_ssid.length() > 0) {
    tryConnectToWiFi();
  } else {
    Serial.println("No saved WiFi credentials. Starting AP mode...");
    startAPMode();
  }
}

void WiFiConfig::handleClient() {
  // Web server handling moved to main sketch
}

bool WiFiConfig::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

IPAddress WiFiConfig::getIP() {
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    return WiFi.softAPIP();
  }
  return WiFi.localIP();
}

void WiFiConfig::clearCredentials() {
  for (int i = 0; i < _eepromSize; i++) {
    EEPROM.write(_eepromStart + i, 0);
  }
  EEPROM.commit();
}

void WiFiConfig::enableConcurrentMode(bool enable) {
  _concurrentMode = enable;
  Serial.println("Concurrent mode: " + String(enable ? "ENABLED" : "DISABLED"));
}

bool WiFiConfig::isConcurrentMode() {
  return _concurrentMode;
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

void WiFiConfig::startAPMode() {
  if (_concurrentMode && WiFi.status() == WL_CONNECTED) {
    WiFi.mode(WIFI_AP_STA);
    Serial.println("Concurrent mode: AP + Station active");
  } else {
    WiFi.mode(WIFI_AP);
    Serial.println("AP mode only");
  }
  
  WiFi.softAP(_apName, _apPassword);
  
  Serial.println("=======================================");
  Serial.println("AP Mode Started!");
  Serial.print("Connect to: ");
  Serial.println(_apName);
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Station IP Address: ");
    Serial.println(WiFi.localIP());
  }
  
  Serial.println("Open http://" + WiFi.softAPIP().toString() + " in your browser");
  Serial.println("=======================================");
}

IPAddress generateStaticIP(const IPAddress& gateway) {
  // Generate a static IP in the same subnet as the gateway
  // Use the gateway's first three octets and set the last octet to 200
  // Example: gateway 192.168.1.1 -> static IP 192.168.1.200
  return IPAddress(gateway[0], gateway[1], gateway[2], 200);
}

bool isIPInSameSubnet(const IPAddress& ip1, const IPAddress& ip2, const IPAddress& subnet) {
  for (int i = 0; i < 4; i++) {
    if ((ip1[i] & subnet[i]) != (ip2[i] & subnet[i])) {
      return false;
    }
  }
  return true;
}

void WiFiConfig::tryConnectToWiFi() {
  Serial.println("Attempting to connect to WiFi...");
  Serial.print("SSID: ");
  Serial.println(_ssid);
  
  // Set mode based on concurrent mode setting
  if (_concurrentMode) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(_apName, _apPassword);
    Serial.println("Concurrent mode: Starting AP alongside Station");
  } else {
    WiFi.mode(WIFI_STA);
  }
  
  // Just connect with DHCP
  WiFi.begin(_ssid.c_str(), _password.c_str());

  Serial.print("Connecting via DHCP");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected via DHCP!");
    Serial.print("Station IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet: ");
    Serial.println(WiFi.subnetMask());

    if (_concurrentMode) {
      Serial.print("AP IP Address: ");
      Serial.println(WiFi.softAPIP());
      Serial.println("Web server accessible on BOTH IP addresses");
    }
  } else {
    Serial.println("\n❌ Failed to connect to WiFi");
    Serial.println("Starting AP Mode...");
    startAPMode();
  }
}


bool WiFiConfig::isAPModeActive() {
  return WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
}
