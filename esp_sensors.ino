#include "pass.h"

#include <BME280I2C.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

const char* MqttServer = "192.168.0.3";
const uint16_t MqttPort = 1883;

const char* Device1 = "Device1";
const char* ResetTopic = "Reset";

void MqttCallback(char* topic, byte* payload, unsigned int length);

WiFiClient espClient;
PubSubClient mqtt(MqttServer, MqttPort, MqttCallback, espClient);

static BME280I2C bme;
float temperature = NAN;
float humidity = NAN;
float pressure = NAN;
uint32_t reconnectCounter = 0;

void setup() 
{
  Serial.begin(115200);
  while(!Serial) 
  {} // Wait
  while(!bme.begin())
  {
    Serial.println("Could not find BME280 sensor on I2C bus!");
    delay(1000);
  }

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

  bme.read(pressure, temperature, humidity, true, 2);
  if (pressure == NAN)
  {
    Serial.println("Error reading sensors data!");
    mqtt.publish("Error", "1");
  }
  else
  {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print("°C");
    Serial.print("\t\tHumidity: ");
    Serial.print(humidity);
    Serial.print("% RH");
    Serial.print("\t\tPressure: ");
    Serial.print(pressure);
    Serial.println(" Hg");

    mqtt.publish("Error", "0");
    static char msg[32];
    snprintf(msg, 31, "%0.2f", temperature);
    mqtt.publish("Sensor1", msg);
    snprintf(msg, 31, "%0.2f", humidity);
    mqtt.publish("Sensor2", msg);
    snprintf(msg, 31, "%0.2f", pressure);
    mqtt.publish("Sensor3", msg);
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
      mqtt.publish("Hello", msg);
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

