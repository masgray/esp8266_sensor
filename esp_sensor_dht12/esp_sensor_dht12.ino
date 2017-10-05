#include "pass.h"

#include <Wire.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

const char* MqttServer = "192.168.0.3";
const uint16_t MqttPort = 1883;

const char* Device1 = "unit1_device3";
const char* ResetTopic = "unit1/device3/reset/action";
constexpr const char *Sensor1 = "unit1/device3/sensor1/status";
constexpr const char *Sensor2 = "unit1/device3/sensor2/status";
constexpr const char *Error = "unit1/device3/error/status";
constexpr const char *Hello = "unit1/device3/hello/status";

void MqttCallback(char* topic, byte* payload, unsigned int length);

WiFiClient espClient;
PubSubClient mqtt(MqttServer, MqttPort, MqttCallback, espClient);

constexpr uint8_t i2cAddr = 0x5c;

float temperature = NAN;
float humidity = NAN;
uint32_t reconnectCounter = 0;

void setup() 
{
  Serial.begin(115200);
  while(!Serial) 
  {} // Wait

  Wire.begin(5, 4);
  delay(10);
  
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() 
{
  if (!mqtt.connected()) 
  {
    MqttReconnect();
  }
  mqtt.loop();

  if (!ReadDHT12Sensor() || temperature == NAN || humidity == NAN)
  {
    Serial.println("Error reading sensors data!");
    mqtt.publish(Error, "1");
  }
  else
  {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print("'C");
    Serial.print("\t\tHumidity: ");
    Serial.print(humidity);
    Serial.print("% RH");

    mqtt.publish(Error, "0");
    static char msg[32];
    
    dtostrf(temperature, 4, 2, msg);
    mqtt.publish(Sensor1, msg);
    
    dtostrf(humidity, 4, 2, msg);
    mqtt.publish(Sensor2, msg);
  }
  delay(1000);
}

void MqttCallback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) 
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  char* s = new char[length + 1];
  memcpy(s, payload, length);
  s[length+1] = 0;
  if (strlen(s) == strlen(ResetTopic) && strcmp(s, ResetTopic) == 0)
  {
    Serial.println("Going to reboot...");
    delay(1000);
    ESP.reset();
  }
}

void MqttReconnect() 
{
  // Loop until we're reconnected
  while (!mqtt.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(Device1)) 
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      ++reconnectCounter;
      static char msg[32];
      snprintf(msg, 31, "%ld", reconnectCounter);
      mqtt.publish(Hello, msg);
      // ... and resubscribe
      mqtt.subscribe(ResetTopic);
    } 
    else 
    {
      Serial.print("Failed, result=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

bool i2cRead(uint8_t reg, uint8_t* buffer)
{
  Wire.beginTransmission(i2cAddr);
  Wire.write(reg);
  Wire.endTransmission();
  auto n = Wire.requestFrom(i2cAddr, (uint8_t)5);
  if (n != 5)
    return false;
  for (int i = 0; i < 5; ++i)
    buffer[i] = Wire.read();
  return true;
}

bool ReadDHT12Sensor(void)
{
    uint8_t buf[10] = {0};
    if (!i2cRead(0, &buf[0]))
    {
      Serial.println("Error reading DHT12...");
      return false;
    }
    char s[16] = {0};
    uint8_t crc = 0;
    for (int i = 0; i < 4; ++i)
       crc += buf[i];
    if (crc != buf[4])
    {
        sprintf(s, "%02d %02d %02d %02d %02d", buf[0], buf[1], buf[2], buf[3], buf[4]);
        Serial.println(s);
        Serial.println("CRC wrong!");
        delay(3000);
        return false;
    }
    temperature  = buf[2] + (float)buf[3] / 10;
    humidity     = buf[0] + (float)buf[1] / 10;
    return true;
}

