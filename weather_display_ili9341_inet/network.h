#pragma once

#include "run_state.h"

#include <ESP8266WiFi.h>

class Configuration;
class Display;

class Network
{
public:
  Network(Display& display, RunState* runState);
  ~Network();

  void begin();
  void loop();
  bool Connected();
  bool ConnectedNoWait();

private:
  bool ConnectToWiFi();
  void Connect();

private:
  Display& m_display;
  RunState* m_runState;
  
  WiFiClient m_wifiClient;

  bool m_isReset = false;
};


