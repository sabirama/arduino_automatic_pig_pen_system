#ifndef WIFICONFIG_H
#define WIFICONFIG_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#define WIFI_EEPROM_SIZE 96

class WiFiConfig {
public:
  WiFiConfig(const char* apName = "WiFi_Setup", const char* apPassword = "12345678", 
             int eepromStart = 0, int eepromSize = WIFI_EEPROM_SIZE);
  void begin();
  void handleClient();
  bool isConnected();
  IPAddress getIP();
  void clearCredentials();
  bool isAPModeActive();

private:
  const char* _apName;
  const char* _apPassword;
  int _eepromStart;
  int _eepromSize;
  ESP8266WebServer _server;
  String _ssid;
  String _password;
  bool _apModeActive = false;

  void loadCredentials();
  void saveCredentials(const String& newSsid, const String& newPassword);
  void startConfigPortal();
  void tryConnectToWiFi();
};

#endif
