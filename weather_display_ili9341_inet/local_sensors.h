#pragma once

#include "sensor_value.h"
#include "timer.h"

#include <BME280I2C.h>

class Display;
class Configuration;

class LocalSensors
{
public:
  LocalSensors(Configuration& configuration, Display& display);

  void begin();
  void loop();

private:
  bool Read();
  void Print();
  
private:
  Configuration& m_configuration;
  Display& m_display;

  BME280I2C m_roomTHSensor;
  SensorValue m_roomTemperature;
  SensorValue m_roomHumidity;
  SensorValue m_roomLight;

  Timer m_timerForReadSensors;
  Timer m_timerForReadAnalogue;
};


