#pragma once

#include "run_state.h"

#include <ESP8266WiFi.h>

//http://pubsubclient.knolleary.net
#include <PubSubClient.h>

class Configuration;
class Display;

class IMqttConsumer
{
public:
  virtual void OnDataArrived(const char* topic, const uint8_t* payload, uint32_t length) = 0;
};

class Network
{
public:
  Network(Configuration& configuration, Display& display, RunState* runState, IMqttConsumer* mqttConsumer);
  ~Network();

  void begin();
  void loop();
  bool Connected();

private:
  bool ConnectToWiFi();
  bool Connect();
  bool MqttConnect();

private:
  Configuration& m_configuration;
  Display& m_display;
  RunState* m_runState;
  IMqttConsumer* m_mqttConsumer;
  
  WiFiClient m_wifiClient;
  PubSubClient m_mqttClient;

  bool m_isAp = false;
};


