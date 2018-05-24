#pragma once

#include "run_state.h"

#include <ESP8266WiFi.h>

class Configuration;
class Display;
class DNSServer;

class Network
{
public:
  Network(Configuration& configuration, Display& display, RunState* runState);
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

private:
  Configuration& m_configuration;
  Display& m_display;
  RunState* m_runState;
  
  WiFiClient m_wifiClient;

  WiFiMode m_wifiMode = WiFiMode::ClientMode;

  bool m_isReset = false;

  std::unique_ptr<DNSServer> m_dnsServer;
};


