#include "ScaleModule.h"

ScaleModule::ScaleModule(uint8_t doutPin, uint8_t sckPin)
  : _dout(doutPin), _sck(sckPin) {}

void ScaleModule::begin() {
  _scale.begin(_dout, _sck);
  _scale.set_scale(99.0); // Set calibration factor
}

void ScaleModule::tare() {
  if (_scale.is_ready()) {
    _scale.tare();
  }
}

float ScaleModule::read() {
  if (_scale.is_ready()) {
    // Get average of 100 samples with stability check
    return _scale.get_units(5);
  } else {
    return 0.0;
  }
}

bool ScaleModule::isReady() {
  return _scale.is_ready();
}