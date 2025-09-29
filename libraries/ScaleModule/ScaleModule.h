#ifndef SCALEMODULE_H
#define SCALEMODULE_H

#include <Arduino.h>
#include <HX711.h>

class ScaleModule {
  public:
    ScaleModule(uint8_t doutPin, uint8_t sckPin);
    void begin();
    void tare();         // Zero the scale manually
    float read();         // Get raw value
    bool isReady();      // Check if HX711 is ready

  private:
    HX711 _scale;
    uint8_t _dout;
    uint8_t _sck;
};

#endif

