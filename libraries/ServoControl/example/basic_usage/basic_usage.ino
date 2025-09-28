#include <ServoControl.h>

ServoControl myServo(D5);

void setup() {
  Serial.begin(115200);
  myServo.begin();
  Serial.println("Servo ready");
  myServo.open();
  delay(2000);
  myServo.close();
}

void loop() {
  // Call myServo.open() or myServo.close() as needed
}

