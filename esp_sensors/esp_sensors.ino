#include "pass.h"

#include <BME280I2C.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

const char* MqttServer = "192.168.0.3";
const uint16_t MqttPort = 1883;

const char* Device1 = "unit1_device1";
const char* ResetTopic = "unit1/device1/reset/action";
constexpr const char *Sensor1 = "unit1/device1/sensor1/status";
constexpr const char *Sensor2 = "unit1/device1/sensor2/status";
constexpr const char *Sensor3 = "unit1/device1/sensor3/status";
constexpr const char *Error = "unit1/device1/error/status";
constexpr const char *Hello = "unit1/device1/hello/status";

void MqttCallback(char* topic, byte* payload, unsigned int length);

WiFiClient espClient;
PubSubClient mqtt(MqttServer, MqttPort, MqttCallback, espClient);

BME280I2C bme;
float temperature = NAN;
float humidity = NAN;
float pressure = NAN;
uint32_t reconnectCounter = 0;

void setup() 
{
  Serial.begin(115200);
  while(!Serial) 
  {} // Wait
  while(!bme.begin(5, 4))
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

  bme.read(pressure, temperature, humidity);
  if (pressure == NAN)
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
    Serial.print("\t\tPressure: ");
    pressure = pressure / 133.3;
    Serial.print(pressure);
    Serial.println(" mm Hg");

    mqtt.publish(Error, "0");
    static char msg[32];
    
    dtostrf(temperature, 4, 2, msg);
    mqtt.publish(Sensor1, msg);
    
    dtostrf(humidity, 4, 2, msg);
    mqtt.publish(Sensor2, msg);
    
    dtostrf(pressure, 4, 1, msg);
    mqtt.publish(Sensor3, msg);
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

