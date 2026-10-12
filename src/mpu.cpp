#include "mpu.h"
#include <Wire.h>
#include "config.h"

constexpr uint8_t REG_WHO_AM_I     = 0x75;
constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_CONFIG       = 0x1A;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;

constexpr uint8_t ACCEL_RANGE_2G = 0x00;
constexpr uint8_t DLPF_44HZ      = 0x03;
constexpr float   ACCEL_LSB_PER_G = 16384.0f;
constexpr int     AVG_SAMPLES = 4;

static float angleZero = 0.0f;
static float filtAngle = 0.0f;
static float shownAngle = 0.0f;

static bool mpuWrite(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(MPU_ADDR);
  Wire1.write(reg);
  Wire1.write(val);
  return Wire1.endTransmission() == 0;
}

// Leitura direta pelos registradores: a Adafruit_MPU6050 recusa clones cujo
// WHO_AM_I é diferente do original, mesmo o sensor funcionando normalmente.
static bool readAccel(float a[3]) {
  Wire1.beginTransmission(MPU_ADDR);
  Wire1.write(REG_ACCEL_XOUT_H);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6) != 6) return false;
  for (int i = 0; i < 3; i++) {
    int16_t v = (Wire1.read() << 8) | Wire1.read();
    a[i] = v / ACCEL_LSB_PER_G;
  }
  return true;
}

static bool readAccelAvg(float out[3], int n = AVG_SAMPLES) {
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

static float rawAngle(const float a[3]) {
  float vert = VERT_SIGN * a[VERT_AXIS];
  float tilt = TILT_SIGN * a[TILT_AXIS];
  return atan2f(tilt, vert) * 180.0f / PI;
}

bool mpuInit() {
  // Wire1 porque os pinos escolhidos (GP14/GP15) pertencem ao I2C1
  Wire1.setSDA(PIN_I2C_SDA);
  Wire1.setSCL(PIN_I2C_SCL);
  Wire1.setClock(400000);
  Wire1.begin();

  if (!mpuWrite(REG_PWR_MGMT_1, 0x00)) {
    Serial.println("MPU nao responde");
    return false;
  }
  delay(100);
  Wire1.beginTransmission(MPU_ADDR);
  Wire1.write(REG_WHO_AM_I);
  if (Wire1.endTransmission(false) == 0 && Wire1.requestFrom((uint8_t)MPU_ADDR, (uint8_t)1) == 1) {
    Serial.print("WHO_AM_I=0x"); Serial.println(Wire1.read(), HEX);
  }
  mpuWrite(REG_ACCEL_CONFIG, ACCEL_RANGE_2G);
  // filtro digital ~44 Hz (rápido, o filtro adaptativo cuida do ruído)
  mpuWrite(REG_CONFIG, DLPF_44HZ);
  delay(300);
  return true;
}

bool mpuCalibrate() {
  float sum = 0;
  int n = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < CALIB_DURATION_MS) {
    float a[3];
    if (readAccelAvg(a)) {
      sum += rawAngle(a);
      n++;
    }
    delay(5);
  }
  if (!n) return false;

  angleZero = sum / n;
  filtAngle = shownAngle = 0.0f;
  return true;
}

// Filtro adaptativo: um filtro fixo ou fica lento ao mover (alpha baixo) ou
// fica tremendo parado (alpha alto). Aqui o alpha depende do erro: movimento
// grande segue rápido, ruído pequeno é suavizado forte.
float mpuUpdate() {
  float a[3];
  if (readAccelAvg(a)) {
    float raw = rawAngle(a) - angleZero + ANGLE_TRIM;
    float err = raw - filtAngle;
    float ae = fabsf(err);

    float alpha = ALPHA_SLOW;
    if (ae > ERR_FAST)     alpha = ALPHA_FAST;
    else if (ae > ERR_MID) alpha = ALPHA_MID;

    filtAngle += alpha * err;

    // Zona morta: número fica parado com a placa quieta, mas segue rápido ao mover
    if (ae > ERR_MID || fabsf(filtAngle - shownAngle) > DEADBAND) {
      shownAngle = filtAngle;
    }
  }
  return shownAngle;
}
