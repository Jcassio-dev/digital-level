#include "display.h"
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "config.h"

static Adafruit_SH1106G display(SCREEN_W, SCREEN_H, &SPI, PIN_OLED_DC, PIN_OLED_RST, PIN_OLED_CS);

bool displayInit() {
  SPI.setSCK(PIN_OLED_SCK);
  SPI.setTX(PIN_OLED_MOSI);
  SPI.begin();

  if (!display.begin(0, true)) {
    Serial.println("Falha ao iniciar o OLED (SH1106)");
    return false;
  }
  return true;
}

void displayShowCalibrating() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 28);
  display.print("Calibrando...");
  display.display();
}

void displayDrawGauge(float ang) {
  display.clearDisplay();

  const int cx = 32, cy = 32, r = 28;
  display.drawCircle(cx, cy, r + 1, SH110X_WHITE);
  display.drawFastVLine(cx, cy - r - 1, 5, SH110X_WHITE);

  float rad = ang * PI / 180.0f;
  int x2 = cx + (int)(sinf(rad) * r);
  int y2 = cy - (int)(cosf(rad) * r);
  display.drawLine(cx, cy, x2, y2, SH110X_WHITE);
  display.drawLine(cx + 1, cy, x2 + 1, y2, SH110X_WHITE);
  display.fillCircle(cx, cy, 3, SH110X_WHITE);

  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(68, 22);
  if (ang >= 0) display.print("+");
  display.print(ang, 1);

  display.setTextSize(1);
  display.setCursor(72, 48);
  display.print(fabsf(ang) < OK_TOL ? "graus  OK" : "graus");

  display.display();
}
