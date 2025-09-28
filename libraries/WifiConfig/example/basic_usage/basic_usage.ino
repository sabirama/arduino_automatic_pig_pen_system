#include <WiFiConfig.h>

WiFiConfig wifiManager;

void setup() {
  Serial.begin(115200);
  wifiManager.begin();
}

void loop() {
  wifiManager.handleClient();

  if (wifiManager.isConnected()) {
    // Your WiFi-enabled logic here
  }
}
