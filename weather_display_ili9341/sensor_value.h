#pragma once

struct SensorValue
{
  float value = NAN;
  bool isGood = false;
  float pred = NAN;
  float r = NAN;
  uint32_t k = 0;
};

void CalcAvarage(SensorValue& sensorValue);

