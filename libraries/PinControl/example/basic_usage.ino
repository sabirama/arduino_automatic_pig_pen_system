#include <PinControl.h>

PinControl led(D5);  // Pin D5 on NodeMCU

void setup() {
  Serial.begin(115200);
  led.begin();
  Serial.println("PinControl example started");
}

void loop() {
  led.setHigh();
  delay(1000);
  led.setLow();
  delay(1000);
  led.toggle();
  delay(1000);
}

