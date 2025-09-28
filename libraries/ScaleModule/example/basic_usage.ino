#include <ScaleModule.h>

ScaleModule scale(D2, D3); // DOUT, SCK

void setup() {
  Serial.begin(115200);
  scale.begin();
  Serial.println("Scale initialized. Call tare() when ready.");
}

void loop() {
  if (scale.isReady()) {
    Serial.print("Raw: ");
    Serial.println(scale.read());
  } else {
    Serial.println("Scale not ready.");
  }

  delay(1000);
}

// You can call scale.tare() when needed, e.g., after button press

