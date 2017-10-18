#pragma once

#include "sensor_value.h"
#include "timer.h"
#include "network.h"
#include "chart.h"

class Display;

class RemoteSensors: public IMqttConsumer
{
public:
  RemoteSensors(Display& display);

  void begin();
  void loop();

  void OnDataArrived(const char* topic, const uint8_t* payload, uint32_t length) final;

private:
  bool Print();
  void PrintForecastWeather();
  void AddToHistory();

private:
  Display& m_display;
  SensorValue m_outerTemperature;
  SensorValue m_outerHumidity;
  SensorValue m_outerPressure;
  float m_outerPressureMin = 730.0;
  float m_outerPressureMax = 750.0;
  int m_outerSensorErrorCode = 0;

  SensorValue forecast12h_T;
  SensorValue forecast24h_T;
  SensorValue forecast12h_Clouds;
  SensorValue forecast24h_Clouds;
  SensorValue forecast12h_Rain;
  SensorValue forecast24h_Rain;
  SensorValue forecast12h_WindSpeed;
  SensorValue forecast24h_WindSpeed;
  SensorValue forecast12h_WindDirection;
  SensorValue forecast24h_WindDirection;
  SensorValue current_Rain;
  SensorValue current_WindSpeed;
  SensorValue current_WindDirection;
  
  bool m_outerSensorsReady = false;
  bool m_forecastWeatherReady = false;
  bool m_currentWeatherReady = false;

  Timer m_timerForReadForecast;
  Timer m_timerForReadCurrentWeather;
  
  uint32_t m_timeForReadOuterSensors = 0;
  uint32_t m_timeForReadForecast = 0;
  uint32_t m_timeForReadCurrentWeather = 0;

  float m_pressureHistory[HistoryDepth]{};
  uint32_t m_historyTimeLastAdded = 0;
  uint32_t m_historyIndex = 0;
};

