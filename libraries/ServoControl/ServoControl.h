#ifndef SERVOCONTROL_H
#define SERVOCONTROL_H

#include <Arduino.h>
#include <Servo.h>

class ServoControl {
  public:
    ServoControl(uint8_t pin);
    void begin();          // Attach servo and set initial position
    void open();           // Move servo to open position (180°)
    void close();          // Move servo to closed position (0°)
    void write(int angle); // Write arbitrary angle to servo
  private:
    Servo _servo;
    uint8_t _pin;
};

#endif

