#ifndef WIFICONFIG_H
#define WIFICONFIG_H

#include <ESP8266WiFi.h>
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
  void enableConcurrentMode(bool enable = true);
  bool isConcurrentMode();
  void saveCredentials(const String& newSsid, const String& newPassword);

private:
  const char* _apName;
  const char* _apPassword;
  int _eepromStart;
  int _eepromSize;
  String _ssid;
  String _password;
  bool _concurrentMode = false;

  void loadCredentials();
  void startAPMode();
  void tryConnectToWiFi();
};

#endif