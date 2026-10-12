#pragma once
#include <Arduino.h>

bool mpuInit();

bool mpuCalibrate();

float mpuUpdate();
