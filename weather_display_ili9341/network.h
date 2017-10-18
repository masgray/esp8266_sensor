#pragma once

#include "run_state.h"

#include <WiFiManager.h>
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
  void ConnectToWiFi();
  void Connect();
  void MqttConnect();

private:
  Configuration& m_configuration;
  Display& m_display;
  RunState* m_runState;
  IMqttConsumer* m_mqttConsumer;
  
  WiFiManager m_wifiManager;
  std::unique_ptr<WiFiManagerParameter> m_parameterMqttServer;
  std::unique_ptr<WiFiManagerParameter> m_parameterMqttPort;
  std::unique_ptr<WiFiManagerParameter> m_parameterApiAppID;
  std::unique_ptr<WiFiManagerParameter> m_parameterApiLocation;

  WiFiClient m_wifiClient;
  PubSubClient m_mqttClient;
};


