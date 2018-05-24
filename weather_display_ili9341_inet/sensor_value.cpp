#include "sensor_value.h"

void CalcAvarage(SensorValue& sensorValue)
{
  if (!sensorValue.isGood)
    return;
  if (sensorValue.k == 1)
  {
    sensorValue.r = (sensorValue.value + sensorValue.pred)/2;
  }
  else if (sensorValue.k > 2)
  {
    float n = (sensorValue.k * sensorValue.r + sensorValue.value) / ((sensorValue.k + 1) * sensorValue.r);
    sensorValue.r *= n;
  }
  ++sensorValue.k;
}

