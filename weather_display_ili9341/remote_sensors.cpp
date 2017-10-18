#include "remote_sensors.h"

#include <ArduinoJson.h>

constexpr const char *ApiOpenWeatherMapOrgHost PROGMEM= "api.openweathermap.org";
constexpr const char *ApiOpenWeatherMapOrgForecast1 PROGMEM= "/data/2.5/forecast?q=";
constexpr const char *ApiOpenWeatherMapOrgForecast2 PROGMEM= "&units=metric&cnt=10&APPID=";
constexpr const char *ApiOpenWeatherMapOrgCurrent1 PROGMEM= "/data/2.5/weather?q=";
constexpr const char *ApiOpenWeatherMapOrgCurrent2 PROGMEM= "&units=metric&APPID=";

enum WeatherType
{
  Forecast,
  Current
};

RemoteSensors::RemoteSensors(Display& display)
  : m_display(display)
  , m_timerForReadForecast(15*60*1000, TimerState::Started)
  , m_timerForReadCurrentWeather(60*1000, TimerState::Started)
{
}

void RemoteSensors::begin()
{
  m_timerForReadForecast.Start();
  m_timerForReadCurrentWeather.Start();
}

void RemoteSensors::loop()
{
  auto current = millis();
  
  if (m_outerSensorsReady)
  {
    m_outerSensorsReady = false;
    m_timeForReadOuterSensors = current;
  }

  if (timerForReadForecast.IsElapsed())
  {
    m_timeForReadForecast = current;
    if (ReadWeather(WeatherType::Forecast))
      m_forecastWeatherReady = true;
    else
      timerForReadForecast.Reset(TimerState::Started);
  }

  if (timerForReadCurrentWeather.IsElapsed())
  {
    m_timeForReadCurrentWeather = current;
    if (ReadWeather(WeatherType::Current))
      m_currentWeatherReady = true;
    else
      timerForReadCurrentWeather.Reset(TimerState::Started);
  }

  Print();
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
    m_display.DrawNumber(m_outerHumidity.value, 152, 14, false);
    m_display.DrawArrow(m_outerHumidity.value, m_outerHumidity.r, 216, 19);
  }

  if (m_outerPressure.isGood)
  {
    m_display.DrawNumber(m_outerPressure.value, 46, 80, false);
    m_display.DrawArrow(m_outerPressure.value, m_outerPressure.r, 104, 93);
    m_display.DrawChart(left, right, top, bottom);
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
  uint32_t dt = current - m_timeForReadOuterSensors;
  m_display.PrintLastUpdated(114, 300, dt);

  dt = current - m_timeForReadCurrentWeather;
  m_display.PrintLastUpdated(160, 300, dt);
  
  dt = (current - m_timeForReadForecast)/1000/60;
  m_display.PrintLastUpdated(200, 300, dt);
  return true;
}

void GetForecastJsonParams(const JsonObject& root, int lineNumber, float& t, float& clouds, float& rain, float& windSpeed, float& windDirection)
{
  auto& line = root["list"][lineNumber];
  t = line["main"]["temp"];
  clouds = line["clouds"]["all"];
  rain = line["rain"]["3h"];
  windSpeed = line["wind"]["speed"];
  windDirection = line["wind"]["deg"];
}

void GetCurrentWeatherJsonParams(const JsonObject& root, float& rain, float& windSpeed, float& windDirection)
{
  rain = root["rain"]["3h"];
  windSpeed = root["wind"]["speed"];
  windDirection = root["wind"]["deg"];
}

void RemoteSensors::PrintForecastWeather()
{
  static char msg[32];
  m_display.SetSmallFont();
  if (forecast12h_T != NAN)
  {
    m_display.DrawNumber(forecast12h_T, 44, 142, true);
  }
  if (forecast24h_T != NAN)
  {
    m_display.DrawNumber(forecast24h_T, 44, 168, true);
  }
  if (forecast12h_Clouds != NAN)
  {
    m_display.DrawNumber(forecast12h_Clouds, 94, 142, false);
  }
  if (forecast24h_Clouds != NAN)
  {
    m_display.DrawNumber(forecast24h_Clouds, 94, 168, false);
  }
  if (forecast12h_Rain != NAN)
  {
    dtostrf(forecast12h_Rain, 3, 2, msg);
    tft.print(msg, 134, 142);
  }
  if (forecast24h_Rain != NAN)
  {
    dtostrf(forecast24h_Rain, 3, 2, msg);
    tft.print(msg, 134, 168);
  }
  if (forecast12h_WindSpeed != NAN)
  {
    m_display.DrawNumber(forecast12h_WindSpeed, 188, 142, false);
  }
  if (forecast24h_WindSpeed != NAN)
  {
    m_display.DrawNumber(forecast24h_WindSpeed, 188, 168, false);
  }
  if (forecast12h_WindDirection != NAN)
  {
    m_display.DrawWind(forecast12h_WindDirection, 224, 151);
  }
  if (forecast24h_WindDirection != NAN)
  {
    m_display.DrawWind(forecast24h_WindDirection, 224, 177);
  }
}

void PrintCurrentWeather()
{
  m_display.SetSmallFont();
  if (current_Rain != NAN)
  {
    m_display.DrawNumber(current_Rain, 136, 90, false, 3, 2);
  }
  if (current_WindSpeed != NAN)
  {
    m_display.DrawNumber(current_WindSpeed, 188, 90, false);
  }
  if (current_WindDirection != NAN)
  {
    m_display.DrawWind(current_WindDirection, 224, 100);
  }
}

bool ReadWeather(WeatherType weatherType)
{
  bool ok = false;
  HTTPClient http;

  if (weatherType == WeatherType::Forecast)
    http.begin(ApiOpenWeatherMapOrgHost, 80, String(ApiOpenWeatherMapOrgForecast1) + String(ApiLocation) + String(ApiOpenWeatherMapOrgForecast2) + String(ApiAppID));
  else
    http.begin(ApiOpenWeatherMapOrgHost, 80, String(ApiOpenWeatherMapOrgCurrent1) + String(ApiLocation) + String(ApiOpenWeatherMapOrgCurrent2) + String(ApiAppID));
  
  int httpCode = http.GET();
  
  if(httpCode > 0) 
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
    if (m_outerTemperature.isGood)
      m_outerTemperature.pred = m_outerTemperature.value;
    m_outerTemperature.value = text.toFloat();
    m_outerTemperature.isGood = m_outerTemperature.value != NAN;
    CalcAvarage(m_outerTemperature);
    m_outerSensorsReady = true;
  }
  else if (StrEq(topic, Device1Sensor2))
  {
    if (m_outerHumidity.isGood)
      m_outerHumidity.pred = m_outerHumidity.value;
    m_outerHumidity.value = text.toFloat();
    m_outerHumidity.isGood = m_outerHumidity.value != NAN;
    CalcAvarage(m_outerHumidity);
    m_outerSensorsReady = true;
  }
  else if (StrEq(topic, Device1Sensor3))
  {
    if (m_outerPressure.isGood)
      m_outerPressure.pred = m_outerPressure.value;
    m_outerPressure.value = text.toFloat();
    m_outerPressure.isGood = m_outerPressure.value != NAN;
    CalcAvarage(m_outerPressure);
    AddToHistory();
    m_outerSensorsReady = true;
  }
  else if (StrEq(topic, Device1Error))
  {
    m_outerSensorErrorCode = text.toInt();
  }
}

void RemoteSensors::AddToHistory()
{
  constexpr uint32_t historyTimeStep PROGMEM= 12*60*60*1000 / historyDepth;

  auto current = millis();

  if (current - m_historyTimeLastAdded > historyTimeStep || m_historyTimeLastAdded == 0)
  {
    m_historyTimeLastAdded = current;
    if (m_historyIndex >= historyDepth)
    {
      memcpy(&m_pressureHistory[0], &m_pressureHistory[1], (historyDepth - 1) * sizeof(int));
      m_historyIndex = historyDepth - 1;
    }
    if (m_outerPressure.value < m_outerPressureMin)
      m_outerPressure = m_outerPressure.value;
    if (m_outerPressure.value > m_outerPressureMax)
      m_outerPressureMax = m_outerPressure.value;
    m_pressureHistory[m_historyIndex] = m_outerPressure.value;
    ++m_historyIndex;
  }
}

