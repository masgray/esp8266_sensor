#include "remote_sensors.h"
#include "display.h"
#include "configuration.h"
#include "consts.h"
#include "pass.h"

//https://bblanchon.github.io/ArduinoJson/
#include <ArduinoJson.h>

#include <ESP8266HTTPClient.h>

constexpr const char *ApiOpenWeatherMapOrgHost PROGMEM = "api.openweathermap.org";
constexpr const char *ApiOpenWeatherMapOrgForecast1 PROGMEM = "/data/2.5/forecast?q=";
constexpr const char *ApiOpenWeatherMapOrgForecast2 PROGMEM = "&units=metric&cnt=10&APPID=";
constexpr const char *ApiOpenWeatherMapOrgCurrent1 PROGMEM = "/data/2.5/weather?q=";
constexpr const char *ApiOpenWeatherMapOrgCurrent2 PROGMEM = "&units=metric&APPID=";

constexpr uint32_t historyTimeStep PROGMEM = 12*60*60*1000 / HistoryDepth;

namespace keys
{
  constexpr const char * List PROGMEM = "list";
  constexpr const char * Main PROGMEM = "main";
  constexpr const char * Temp PROGMEM = "temp";
  constexpr const char * Clouds PROGMEM = "clouds";
  constexpr const char * All PROGMEM = "all";
  constexpr const char * Rain PROGMEM = "rain";
  constexpr const char * Rain3h PROGMEM = "3h";
  constexpr const char * Snow PROGMEM = "snow";
  constexpr const char * Wind PROGMEM = "wind";
  constexpr const char * Speed PROGMEM = "speed";
  constexpr const char * Deg PROGMEM = "deg";
}

RemoteSensors::RemoteSensors(Configuration& configuration, Display& display)
  : m_configuration(configuration)
  , m_display(display)
  , m_timerForReadOuterSensors(1000, TimerState::Stopped)
  , m_timerForReadForecast(15*60*1000, TimerState::Started)
  , m_timerForReadCurrentWeather(60*1000, TimerState::Started)
{
}

void RemoteSensors::begin()
{
  m_timerForReadOuterSensors.Start();
  m_timerForReadForecast.Start();
  m_timerForReadCurrentWeather.Start();
}

void RemoteSensors::loop()
{
  auto current = millis();
  
  if (m_timerForReadOuterSensors.IsElapsed())
  {
    if (m_outerSensorsReady[0] || m_outerSensorsReady[1] || m_outerSensorsReady[2])
    {
      m_outerSensorsReady[0] = false;
      m_outerSensorsReady[1] = false;
      m_outerSensorsReady[2] = false;
      m_timeForReadOuterSensors = current;
      ParseMqttData();
    }
  
    if (m_timerForReadForecast.IsElapsed())
    {
      m_timeForReadForecast = current;
      if (ReadWeather(WeatherType::Forecast))
        m_forecastWeatherReady = true;
      else
        m_timerForReadForecast.Reset(TimerState::Started);
    }
  
    if (m_timerForReadCurrentWeather.IsElapsed())
    {
      m_timeForReadCurrentWeather = current;
      if (ReadWeather(WeatherType::Current))
        m_currentWeatherReady = true;
      else
        m_timerForReadCurrentWeather.Reset(TimerState::Started);
    }
  
    Print();
  }
}

bool RemoteSensors::Print()
{
  m_display.SetBigFont();
  if (m_outerTemperature.isGood)
  {
    m_display.DrawNumber(m_outerTemperature.value, 24, 14, true);
    m_display.DrawArrow(m_outerTemperature.value, m_outerTemperature.r, 100, 19);
  }

  if (m_outerHumidity.isGood)
  {
    m_display.DrawNumber(m_outerHumidity.value, 160, 14, false);
    m_display.DrawArrow(m_outerHumidity.value, m_outerHumidity.r, 216, 19);
  }

  if (m_outerPressure.isGood)
  {
    m_display.DrawNumber(m_outerPressure.value, 46, 80, false);
    m_display.DrawArrow(m_outerPressure.value, m_outerPressure.r, 104, 93);
    m_display.DrawChart(m_outerPressureMin, m_outerPressureMax, m_pressureHistory, m_historyIndex);
  }

  if (m_forecastWeatherReady)
  {
    m_forecastWeatherReady = false;
    PrintForecastWeather();
  }

  if (m_currentWeatherReady)
  {
    m_currentWeatherReady = false;
    PrintCurrentWeather();
  }

  auto current = millis();
  uint32_t dt = (current - m_timeForReadOuterSensors) / 1000;
  m_display.PrintLastUpdated(114, 298, dt);

  dt = (current - m_timeForReadCurrentWeather) / 1000;
  m_display.PrintLastUpdated(160, 298, dt);
  
  dt = (current - m_timeForReadForecast)/1000/60;
  m_display.PrintLastUpdated(200, 298, dt);
  return true;
}

void GetForecastJsonParams(const JsonObject& root, int lineNumber, SensorValue& t, SensorValue& clouds, SensorValue& rain, SensorValue& windSpeed, SensorValue& windDirection)
{
  auto& line = root[keys::List][lineNumber];
  t.value = line[keys::Main][keys::Temp];
  t.isGood = t.value != NAN;
  clouds.value = line[keys::Clouds][keys::All];
  clouds.isGood = clouds.value != NAN;
  float f1 = line[keys::Rain][keys::Rain3h];
  float f2 = line[keys::Snow][keys::Rain3h];
  rain.value = (f1 != 0.0 ? f1 : f2);
  rain.isGood = rain.value != NAN;
  windSpeed.value = line[keys::Wind][keys::Speed];
  windSpeed.isGood = windSpeed.value != NAN;
  windDirection.value = line[keys::Wind][keys::Deg];
  windDirection.isGood = windDirection.value != NAN;
}

void GetCurrentWeatherJsonParams(const JsonObject& root, SensorValue& rain, SensorValue& windSpeed, SensorValue& windDirection)
{
  float f1 = root[keys::Rain][keys::Rain3h];
  float f2 = root[keys::Snow][keys::Rain3h];
  rain.value = (f1 != 0.0 ? f1 : f2);
  rain.isGood = rain.value != NAN;
  windSpeed.value = root[keys::Wind][keys::Speed];
  windSpeed.isGood = rain.value != NAN;
  windDirection.value = root[keys::Wind][keys::Deg];
  windDirection.isGood = rain.value != NAN;
}

void RemoteSensors::PrintForecastWeather()
{
  m_display.SetSmallFont();
  if (forecast12h_T.isGood)
  {
    m_display.DrawNumber(forecast12h_T.value, 44, 142, true);
  }
  if (forecast24h_T.isGood)
  {
    m_display.DrawNumber(forecast24h_T.value, 44, 168, true);
  }
  if (forecast12h_Clouds.isGood)
  {
    m_display.DrawNumber(forecast12h_Clouds.value, 94, 142, false);
  }
  if (forecast24h_Clouds.isGood)
  {
    m_display.DrawNumber(forecast24h_Clouds.value, 94, 168, false);
  }
  if (forecast12h_Rain.isGood)
  {
    m_display.DrawNumber(forecast12h_Rain.value, 134, 142, false, 2);
  }
  if (forecast24h_Rain.isGood)
  {
    m_display.DrawNumber(forecast24h_Rain.value, 134, 168, false, 2);
  }
  if (forecast12h_WindSpeed.isGood)
  {
    m_display.DrawNumber(forecast12h_WindSpeed.value, 188, 142, false);
  }
  if (forecast24h_WindSpeed.isGood)
  {
    m_display.DrawNumber(forecast24h_WindSpeed.value, 188, 168, false);
  }
  if (forecast12h_WindDirection.isGood)
  {
    m_display.DrawWind(forecast12h_WindDirection.value, 224, 151);
  }
  if (forecast24h_WindDirection.isGood)
  {
    m_display.DrawWind(forecast24h_WindDirection.value, 224, 177);
  }
}

void RemoteSensors::PrintCurrentWeather()
{
  m_display.SetSmallFont();
  if (current_Rain.isGood)
  {
    m_display.DrawNumber(current_Rain.value, 136, 90, false, 2);
  }
  if (current_WindSpeed.isGood)
  {
    m_display.DrawNumber(current_WindSpeed.value, 188, 90, false);
  }
  if (current_WindDirection.isGood)
  {
    m_display.DrawWind(current_WindDirection.value, 224, 100);
  }
}

bool RemoteSensors::ReadWeather(WeatherType weatherType)
{
  bool ok = false;
  HTTPClient http;

  if (weatherType == WeatherType::Forecast)
    http.begin(ApiOpenWeatherMapOrgHost, 80, String(ApiOpenWeatherMapOrgForecast1) + String(m_configuration.GetApiLocation()) + String(ApiOpenWeatherMapOrgForecast2) + String(MyApiAppID));
  else
    http.begin(ApiOpenWeatherMapOrgHost, 80, String(ApiOpenWeatherMapOrgCurrent1) + String(m_configuration.GetApiLocation()) + String(ApiOpenWeatherMapOrgCurrent2) + String(MyApiAppID));
  
  int httpCode = http.GET();
  
  if (httpCode > 0) 
  {
    if (httpCode == HTTP_CODE_OK) 
    {
      DynamicJsonBuffer jsonBuffer(4096);
      auto response = http.getString();
      JsonObject& root = jsonBuffer.parseObject(response);
      if (root.success())
      {
        if (weatherType == WeatherType::Forecast)
        {
          int cnt = root["cnt"];
          if (cnt == 10)
          {
            GetForecastJsonParams(root, 4, forecast12h_T, forecast12h_Clouds, forecast12h_Rain, forecast12h_WindSpeed, forecast12h_WindDirection);
            GetForecastJsonParams(root, 9, forecast24h_T, forecast24h_Clouds, forecast24h_Rain, forecast24h_WindSpeed, forecast24h_WindDirection);
            ok = true;
          }
        }
        else
        {
           GetCurrentWeatherJsonParams(root, current_Rain, current_WindSpeed, current_WindDirection);
           ok = true;
        }
      }
    }
  } 
  
  http.end();
  return ok;
}

bool StrEq(const char* s1, const char* s2)
{
  if (strlen(s1) != strlen(s2))
    return false;
  return strcmp(s1, s2) == 0;
}

void RemoteSensors::OnDataArrived(const char* topic, const uint8_t* payload, uint32_t length)
{
  static char buffer[1024];
  if (length > 1023)
    length = 1023;
  buffer[length] = 0;
  memcpy(buffer, payload, length);
  String text(buffer);

  if (StrEq(topic, Device1Sensor1))
  {
    m_payloadMqttSensor1 = text;
    m_outerSensorsReady[0] = true;
  }
  else if (StrEq(topic, Device1Sensor2))
  {
    m_payloadMqttSensor2 = text;
    m_outerSensorsReady[1] = true;
  }
  else if (StrEq(topic, Device1Sensor3))
  {
    m_payloadMqttSensor3 = text;
    m_outerSensorsReady[2] = true;
  }
}

void RemoteSensors::ParseMqttData()
{
  if (m_payloadMqttSensor1.length() != 0)
  {
    if (m_outerTemperature.isGood)
      m_outerTemperature.pred = m_outerTemperature.value;
    m_outerTemperature.value = m_payloadMqttSensor1.toFloat();
    m_payloadMqttSensor1.remove(0);
    m_outerTemperature.isGood = m_outerTemperature.value != NAN;
    CalcAvarage(m_outerTemperature);
  }
  if (m_payloadMqttSensor2.length() != 0)
  {
    if (m_outerHumidity.isGood)
      m_outerHumidity.pred = m_outerHumidity.value;
    m_outerHumidity.value = m_payloadMqttSensor2.toFloat();
    m_payloadMqttSensor2.remove(0);
    m_outerHumidity.isGood = m_outerHumidity.value != NAN;
    CalcAvarage(m_outerHumidity);
  }
  if (m_payloadMqttSensor3.length() != 0)
  {
    if (m_outerPressure.isGood)
      m_outerPressure.pred = m_outerPressure.value;
    m_outerPressure.value = m_payloadMqttSensor3.toFloat();
    m_payloadMqttSensor3.remove(0);
    m_outerPressure.isGood = m_outerPressure.value != NAN;
    CalcAvarage(m_outerPressure);
    AddToHistory();
  }
}

void RemoteSensors::AddToHistory()
{
  auto current = millis();

  if (current - m_historyTimeLastAdded > historyTimeStep || m_historyTimeLastAdded == 0)
  {
    m_historyTimeLastAdded = current;
    if (m_historyIndex >= HistoryDepth)
    {
      memcpy(&m_pressureHistory[0], &m_pressureHistory[1], (HistoryDepth - 1) * sizeof(float));
      m_historyIndex = HistoryDepth - 1;
    }
    if (m_outerPressure.value < m_outerPressureMin)
      m_outerPressureMin = m_outerPressure.value;
    if (m_outerPressure.value > m_outerPressureMax)
      m_outerPressureMax = m_outerPressure.value;
    m_pressureHistory[m_historyIndex] = m_outerPressure.value;
    ++m_historyIndex;
  }
}

