#pragma once
#include <Arduino.h>

constexpr uint8_t PIN_OLED_SCK  = 2;
constexpr uint8_t PIN_OLED_MOSI = 3;
constexpr uint8_t PIN_OLED_CS   = 4;
constexpr uint8_t PIN_OLED_DC   = 5;
constexpr uint8_t PIN_OLED_RST  = 6;
// MPU no I2C1 (Wire1): GP14/GP15 são pinos I2C1 válidos e não colidem com o SPI0 do OLED
constexpr uint8_t PIN_I2C_SDA   = 14;
constexpr uint8_t PIN_I2C_SCL   = 15;

constexpr uint8_t MPU_ADDR = 0x68;

constexpr int SCREEN_W = 128;
constexpr int SCREEN_H = 64;
constexpr float OK_TOL = 0.5f;

constexpr int   VERT_AXIS = 2;       // Z aponta pra cima quando deitado (0=X, 1=Y, 2=Z)
constexpr float VERT_SIGN = 1.0f;    // -1.0f se o sensor estiver de cabeça pra baixo
constexpr int   TILT_AXIS = 1;       // eixo que muda ao inclinar (troque 1 <-> 0 se medir o lado errado)
constexpr float TILT_SIGN = 1.0f;    // -1.0f se o sinal sair invertido

constexpr uint32_t CALIB_DURATION_MS = 2000;
constexpr float ANGLE_TRIM = 0.0f;            // ajuste fino manual (graus), somado após a calibração

// ---------- Filtro adaptativo ----------
// erro grande  -> segue rápido (resposta ágil)
// erro pequeno -> suaviza forte (número estável parado)
constexpr float ALPHA_FAST = 0.60f;
constexpr float ALPHA_MID  = 0.30f;
constexpr float ALPHA_SLOW = 0.06f;
constexpr float ERR_FAST   = 2.0f;
constexpr float ERR_MID    = 0.7f;
constexpr float DEADBAND   = 0.15f;   // só atualiza o número mostrado se mudar mais que isso
