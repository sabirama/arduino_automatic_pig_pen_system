#include <ESP8266WebServer.h>
#include <WiFiConfig.h>
#include <ServoControl.h>
#include <ScaleModule.h>
#include <PinControl.h>
#include <SMSModule.h>
#include <TimeStore.h>
#include "SystemServer.h"

WiFiConfig wifiManager;
ESP8266WebServer server(80);

ServoControl feedGate(D4);
ScaleModule feedScale(D2, D3);
PinControl relayPump(D5);
SMSModule sim800l(D6, D7);
TimeStore feedSchedules;
TimeStore washSchedules;

SystemServer systemServer(server, feedGate, feedScale, relayPump, feedSchedules, washSchedules, wifiManager);

void setup() {
    Serial.begin(115200);
    feedGate.begin();
    feedScale.begin();
    relayPump.begin();
    sim800l.begin();
    feedSchedules.begin();
    washSchedules.begin();
    wifiManager.begin();

    if (!SPIFFS.begin()) Serial.println("SPIFFS mount failed");

    server.serveStatic("/", SPIFFS, "/index.html");

    systemServer.begin();
}

void loop() {
    wifiManager.handleClient();
    systemServer.handleClient();
}

