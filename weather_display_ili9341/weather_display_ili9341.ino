#include "pass.h"
#include "timer.h"

#include <dht12.h>

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <UTFT.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

extern uint8_t Retro8x16[];
extern uint8_t Arial_round_16x24[];

constexpr int GPIO_RS = 5;

constexpr int GPIO_I2C_DATA = 2;
constexpr int GPIO_I2C_CLK = 4;

Dht12 sensor(GPIO_I2C_DATA, GPIO_I2C_CLK);
UTFT tft(ILI9341_S5P, NOTINUSE, NOTINUSE, GPIO_RS);

extern unsigned short Background[];

constexpr const char* MqttServer PROGMEM= "192.168.0.3";
constexpr const uint16_t MqttPort PROGMEM= 1883;
constexpr const char *prefix PROGMEM= "unit1";
constexpr const char *deviceId PROGMEM= "unit1_device2";
constexpr const char *Device1Sensor1 PROGMEM= "unit1/device1/sensor1/status";
constexpr const char *Device1Sensor2 PROGMEM= "unit1/device1/sensor2/status";
constexpr const char *Device1Sensor3 PROGMEM= "unit1/device1/sensor3/status";
constexpr const char *Device1Error PROGMEM= "unit1/device1/error/status";
constexpr const char *Device1Hello PROGMEM= "unit1/device1/hello/status";

constexpr const char *ApiOpenWeatherMapOrgHost PROGMEM= "api.openweathermap.org";
constexpr const char *ApiOpenWeatherMapOrgForecast PROGMEM= "/data/2.5/forecast?q=Moscow,ru&units=metric&cnt=10&APPID=";
constexpr const char *ApiOpenWeatherMapOrgCurrent PROGMEM= "/data/2.5/weather?q=Moscow,ru&units=metric&APPID=";

constexpr const char* HostName PROGMEM= "EspWeatherStationDisplay";

void OnMessageArrived(char* topic, byte* payload, unsigned int length);

WiFiClient wifiClient;
PubSubClient mqttClient(MqttServer, MqttPort, OnMessageArrived, wifiClient);

float temperature = NAN;
float humidity = NAN;
float pressure = NAN;
float pressureMin = 730.0;
float pressureMax = 750.0;
float temperaturePred = NAN;
float humidityPred = NAN;
float pressurePred = NAN;
float temperatureR = NAN;
float humidityR = NAN;
float pressureR = NAN;
uint32_t temperatureK = 0;
uint32_t humidityK = 0;
uint32_t pressureK = 0;
int errorCode = 0;

float roomTemperature = NAN;
float roomTemperaturePred = NAN;
float roomTemperatureR = NAN;
uint32_t roomTemperatureK = 0;
float roomHumidity = NAN;
float roomHumidityPred = NAN;
float roomHumidityR = NAN;
uint32_t roomHumidityK = 0;
float forecast12h_T = NAN;
float forecast24h_T = NAN;
float forecast12h_Clouds = NAN;
float forecast24h_Clouds = NAN;
float forecast12h_Rain = NAN;
float forecast24h_Rain = NAN;
float forecast12h_WindSpeed = NAN;
float forecast24h_WindSpeed = NAN;
float forecast12h_WindDirection = NAN;
float forecast24h_WindDirection = NAN;
float current_Rain = NAN;
float current_WindSpeed = NAN;
float current_WindDirection = NAN;

uint32_t updated = 0;
uint32_t updatedOld = 0;
uint32_t cycleTime = 0;
uint32_t timeForReadForecast = 0;

constexpr double left = 6;
constexpr double right = 236;
constexpr double top = 199;
constexpr double bottom = 284;

int xPos = left;
int xPosPred = left;
int yPosPred = bottom;

constexpr uint32_t historyDepth = right - left;
float pressureHistory[historyDepth] {};

constexpr uint32_t historyTimeStep = 12*60*60*1000 / historyDepth;
uint32_t historyTimeLastAdded = 0;

uint32_t historyIndex = 0;

bool runCycle = true;

Timer timerMainCycle(1000, TimerState::Started);
Timer timerForReadForecast(15*60*1000, TimerState::Started);
Timer timerForReadCurrentWeather(60*1000, TimerState::Started);

struct Vector
{
  float x;
  float y;
};

enum WeatherType
{
  Forecast,
  Current
};

void ConnectToWiFi();
void MqttConnect();
void DrawaArrow(float value, float valueR, int x, int y);
void DrawNumber(float number, int x, int y, bool withPlus = false, int charsWidth = 3, int precision = 0);
void DrawWind(float windDir, int x, int y);
bool ReadWeather(WeatherType weatherType);
void PrintCurrentWeather();

void setup()
{
  pinMode(GPIO_RS, OUTPUT);
  
  sensor.begin();

  tft.InitLCD(PORTRAIT);
  tft.clrScr();
  tft.setColor(VGA_WHITE);
  tft.setBackColor(38, 84, 120);

  int displayWidth = tft.getDisplayXSize();
  int displayHeight = tft.getDisplayYSize();

  tft.drawBitmap(0, 0, displayWidth, displayHeight, Background);

  ConnectToWiFi();
  updatedOld = updated = millis();
  MqttConnect();

  timerForReadForecast.Start();
  timerForReadCurrentWeather.Start();
  timerMainCycle.Start();
}

void loop()
{
  static char msg[32];
  static char msg2[64];

  ArduinoOTA.handle();

  if (!runCycle)
  {
    yield();
    return;
  }
  
  auto current = millis();
  
  if (!mqttClient.connected()) 
  {
    MqttConnect();
  }
  mqttClient.loop();

  if (!runCycle || !timerMainCycle.IsElapsed())
  {
    yield();
    return;
  }

  cycleTime = current;

  if (runCycle && timerForReadForecast.IsElapsed())
  {
    timeForReadForecast = current;
    if (ReadWeather(WeatherType::Forecast))
      PrintForecastWeather();
    else
      timerForReadForecast.Reset(TimerState::Started);
  }

  if (runCycle && timerForReadCurrentWeather.IsElapsed())
  {
    if (ReadWeather(WeatherType::Current))
      PrintCurrentWeather();
    else
      timerForReadCurrentWeather.Reset(TimerState::Started);
  }

  if (runCycle)
    ReadDHT12Sensor();

  if (runCycle && updated != updatedOld)
  {
    updatedOld = updated;

    tft.setFont(Arial_round_16x24);
    if (temperature != NAN)
    {
      DrawNumber(temperature, 24, 14, true);
      DrawArrow(temperature, temperatureR, 100, 19);
    }

    if (roomTemperature != NAN)
    {
      DrawNumber(roomTemperature, 24, 42, true);
      DrawArrow(roomTemperature, roomTemperatureR, 100, 47);
    }

    if (humidity != NAN)
    {
      dtostrf(humidity, 3, 0, msg);
      tft.print(msg, 152, 14);
      DrawArrow(humidity, humidityR, 216, 19);
    }
  
    if (roomHumidity != NAN)
    {
      dtostrf(roomHumidity, 3, 0, msg);
      tft.print(msg, 152, 42);
      DrawArrow(roomHumidity, roomHumidityR, 216, 47);
    }
  
    if (pressure != NAN)
    {
      DrawNumber(pressure, 46, 80, false);
      DrawArrow(pressure, pressureR, 104, 93);
      DrawChart();
    }
  }

  if (runCycle)
  {
    uint32_t dt = (current - updatedOld)/1000;
    if (dt < 100)
      sprintf(msg, "%d ", dt);
    else
      sprintf(msg, "-  ");
    tft.setFont(Retro8x16);
    tft.print(msg, 114, 300);
  
    dt = (current - timeForReadForecast)/1000/60;
    if (dt < 100)
      sprintf(msg, "%d ", dt);
    else
      sprintf(msg, "-  ");
    tft.print(msg, 177, 300);
  }
}

void ConnectToWiFi()
{
  WiFi.disconnect(true);

  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    return;
  }

  WiFi.hostname(HostName);
  ArduinoOTA.setHostname(HostName);
  
  ArduinoOTA.onStart([](){
    runCycle = false;
  });
  ArduinoOTA.begin();
}

void MqttConnect()
{
  if (!mqttClient.connect(deviceId))//, mqtt_user, mqtt_passw
  {
    return;
  }

  if (!mqttClient.subscribe(Device1Sensor1))
  {
    return;
  }
  if (!mqttClient.subscribe(Device1Sensor2))
  {
    return;
  }
  if (!mqttClient.subscribe(Device1Sensor3))
  {
    return;
  }
  if (!mqttClient.subscribe(Device1Error))
  {
    return;
  }
}

bool StrEq(const char* s1, const char* s2)
{
  if (strlen(s1) != strlen(s2))
    return false;
  return strcmp(s1, s2) == 0;
}

void CalcSred(float value, float valuePred, float& valueR, uint32_t& k)
{
  if (valuePred == NAN)
    return;
  if (k == 1)
  {
    valueR = (value + valuePred)/2;
  }
  else if (k > 2)
  {
    float n = (k*valueR + value)/((k + 1)*valueR);
    valueR *= n;
  }
  ++k;
}

void OnMessageArrived(char* topic, byte* payload, unsigned int length)
{
  updated = millis();

  static char buffer[1024];
  if (length > 1023)
    length = 1023;
  buffer[length] = 0;
  memcpy(buffer, payload, length);
  String text(buffer);

  if (StrEq(topic, Device1Sensor1))
  {
    temperaturePred = temperature;
    temperature = text.toFloat();
    CalcSred(temperature, temperaturePred, temperatureR, temperatureK);
  }
  else if (StrEq(topic, Device1Sensor2))
  {
    humidityPred = humidity;
    humidity = text.toFloat();
    CalcSred(humidity, humidityPred, humidityR, humidityK);
  }
  else if (StrEq(topic, Device1Sensor3))
  {
    pressurePred = pressure;
    pressure = text.toFloat();
    CalcSred(pressure, pressurePred, pressureR, pressureK);
    AddToHistory();
  }
  else if (StrEq(topic, Device1Error))
  {
    errorCode = text.toInt();
  }
}

void DrawUp(int x, int y)
{
  DrawStable(x, y);
  tft.drawLine(x, y, x, y + 8);
  tft.drawLine(x, y, x - 2, y + 4);
  tft.drawLine(x, y, x + 2, y + 4);
}

void DrawDown(int x, int y)
{
  DrawStable(x, y);
  tft.drawLine(x, y, x, y + 8);
  tft.drawLine(x, y + 8, x - 2, y + 4);
  tft.drawLine(x, y + 8, x + 2, y + 4);
}

void DrawStable(int x, int y)
{
  tft.setColor(38, 84, 120);
  tft.fillRect(x-2, y, x+2, y + 8);
  tft.setColor(VGA_WHITE);
}

void DrawArrow(float value, float valueR, int x, int y)
{
  if (valueR == NAN)
    return;
  if (value > valueR)
    DrawUp(x, y);
  else if (value < valueR)
    DrawDown(x, y);
  else
    DrawStable(x, y);
}

int CalcY(float p)
{
  double yPos = bottom - (bottom - top) / (pressureMax - pressureMin) * (p - pressureMin);
  return yPos;
}

void DrawChart()
{
  tft.setColor(38, 84, 120);
  tft.fillRect(left, top, right, bottom);
  tft.setColor(VGA_LIME);

  if (historyIndex < 2)
  {
    tft.drawPixel(left, CalcY(pressureHistory[0]));
  }
  else
  {
    for (int i = 1; i < historyIndex; ++i)
    {
      tft.drawLine(left + i - 1, CalcY(pressureHistory[i - 1]), left + i, CalcY(pressureHistory[i]));
    }
  }
  tft.setColor(VGA_WHITE);
}

void AddToHistory()
{
  auto current = millis();

  if (current - historyTimeLastAdded > historyTimeStep || historyTimeLastAdded == 0)
  {
    historyTimeLastAdded = current;
    if (historyIndex >= historyDepth)
    {
      memcpy(&pressureHistory[0], &pressureHistory[1], (historyDepth - 1) * sizeof(int));
      historyIndex = historyDepth - 1;
    }
    if (pressure < pressureMin)
      pressureMin = pressure;
    if (pressure > pressureMax)
      pressureMax = pressure;
    pressureHistory[historyIndex] = pressure;
    ++historyIndex;
  }
}

bool ReadDHT12Sensor(void)
{
    roomTemperaturePred = roomTemperature;
    roomHumidityPred = roomHumidity;

    if (!sensor.read(roomTemperature, roomHumidity))
      return false;

    CalcSred(roomTemperature, roomTemperaturePred, roomTemperatureR, roomTemperatureK);
    CalcSred(roomHumidity, roomHumidityPred, roomHumidityR, roomHumidityK);
    
    return true;
}

void DrawNumber(float number, int x, int y, bool withPlus, int charsWidth, int precision)
{
  static char msg[32];
  static char msg2[32];
  dtostrf(number, charsWidth, precision, msg);
  char* p = msg;
  while(*p == ' ' && *p != 0)
    ++p;
  if (withPlus && number >= 0.0)
    sprintf(msg2, "+%s ", p);
  else
    sprintf(msg2, "%s ", p);
  tft.print(msg2, x, y);
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

void PrintForecastWeather(void)
{
  static char msg[32];
  tft.setFont(Retro8x16);
  if (forecast12h_T != NAN)
  {
    DrawNumber(forecast12h_T, 44, 142, true);
  }
  if (forecast24h_T != NAN)
  {
    DrawNumber(forecast24h_T, 44, 168, true);
  }
  if (forecast12h_Clouds != NAN)
  {
    DrawNumber(forecast12h_Clouds, 94, 142, false);
  }
  if (forecast24h_Clouds != NAN)
  {
    DrawNumber(forecast24h_Clouds, 94, 168, false);
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
    DrawNumber(forecast12h_WindSpeed, 188, 142, false);
  }
  if (forecast24h_WindSpeed != NAN)
  {
    DrawNumber(forecast24h_WindSpeed, 188, 168, false);
  }
  if (forecast12h_WindDirection != NAN)
  {
    DrawWind(forecast12h_WindDirection, 224, 151);
  }
  if (forecast24h_WindDirection != NAN)
  {
    DrawWind(forecast24h_WindDirection, 224, 177);
  }
}

void PrintCurrentWeather()
{
  tft.setFont(Retro8x16);
  if (current_Rain != NAN)
  {
    DrawNumber(current_Rain, 136, 90, false, 3, 2);
  }
  if (current_WindSpeed != NAN)
  {
    DrawNumber(current_WindSpeed, 188, 90, false);
  }
  if (current_WindDirection != NAN)
  {
    DrawWind(current_WindDirection, 224, 100);
  }
}

void ClearWindArrow(int x, int y)
{
  tft.setColor(38, 84, 120);
  tft.fillRect(x - 8, y - 8, x + 8, y + 8);
  tft.setColor(VGA_WHITE);
}

void DrawWindArrow(int x, int y, Vector point1, Vector point2, Vector point3, Vector point4)
{
  ClearWindArrow(x, y);
  tft.drawLine(x + point1.x, y + point1.y, x + point2.x, y + point2.y);
  tft.drawLine(x + point1.x, y + point1.y, x + point3.x, y + point3.y);
  tft.drawLine(x + point1.x, y + point1.y, x + point4.x, y + point4.y);
}

void Rotate(Vector& point, float angle)
{
  Vector rotatedPoint;
  float rad = angle * 2 * PI / 360;
  rotatedPoint.x = point.x * cos(rad) + point.y * sin(rad);
  rotatedPoint.y = point.x * sin(rad) - point.y * cos(rad);
  point = rotatedPoint;
}

void DrawWind(float windDir, int x, int y)
{
  Vector point1{0, -8};
  Rotate(point1, windDir);
  Vector point2{0, 8};
  Rotate(point2, windDir);
  Vector point3{-2, -2};
  Rotate(point3, windDir);
  Vector point4{2, -2};
  Rotate(point4, windDir);
  DrawWindArrow(x, y, point1, point2, point3, point4);
}

bool ReadWeather(WeatherType weatherType)
{
  bool ok = false;
  HTTPClient http;

  if (weatherType == WeatherType::Forecast)
    http.begin(ApiOpenWeatherMapOrgHost, 80, String(ApiOpenWeatherMapOrgForecast) + String(ApiAppID));
  else
    http.begin(ApiOpenWeatherMapOrgHost, 80, String(ApiOpenWeatherMapOrgCurrent) + String(ApiAppID));
  
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


