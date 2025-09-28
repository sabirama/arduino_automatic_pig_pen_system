#include "ServoControl.h"

ServoControl::ServoControl(uint8_t pin) : _pin(pin) {}

void ServoControl::begin() {
  _servo.attach(_pin);
  close(); // start closed by default
}

void ServoControl::open() {
  _servo.write(180);
}

void ServoControl::close() {
  _servo.write(0);
}

void ServoControl::write(int angle) {
  _servo.write(angle);
}

