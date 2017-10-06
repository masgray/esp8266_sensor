#include "pass.h"

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <Wire.h>

#include <UTFT.h>

extern uint8_t Retro8x16[];
extern uint8_t Arial_round_16x24[];

constexpr int GPIO_RS = 5;
constexpr uint8_t Dht12Address = 0x5c;
constexpr int GPIO_I2C_CLK = 4;
constexpr int GPIO_I2C_DATA = 2;

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

void ConnectToWiFi();
void MqttConnect();
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

uint32_t updated = 0;
uint32_t updatedOld = 0;
uint32_t cycleTime = 0;

constexpr double left = 6;
constexpr double right = 236;
constexpr double top = 125;
constexpr double bottom = 284;

int xPos = left;
int xPosPred = left;
int yPosPred = bottom;

constexpr uint32_t historyDepth = right - left;
float pressureHistory[historyDepth] {};

constexpr uint32_t historyTimeStep = 12*60*60*1000 / historyDepth;
uint32_t historyTimeLastAdded = 0;

uint32_t historyIndex = 0;

void DrawaArrow(float value, float valueR, int x, int y);

void setup()
{
  pinMode(GPIO_RS, OUTPUT);
  
  Serial.begin(115200);
  while (!Serial)
  {
    //Wait
  }
  Wire.begin(GPIO_I2C_DATA, GPIO_I2C_CLK);

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
}

void loop()
{
  static char msg[32];
  static char msg2[64];

  if (!mqttClient.connected()) 
  {
    MqttConnect();
  }
  mqttClient.loop();

  auto current = millis();
  if (current - cycleTime < 1000)
    return;

  cycleTime = current;

  ReadDHT12Sensor();

  if (updated != updatedOld)
  {
    updatedOld = updated;

    tft.setFont(Arial_round_16x24);
    if (temperature != NAN)
    {
      dtostrf(temperature, 3, 0, msg);
      if (temperature >= 0.0)
      {
        char* p = msg;
        while(*p == ' ' && *p != 0)
          ++p;
        sprintf(msg2, "+%s", p);
        tft.print(msg2, 24, 14);
      }
      else
      {
        tft.print(msg, 24, 14);
      }
      DrawaArrow(temperature, temperatureR, 100, 19);
    }

    if (roomTemperature != NAN)
    {
      dtostrf(roomTemperature, 3, 0, msg);
      if (roomTemperature >= 0.0)
      {
        char* p = msg;
        while(*p == ' ' && *p != 0)
          ++p;
        sprintf(msg2, "+%s", p);
        tft.print(msg2, 24, 42);
      }
      else
      {
        tft.print(msg, 24, 42);
      }
      DrawaArrow(roomTemperature, roomTemperatureR, 100, 47);
    }

    if (humidity != NAN)
    {
      dtostrf(humidity, 3, 0, msg);
      tft.print(msg, 152, 14);
      DrawaArrow(humidity, humidityR, 216, 19);
    }
  
    if (roomHumidity != NAN)
    {
      dtostrf(roomHumidity, 3, 0, msg);
      tft.print(msg, 152, 42);
      DrawaArrow(roomHumidity, roomHumidityR, 216, 47);
    }
  
    if (pressure != NAN)
    {
      dtostrf(pressure, 4, 0, msg);
      tft.print(msg, 60, 64+26);
      DrawaArrow(pressure, pressureR, 146, 67+26);
      DrawChart();
    }
  }

  uint32_t dt = (current - updatedOld)/1000;
  if (dt < 100)
    sprintf(msg, "%d ", dt);
  else
    sprintf(msg, "-");
  tft.setFont(Retro8x16);
  tft.print(msg, 198, 300);
}

void ConnectToWiFi()
{
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println("...");

  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("WiFi FAULT!");
    return;
  }
  Serial.println("WiFi connected.");
  Serial.println(WiFi.localIP());
}

void MqttConnect()
{
  Serial.println("Connecting to MQTT server...");
  if (!mqttClient.connect(deviceId))//, mqtt_user, mqtt_passw
  {
    Serial.println("Could not connect to MQTT server");
    return;
  }
  Serial.println("Connected to MQTT server");

  if (!mqttClient.subscribe(Device1Sensor1))
  {
    Serial.print("Could not subscribe to topic: ");
    Serial.println(Device1Sensor1);
    return;
  }
  if (!mqttClient.subscribe(Device1Sensor2))
  {
    Serial.print("Could not subscribe to topic: ");
    Serial.println(Device1Sensor1);
    return;
  }
  if (!mqttClient.subscribe(Device1Sensor3))
  {
    Serial.print("Could not subscribe to topic: ");
    Serial.println(Device1Sensor1);
    return;
  }
  if (!mqttClient.subscribe(Device1Error))
  {
    Serial.print("Could not subscribe to topic: ");
    Serial.println(Device1Sensor1);
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

void DrawaArrow(float value, float valueR, int x, int y)
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
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());

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

bool ReadI2C(uint8_t deviceAdress, uint8_t registerAdress, uint8_t* buffer, size_t toRead)
{
  Wire.beginTransmission(deviceAdress);
  Wire.write(registerAdress);
  Wire.endTransmission();
  auto n = Wire.requestFrom(deviceAdress, static_cast<uint8_t>(toRead));
  if (n != toRead)
    return false;
  for (int i = 0; i < toRead; ++i)
    buffer[i] = Wire.read();
  return true;
}

bool ReadDHT12Sensor(void)
{
    static uint8_t buf[5] = {0};
    if (!ReadI2C(Dht12Address, 0, &buf[0], 5))
    {
      Serial.println("Error reading DHT12...");
      return false;
    }
    static char s[32] = {0};
    uint8_t crc = 0;
    for (int i = 0; i < 4; ++i)
       crc += buf[i];
    if (crc != buf[4])
    {
        sprintf(s, "DHT12 data: %02d %02d %02d %02d %02d", buf[0], buf[1], buf[2], buf[3], buf[4]);
        Serial.println(s);
        Serial.println("CRC wrong!");
        return false;
    }
    roomTemperaturePred = roomTemperature;
    roomHumidityPred = roomHumidity;
    
    roomTemperature = buf[2] + (float)buf[3] / 10;
    roomHumidity = buf[0] + (float)buf[1] / 10;
    
    CalcSred(roomTemperature, roomTemperaturePred, roomTemperatureR, roomTemperatureK);
    CalcSred(roomHumidity, roomHumidityPred, roomHumidityR, roomHumidityK);
    
    return true;
}

