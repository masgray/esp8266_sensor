#pragma once

#include "sensor_value.h"
#include "timer.h"

//https://www.github.com/masgray/dht12
#include <dht12.h>

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

  Dht12 m_roomTHSensor;
  SensorValue m_roomTemperature;
  SensorValue m_roomHumidity;
  SensorValue m_roomLight;

  Timer m_timerForReadSensors;
  Timer m_timerForReadAnalogue;
};


