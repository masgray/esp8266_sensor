#pragma once

#include <ESP8266WiFi.h>

//https://github.com/me-no-dev/ESPAsyncWebServer
#include <ESPAsyncWebServer.h>

#include "configuration.h"
#include "network.h"
#include "display.h"
#include "local_sensors.h"
#include "remote_sensors.h"

class Application: public RunState
{
public:
  Application();

  void begin();
  void loop();

  void SetWebIsRoot(AsyncWebServerRequest* request);
  void SetWebIsNotFound(AsyncWebServerRequest* request);
  void SetWebIsSave(AsyncWebServerRequest* request);

private:
  void WebHandleRoot();
  void WebHandleNotFound();
  void WebHandleSave();

private:
  Configuration m_configuration;
  Network m_network;
  Display m_display;
  LocalSensors m_localSensors;
  RemoteSensors m_remoteSensors;
  bool m_isRun = true;
  AsyncWebServer m_webServer;
  bool m_webIsRoot = false;
  bool m_webIsNotFound = false;
  bool m_webIsSave = false;
  AsyncWebServerRequest* m_requestRoot = nullptr;
  AsyncWebServerRequest* m_requestNotFound = nullptr;
  AsyncWebServerRequest* m_requestSave = nullptr;
};


