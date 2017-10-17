#include "timer.h"
#include "sensor_value.h"
#include "application.h"

#include <dht12.h>

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <UTFT.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include <DNSServer.h>
#include <WiFiManager.h>
#include <FS.h>

Application app;

extern uint8_t Retro8x16[];
extern uint8_t Arial_round_16x24[];

constexpr int GPIO_RS = 5;

constexpr int GPIO_I2C_DATA = 2;
constexpr int GPIO_I2C_CLK = 4;

Dht12 roomTHSensor(GPIO_I2C_DATA, GPIO_I2C_CLK);
UTFT tft(ILI9341_S5P, NOTINUSE, NOTINUSE, GPIO_RS);

extern unsigned short Background[];

constexpr const char *DefaultApPassword PROGMEM= "DefaultPassword0001";

constexpr const size_t MqttServerSize PROGMEM= 32;
static char MqttServer[MqttServerSize] = "192.168.0.3";

constexpr const size_t MqttPortStrSize PROGMEM= 6;
static char MqttPortStr[MqttPortStrSize] = "1883";

constexpr const size_t ApiAppIDSize PROGMEM= 33;
static char ApiAppID[ApiAppIDSize] = "";

constexpr const size_t ApiLocationSize PROGMEM= 33;
static char ApiLocation[ApiLocationSize] = "Moscow,ru";

constexpr const uint16_t MqttDefaultPort PROGMEM= 1883;

constexpr const char *prefix PROGMEM= "unit1";
constexpr const char *deviceId PROGMEM= "unit1_device2";
constexpr const char *Device1Sensor1 PROGMEM= "unit1/device1/sensor1/status";
constexpr const char *Device1Sensor2 PROGMEM= "unit1/device1/sensor2/status";
constexpr const char *Device1Sensor3 PROGMEM= "unit1/device1/sensor3/status";
constexpr const char *Device1Error PROGMEM= "unit1/device1/error/status";
constexpr const char *Device1Hello PROGMEM= "unit1/device1/hello/status";

constexpr const char *ApiOpenWeatherMapOrgHost PROGMEM= "api.openweathermap.org";
constexpr const char *ApiOpenWeatherMapOrgForecast1 PROGMEM= "/data/2.5/forecast?q=";
constexpr const char *ApiOpenWeatherMapOrgForecast2 PROGMEM= "&units=metric&cnt=10&APPID=";
constexpr const char *ApiOpenWeatherMapOrgCurrent1 PROGMEM= "/data/2.5/weather?q=";
constexpr const char *ApiOpenWeatherMapOrgCurrent2 PROGMEM= "&units=metric&APPID=";

constexpr const char* HostName PROGMEM= "EspWeatherStationDisplay";

void OnMessageArrived(char* topic, byte* payload, unsigned int length);

WiFiManager wifiManager;
WiFiManagerParameter parameterMqttServer("server", "MQTT server", MqttServer, MqttServerSize);
WiFiManagerParameter parameterMqttPort("port", "MQTT port", MqttPortStr, MqttPortStrSize);
WiFiManagerParameter parameterApiAppID("ApiAppId", "APPID for api.openweathermap.org", ApiAppID, ApiAppIDSize);
WiFiManagerParameter parameterApiLocation("ApiLocation", "Location for api.openweathermap.org in form of \"City,country code\" (Moscow,ru)", ApiLocation, ApiLocationSize);

WiFiClient wifiClient;
PubSubClient mqttClient(MqttServer, MqttDefaultPort, OnMessageArrived, wifiClient);

SensorValue outerTemperature;
SensorValue outerHumidity;
SensorValue outerPressure;
float outerPressureMin = 730.0;
float outerPressureMax = 750.0;
int outerSensorErrorCode = 0;

SensorValue roomTemperature;
SensorValue roomHumidity;

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
bool shouldSaveConfig = false;

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
  app.begin();
  
  pinMode(GPIO_RS, OUTPUT);
  
  roomTHSensor.begin();

  tft.InitLCD(PORTRAIT);
  tft.clrScr();
  tft.setColor(VGA_WHITE);
  tft.setBackColor(38, 84, 120);

  int displayWidth = tft.getDisplayXSize();
  int displayHeight = tft.getDisplayYSize();

  tft.drawBitmap(0, 0, displayWidth, displayHeight, Background);

  if (!ReadConfiguration())
    PrintError("Error reading config!");
    
  ConnectToWiFi();
  updatedOld = updated = millis();
  MqttConnect();

  timerForReadForecast.Start();
  timerForReadCurrentWeather.Start();
  timerMainCycle.Start();
}

void loop()
{
  app.loop();
  yield();

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
    ReadRoomTHSensor();

  if (runCycle && updated != updatedOld)
  {
    updatedOld = updated;

    tft.setFont(Arial_round_16x24);
    if (outerTemperature.isGood)
    {
      DrawNumber(outerTemperature.value, 24, 14, true);
      DrawArrow(outerTemperature.value, outerTemperature.r, 100, 19);
    }

    if (roomTemperature.isGood)
    {
      DrawNumber(roomTemperature.value, 24, 42, true);
      DrawArrow(roomTemperature.value, roomTemperature.r, 100, 47);
    }

    if (outerHumidity.isGood)
    {
      DrawNumber(outerHumidity.value, 152, 14, false);
      DrawArrow(outerHumidity.value, outerHumidity.r, 216, 19);
    }
  
    if (roomHumidity.isGood)
    {
      DrawNumber(roomHumidity.value, 152, 42, false);
      DrawArrow(roomHumidity.value, roomHumidity.r, 216, 47);
    }
  
    if (outerPressure.isGood)
    {
      DrawNumber(outerPressure.value, 46, 80, false);
      DrawArrow(outerPressure.value, outerPressure.r, 104, 93);
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

bool ReadConfiguration()
{
  if (!SPIFFS.begin()) 
    return false;
  
  if (!SPIFFS.exists("/config.json"))
    return false;

  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) 
    return false;

  size_t size = configFile.size();
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) 
    return false;
    
  strcpy(MqttServer, json["MqttServer"]);
  strcpy(MqttPortStr, json["MqttPort"]);
  strcpy(ApiAppID, json["ApiAppID"]);
  strcpy(ApiLocation, json["ApiLocation"]);
  return true;
}

void SaveConfiguration()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["MqttServer"] = MqttServer;
  json["MqttPort"] = MqttPortStr;
  json["ApiAppID"] = ApiAppID;
  json["ApiLocation"] = ApiLocation;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
    return;

  json.printTo(configFile);
  configFile.close();
}

void configModeCallback(WiFiManager *myWiFiManager)
{
  static char msg[64];
  auto ip = WiFi.softAPIP().toString();
  sprintf(msg, "Connect to Wi-Fi network \"%s\"\nOpen page at http://%s\nto configure station.", HostName, ip.c_str());
  PrintError(msg);
}

void saveConfigCallback() 
{
  shouldSaveConfig = true;
}

void ConnectToWiFi()
{
  WiFi.disconnect(true);

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setMinimumSignalQuality(20);
  wifiManager.addParameter(&parameterMqttServer);
  wifiManager.addParameter(&parameterMqttPort);
  wifiManager.addParameter(&parameterApiAppID);
  wifiManager.addParameter(&parameterApiLocation);

  if (!wifiManager.autoConnect(HostName, DefaultApPassword))
  {
    PrintError("Web configuring error!");
    ESP.reset();
    delay(5000);
  }

  strcpy(MqttServer, parameterMqttServer.getValue());
  strcpy(MqttPortStr, parameterMqttPort.getValue());
  strcpy(ApiAppID, parameterApiAppID.getValue());
  strcpy(ApiLocation, parameterApiLocation.getValue());

  if (shouldSaveConfig)
  {
    SaveConfiguration();
  }

//  WiFi.begin(ssid, password);
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
    if (outerTemperature.isGood)
      outerTemperature.pred = outerTemperature.value;
    outerTemperature.value = text.toFloat();
    outerTemperature.isGood = outerTemperature.value != NAN;
    CalcAvarage(outerTemperature);
  }
  else if (StrEq(topic, Device1Sensor2))
  {
    if (outerHumidity.isGood)
      outerHumidity.pred = outerHumidity.value;
    outerHumidity.value = text.toFloat();
    outerHumidity.isGood = outerHumidity.value != NAN;
    CalcAvarage(outerHumidity);
  }
  else if (StrEq(topic, Device1Sensor3))
  {
    if (outerPressure.isGood)
      outerPressure.pred = outerPressure.value;
    outerPressure.value = text.toFloat();
    outerPressure.isGood = outerPressure.value != NAN;
    CalcAvarage(outerPressure);
    AddToHistory();
  }
  else if (StrEq(topic, Device1Error))
  {
    outerSensorErrorCode = text.toInt();
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
  double yPos = bottom - (bottom - top) / (outerPressureMax - outerPressureMin) * (p - outerPressureMin);
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
    if (outerPressure.value < outerPressureMin)
      outerPressureMin = outerPressure.value;
    if (outerPressure.value > outerPressureMax)
      outerPressureMax = outerPressure.value;
    pressureHistory[historyIndex] = outerPressure.value;
    ++historyIndex;
  }
}

bool ReadRoomTHSensor(void)
{
  if (roomTemperature.isGood)
    roomTemperature.pred = roomTemperature.value;
  if (roomHumidity.isGood)
    roomHumidity.pred = roomHumidity.value;

    if (!roomTHSensor.read(roomTemperature.value, roomHumidity.value))
      return false;

    CalcAvarage(roomTemperature);
    CalcAvarage(roomHumidity);
    
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

void PrintError(const char* msg)
{
  tft.setFont(Retro8x16);
  tft.setColor(38, 84, 120);
  tft.fillRect(left, top, right, bottom);
  tft.setColor(VGA_RED);
  tft.print(msg, left + 2, top + 2);
  tft.setColor(VGA_WHITE);
  delay(15000);
}

