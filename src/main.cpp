#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "mpu.h"

void setup() {
  Serial.begin(115200);

  if (!displayInit()) {
    while (true) delay(100);
  }

  if (!mpuInit()) {
    while (true) delay(100);
  }
}

void loop() {
  float ang = mpuUpdate();
  displayDrawGauge(ang);

  static uint32_t last = 0;
  if (millis() - last > 200) {
    last = millis();
    // print(valor, casas): printf com float não funciona neste core
    Serial.print("angulo="); Serial.println(ang, 1);
  }
  delay(5);
}
