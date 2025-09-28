#ifndef PINCONTROL_H
#define PINCONTROL_H

#include <Arduino.h>

class PinControl {
  public:
    PinControl(uint8_t pin);
    void begin();       // Initialize pin as OUTPUT
    void setHigh();     // Set pin HIGH
    void setLow();      // Set pin LOW
    void toggle();      // Toggle pin state
    bool isHigh();      // Check if pin is HIGH
    bool isLow();       // Check if pin is LOW
  private:
    uint8_t _pin;
    bool _state;
};

#endif

