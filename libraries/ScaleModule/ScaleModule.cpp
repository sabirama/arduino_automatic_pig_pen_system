#include "ScaleModule.h"

ScaleModule::ScaleModule(uint8_t doutPin, uint8_t sckPin)
  : _dout(doutPin), _sck(sckPin) {}

void ScaleModule::begin() {
  _scale.begin(_dout, _sck);
}

void ScaleModule::tare() {
  if (_scale.is_ready()) {
    _scale.tare();
  }
}

long ScaleModule::read() {
  if (_scale.is_ready()) {
    return _scale.read();
  } else {
    return 0; // Or some error code
  }
}

bool ScaleModule::isReady() {
  return _scale.is_ready();
}

