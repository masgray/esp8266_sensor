#include "pass.h"

#include <PubSubClient.h>
#include <ESP8266WiFi.h>

#include <UTFT.h>

extern uint8_t Retro8x16[];
extern uint8_t Arial_round_16x24[];

constexpr int GPIO_RS = 5;

UTFT tft(ILI9341_S5P, NOTINUSE, NOTINUSE, GPIO_RS);

extern unsigned short Background[];

constexpr const char* MqttServer PROGMEM= "192.168.0.3";
constexpr const uint16_t MqttPort PROGMEM= 1883;
constexpr const char *prefix PROGMEM= "unit1";
constexpr const char *deviceId PROGMEM= "unit1_device2";
constexpr const char *Sensor1 PROGMEM= "unit1/device1/sensor1/status";
constexpr const char *Sensor2 PROGMEM= "unit1/device1/sensor2/status";
constexpr const char *Sensor3 PROGMEM= "unit1/device1/sensor3/status";
constexpr const char *Error PROGMEM= "unit1/device1/error/status";
constexpr const char *Hello PROGMEM= "unit1/device1/hello/status";

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

void DrawSred(float value, float valueR, int x, int y);

void setup()
{
  pinMode(GPIO_RS, OUTPUT);
  
  Serial.begin(115200);
  while (!Serial)
  {
    //Wait
  }

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

  if (!mqttClient.connected()) 
  {
    MqttConnect();
  }
  mqttClient.loop();

  auto current = millis();
  if (current - cycleTime < 1000)
    return;

  cycleTime = current;
    
  if (updated != updatedOld)
  {
    updatedOld = updated;

    tft.setFont(Arial_round_16x24);
    if (temperature != NAN)
    {
      dtostrf(temperature, 4, 1, msg);
      tft.print(msg, 24, 28);
      DrawSred(temperature, temperatureR, 110, 33);
    }

    if (humidity != NAN)
    {
      dtostrf(humidity, 3, 0, msg);
      tft.print(msg, 152, 28);
      DrawSred(humidity, humidityR, 226, 33);
    }
  
    if (pressure != NAN)
    {
      dtostrf(pressure, 4, 0, msg);
      tft.print(msg, 60, 64+26);
      DrawSred(pressure, pressureR, 146, 67+26);
      DrawChart();
    }
  }

  sprintf(msg, "%d ", (current - updatedOld)/1000);
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
  Serial.println("Connecting to MQTT server");
  if (!mqttClient.connect(deviceId))//, mqtt_user, mqtt_passw
  {
    Serial.println("Could not connect to MQTT server");
    return;
  }
  Serial.println("Connected to MQTT server");
  mqttClient.subscribe(prefix); // for receiving HELLO messages from IoT Manager
  mqttClient.subscribe(Sensor1);
  mqttClient.subscribe(Sensor2);
  mqttClient.subscribe(Sensor3);
  mqttClient.subscribe(Error);
  mqttClient.subscribe(Hello);
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
  if (StrEq(topic, Hello))
  {
    Serial.print("HELLO from server received, reconnect counter: ");
    Serial.println(text);
  }
  else if (StrEq(topic, Sensor1))
  {
    temperaturePred = temperature;
    temperature = text.toFloat();
    CalcSred(temperature, temperaturePred, temperatureR, temperatureK);
  }
  else if (StrEq(topic, Sensor2))
  {
    humidityPred = humidity;
    humidity = text.toFloat();
    CalcSred(humidity, humidityPred, humidityR, humidityK);
  }
  else if (StrEq(topic, Sensor3))
  {
    pressurePred = pressure;
    pressure = text.toFloat();
    CalcSred(pressure, pressurePred, pressureR, pressureK);
    AddToHistory();
  }
  else if (StrEq(topic, Error))
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

void DrawSred(float value, float valueR, int x, int y)
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

