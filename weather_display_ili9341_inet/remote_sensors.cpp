#include "remote_sensors.h"
#include "display.h"
#include "configuration.h"
#include "pass.h"

#include <ESP8266HTTPClient.h>

#include <ctime>

constexpr const char *ApiOpenWeatherMapOrgHost PROGMEM = "api.openweathermap.org";
constexpr const char *ApiOpenWeatherMapOrgForecast1 PROGMEM = "/data/2.5/forecast?q=";
constexpr const char *ApiOpenWeatherMapOrgForecast2 PROGMEM = "&units=metric&cnt=17&APPID=";
constexpr const char *ApiOpenWeatherMapOrgCurrent1 PROGMEM = "/data/2.5/weather?q=";
constexpr const char *ApiOpenWeatherMapOrgCurrent2 PROGMEM = "&units=metric&APPID=";

constexpr uint32_t historyTimeStep PROGMEM = 12 * 60 * 60 * 1000 / HistoryDepth;

namespace keys
{
constexpr const char* List PROGMEM = "list";
constexpr const char* Main PROGMEM = "main";
constexpr const char* Temp PROGMEM = "temp";
constexpr const char* Clouds PROGMEM = "clouds";
constexpr const char* All PROGMEM = "all";
constexpr const char* Rain PROGMEM = "rain";
constexpr const char* Rain3h PROGMEM = "3h";
constexpr const char* Snow PROGMEM = "snow";
constexpr const char* Wind PROGMEM = "wind";
constexpr const char* Speed PROGMEM = "speed";
constexpr const char* Deg PROGMEM = "deg";
constexpr const char* Pressure PROGMEM = "pressure";
constexpr const char* Humidity PROGMEM = "humidity";
constexpr const char* Dt PROGMEM = "dt";
}

RemoteSensors::RemoteSensors(Display& display)
  : m_display(display)
  , m_timerForReadForecast(15 * 60 * 1000, TimerState::Started)
  , m_timerForReadCurrentWeather(60 * 1000, TimerState::Started)
{
}

void RemoteSensors::begin()
{
  m_timerForReadCurrentWeather.Start();
  m_timerForReadForecast.Start();
}

void RemoteSensors::loop()
{
  auto current = millis();

  if (m_timerForReadCurrentWeather.IsElapsed())
  {
    m_timeForReadCurrentWeather = current;
    if (ReadWeather(WeatherType::Current))
      m_currentWeatherReady = true;
    else
      m_timerForReadCurrentWeather.Reset(TimerState::Started);
  }

  if (m_timerForReadForecast.IsElapsed())
  {
    m_timeForReadForecast = current;
    if (ReadWeather(WeatherType::Forecast))
      m_forecastWeatherReady = true;
    else
      m_timerForReadForecast.Reset(TimerState::Started);
  }

  Print();
}

bool RemoteSensors::Print()
{
  if (m_forecastWeatherReady)
  {
    m_forecastWeatherReady = false;
    PrintForecastWeather();
  }

  if (m_currentWeatherReady)
  {
    m_currentWeatherReady = false;

    if (m_currentDateTime)
    {
      auto dt = m_currentDateTime + 3 * 60 * 60;
      auto* tm = gmtime(reinterpret_cast<const time_t*>(&dt));
      char buffer[16]{};
      snprintf(buffer, 16, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
      m_display.SetSmallFont();
      m_display.DrawText(buffer, 80, 298);
    }
    
    PrintCurrentWeather();
  }

  auto current = millis();

  uint32_t dt = (current - m_timeForReadCurrentWeather) / 1000;
  m_display.PrintLastUpdated(160, 298, dt);

  dt = (current - m_timeForReadForecast) / 1000 / 60;
  m_display.PrintLastUpdated(200, 298, dt);
  return true;
}

bool ForecastFindDateTime(const JsonObject& root, int linesCount, uint32_t currentDateTime, int& line12h, int& line18h)
{
  //12:00:00 MSK = 09:00:00 UTC, 18:00:00 MSK = 15:00:00 UTC
  if (!currentDateTime)
    return false;

  time_t zero = 0;
  auto* ptr = gmtime(&zero);
  if (!ptr)
    return false;
  auto tmZero = *ptr;
  
  uint32_t t = currentDateTime;
  ptr = gmtime(reinterpret_cast<const time_t*>(&currentDateTime));
  if (!ptr)
    return false;
  auto ts = *ptr;
  
  if (ts.tm_hour > 3)
  {
    tmZero.tm_mday += 1;
    t += mktime(&tmZero);
  }
  ptr = gmtime(reinterpret_cast<const time_t*>(&t));
  if (!ptr)
    return false;
  ts = *ptr;

  ts.tm_hour = 9;
  ts.tm_min = 0;
  ts.tm_sec = 0;
  auto t12h = mktime(&ts);
  ts.tm_hour = 15;
  auto t18h = mktime(&ts);

  bool found12h = false;
  bool found18h = false;
  for (int i = 0; i < linesCount; ++i)
  {
    auto& line = root[keys::List][i];
    if (!line)
      return false;
      
    uint32_t dt = line[keys::Dt];
    if (dt == t12h)
    {
      line12h = i;
      found12h = true;
    }
    else if (dt == t18h)
    {
      line18h = i;
      found18h = true;
    }
  }
  return found12h && found18h;
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

void RemoteSensors::GetCurrentWeatherJsonParams(const JsonObject& root, SensorValue& rain, SensorValue& windSpeed, SensorValue& windDirection)
{
  m_currentDateTime = root[keys::Dt];

  float f1 = root[keys::Rain][keys::Rain3h];
  float f2 = root[keys::Snow][keys::Rain3h];
  rain.value = (f1 != 0.0 ? f1 : f2);
  rain.isGood = rain.value != NAN;
  windSpeed.value = root[keys::Wind][keys::Speed];
  windSpeed.isGood = rain.value != NAN;
  windDirection.value = root[keys::Wind][keys::Deg];
  windDirection.isGood = rain.value != NAN;

  m_outerTemperature.pred = m_outerTemperature.value;
  m_outerTemperature.value = root[keys::Main][keys::Temp];
  m_outerTemperature.isGood = m_outerTemperature.value != NAN;
  CalcAvarage(m_outerTemperature);

  m_outerHumidity.pred = m_outerHumidity.value;
  m_outerHumidity.value = root[keys::Main][keys::Humidity];
  m_outerHumidity.isGood = m_outerHumidity.value != NAN;
  CalcAvarage(m_outerHumidity);

  m_outerPressure.pred = m_outerPressure.value;
  m_outerPressure.value = root[keys::Main][keys::Pressure];
  m_outerPressure.isGood = m_outerPressure.value != NAN;
  m_outerPressure.value /= 1.33322387415;
  CalcAvarage(m_outerPressure);
  AddToHistory();
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
    http.begin(ApiOpenWeatherMapOrgHost, 80, String(ApiOpenWeatherMapOrgForecast1) + String(configuration::ApiLocation) + String(ApiOpenWeatherMapOrgForecast2) + String(MyApiAppID));
  else
    http.begin(ApiOpenWeatherMapOrgHost, 80, String(ApiOpenWeatherMapOrgCurrent1) + String(configuration::ApiLocation) + String(ApiOpenWeatherMapOrgCurrent2) + String(MyApiAppID));

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
          int line12h = 4;
          int line18h = 9;
          if (ForecastFindDateTime(root, root["cnt"], m_currentDateTime, line12h, line18h))
          {
            GetForecastJsonParams(root, line12h, forecast12h_T, forecast12h_Clouds, forecast12h_Rain, forecast12h_WindSpeed, forecast12h_WindDirection);
            GetForecastJsonParams(root, line18h, forecast24h_T, forecast24h_Clouds, forecast24h_Rain, forecast24h_WindSpeed, forecast24h_WindDirection);
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

