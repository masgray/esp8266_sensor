#pragma once

#include "sensor_value.h"
#include "timer.h"

//https://www.github.com/masgray/dht12
#include <dht12.h>

class Display;

class LocalSensors
{
public:
  LocalSensors(Display& display);

  void begin();
  void loop();

private:
  bool Read();
  void Print();
  
private:
  Display& m_display;

  Dht12 m_roomTHSensor;
  SensorValue m_roomTemperature;
  SensorValue m_roomHumidity;

  Timer m_timerForReadSensors;
};


