#include "PinControl.h"

PinControl::PinControl(uint8_t pin) : _pin(pin), _state(false) {}

void PinControl::begin() {
  pinMode(_pin, OUTPUT);
  setHigh();
}

void PinControl::setHigh() {
  digitalWrite(_pin, HIGH);
  _state = true;
}

void PinControl::setLow() {
  digitalWrite(_pin, LOW);
  _state = false;
}

void PinControl::toggle() {
  if (_state) {
    setLow();
  } else {
    setHigh();
  }
}

bool PinControl::isHigh() {
  return _state;
}

bool PinControl::isLow() {
  return !_state;
}

