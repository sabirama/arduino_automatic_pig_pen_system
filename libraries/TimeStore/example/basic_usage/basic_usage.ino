#include <TimeStore.h>

TimeStore timeStore;

void setup() {
  Serial.begin(115200);
  timeStore.begin();

  timeStore.clearTimes();

  timeStore.addTime("08:30");
  timeStore.addTime("12:45");
  timeStore.addTime("23:59");

  timeStore.printTimes();

  timeStore.removeTime("12:45");

  timeStore.printTimes();
}

void loop() {
  // Nothing here
}

