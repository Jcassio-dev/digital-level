#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ---------- Pinos (RP2040) ----------
#define PIN_OLED_SCK  2    // SCL (clock) do OLED
#define PIN_OLED_MOSI 3    // SDA (dados) do OLED
#define PIN_OLED_CS   4
#define PIN_OLED_DC   5
#define PIN_OLED_RST  6
#define PIN_I2C_SDA   14   // MPU no I2C1
#define PIN_I2C_SCL   15

#define MPU_ADDR 0x68

// ---------- Configuração dos eixos (sensor DEITADO) ----------
#define VERT_AXIS  2       // Z aponta pra cima quando deitado (0=X, 1=Y, 2=Z)
#define VERT_SIGN  1.0f    // -1.0f se o sensor estiver de cabeça pra baixo
#define TILT_AXIS  1       // eixo que muda ao inclinar (troque 1 <-> 0 se medir o lado errado)
#define TILT_SIGN  1.0f    // -1.0f se o sinal sair invertido
const float ANGLE_OFFSET = -0.9f;

#define SCREEN_W 128
#define SCREEN_H 64

Adafruit_SH1106G display(SCREEN_W, SCREEN_H, &SPI, PIN_OLED_DC, PIN_OLED_RST, PIN_OLED_CS);

const float OK_TOL = 0.5f;     // "nivelado" em graus

// ---------- Filtro adaptativo ----------
// erro grande  -> segue rápido (resposta ágil)
// erro pequeno -> suaviza forte (número estável parado)
const float ALPHA_FAST = 0.60f;   // erro > 2.0°
const float ALPHA_MID  = 0.30f;   // erro > 0.7°
const float ALPHA_SLOW = 0.06f;   // erro pequeno (ruído)
const float DEADBAND   = 0.15f;   // só atualiza o número mostrado se mudar mais que isso

float filtAngle = 0;
float shownAngle = 0;

// ---------- MPU direto pelos registradores ----------
bool mpuWrite(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(MPU_ADDR);
  Wire1.write(reg);
  Wire1.write(val);
  return Wire1.endTransmission() == 0;
}

bool readAccel(float a[3]) {
  Wire1.beginTransmission(MPU_ADDR);
  Wire1.write(0x3B);  // ACCEL_XOUT_H
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6) != 6) return false;
  for (int i = 0; i < 3; i++) {
    int16_t v = (Wire1.read() << 8) | Wire1.read();
    a[i] = v / 16384.0f;  // faixa ±2g
  }
  return true;
}

// média rápida de poucas leituras (reduz ruído sem atrasar)
bool readAccelAvg(float out[3], int n = 4) {
  float sum[3] = {0, 0, 0}, a[3];
  int ok = 0;
  for (int i = 0; i < n; i++) {
    if (readAccel(a)) {
      for (int k = 0; k < 3; k++) sum[k] += a[k];
      ok++;
    }
  }
  if (!ok) return false;
  for (int k = 0; k < 3; k++) out[k] = sum[k] / ok;
  return true;
}

float angleFrom(const float a[3]) {
  float vert = VERT_SIGN * a[VERT_AXIS];
  float tilt = TILT_SIGN * a[TILT_AXIS];
  return atan2f(tilt, vert) * 180.0f / PI + ANGLE_OFFSET;
}

void drawGauge(float ang) {
  display.clearDisplay();

  // mostrador à esquerda: marca do 0° no topo e linha que gira
  const int cx = 32, cy = 32, r = 28;
  display.drawCircle(cx, cy, r + 1, SH110X_WHITE);
  display.drawFastVLine(cx, cy - r - 1, 5, SH110X_WHITE);

  float rad = ang * PI / 180.0f;
  int x2 = cx + (int)(sinf(rad) * r);
  int y2 = cy - (int)(cosf(rad) * r);
  display.drawLine(cx, cy, x2, y2, SH110X_WHITE);
  display.drawLine(cx + 1, cy, x2 + 1, y2, SH110X_WHITE);
  display.fillCircle(cx, cy, 3, SH110X_WHITE);

  // número à direita (tamanho 2 para caber com decimal)
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

void setup() {
  Serial.begin(115200);

  Wire1.setSDA(PIN_I2C_SDA);
  Wire1.setSCL(PIN_I2C_SCL);
  Wire1.setClock(400000);   // I2C rápido
  Wire1.begin();

  SPI.setSCK(PIN_OLED_SCK);
  SPI.setTX(PIN_OLED_MOSI);
  SPI.begin();

  if (!display.begin(0, true)) {
    Serial.println("Falha ao iniciar o OLED (SH1106)");
    while (true) delay(100);
  }

  if (!mpuWrite(0x6B, 0x00)) {   // acorda o sensor
    Serial.println("MPU nao responde");
    while (true) delay(100);
  }
  delay(100);
  mpuWrite(0x1C, 0x00);  // acelerômetro ±2g
  mpuWrite(0x1A, 0x03);  // filtro digital ~44 Hz (rápido, o filtro adaptativo cuida do ruído)
  delay(300);

  float a[3];
  if (readAccelAvg(a, 8)) {
    filtAngle = angleFrom(a);
    shownAngle = filtAngle;
  }
}

void loop() {
  float a[3];
  if (readAccelAvg(a)) {
    float raw = angleFrom(a);
    float err = raw - filtAngle;
    float ae = fabsf(err);

    float alpha = ALPHA_SLOW;
    if (ae > 2.0f)      alpha = ALPHA_FAST;
    else if (ae > 0.7f) alpha = ALPHA_MID;

    filtAngle += alpha * err;

    // zona morta: número fica parado com a placa quieta, mas segue rápido ao mover
    if (ae > 0.7f || fabsf(filtAngle - shownAngle) > DEADBAND) {
      shownAngle = filtAngle;
    }
  }

  drawGauge(shownAngle);

  static uint32_t last = 0;
  if (millis() - last > 200) {
    last = millis();
    Serial.print("angulo="); Serial.println(shownAngle, 1);
  }
  delay(5);
}
