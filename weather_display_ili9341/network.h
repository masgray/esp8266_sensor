#pragma once

#include "run_state.h"

#include <ESP8266WiFi.h>

//http://pubsubclient.knolleary.net
#include <PubSubClient.h>

//https://github.com/me-no-dev/ESPAsyncWebServer
//git clone https://github.com/me-no-dev/ESPAsyncTCP/
//cd ESPAsyncTCP
//git reset --hard 9b0cc37
#include <ESPAsyncWebServer.h>

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

public:
  enum class WiFiMode
  {
    AccessPointMode,
    ClientMode
  };

public:
  bool IsWiFiAccessPointMode() const { m_wifiMode == WiFiMode::AccessPointMode; }
  
private:
  WiFiMode ConnectToWiFi();
  void Connect();
  bool MqttConnect();
  void OnMqttMessageArrived(char* topic, uint8_t* payload, unsigned int length);

  void SetWebIsRoot(AsyncWebServerRequest* request);
  void SetWebIsNotFound(AsyncWebServerRequest* request);
  void SetWebIsSave(AsyncWebServerRequest* request);
  void WebHandleRoot();
  void WebHandleNotFound();
  void WebHandleSave();

private:
  Configuration& m_configuration;
  Display& m_display;
  RunState* m_runState;
  IMqttConsumer* m_mqttConsumer;
  
  WiFiClient m_wifiClient;
  PubSubClient m_mqttClient;

  WiFiMode m_wifiMode = WiFiMode::ClientMode;

  AsyncWebServer m_webServer;
  bool m_webIsRoot = false;
  bool m_webIsNotFound = false;
  bool m_webIsSave = false;
  AsyncWebServerRequest* m_requestRoot = nullptr;
  AsyncWebServerRequest* m_requestNotFound = nullptr;
  AsyncWebServerRequest* m_requestSave = nullptr;
};


