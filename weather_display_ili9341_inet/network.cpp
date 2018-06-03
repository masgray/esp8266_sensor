#include "network.h"
#include "configuration.h"
#include "display.h"
#include "pass.h"

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <pgmspace.h>

constexpr const char* HostName PROGMEM = "EspWeatherStationDisplay";
constexpr const char* Connecting PROGMEM ="Connecting to Wi-Fi...";
constexpr const char* ConnectedStr PROGMEM ="IP: ";
constexpr const char* UpdatingFirmwareStr PROGMEM ="Updating firmware...";
constexpr const char* WiFiConnectionError PROGMEM = "WiFi connection error!";

constexpr uint8_t DNS_PORT PROGMEM = 53;
  
static bool UpdateStarted = false;

Network::Network(Display& display, RunState* runState)
  : m_display(display)
  , m_runState(runState)
{
}

Network::~Network()
{
}

void Network::begin()
{
  Connect();
}

void Network::loop()
{
  if (!Connected())
    Connect();
  ArduinoOTA.handle();
  if (UpdateStarted)
  {
    UpdateStarted = false;
    m_runState->Pause();
    m_display.PrintError(UpdatingFirmwareStr, VGA_LIME);
  }

  if (m_runState->IsStopped())
    return;

  if (!Connected())
    return;
    
  if (m_runState->IsStopped())
    return;
  
  if (m_isReset)
  {
    delay(1000);
    ESP.restart();
    delay(5000);
  }
}

bool Network::Connected()
{
  return (WiFi.waitForConnectResult() == WL_CONNECTED);
}

bool Network::ConnectedNoWait()
{
  return WiFi.status() == WL_CONNECTED;
}

bool Network::ConnectToWiFi()
{
  m_display.PrintError(Connecting, VGA_LIME);
  WiFi.disconnect(true);
  WiFi.enableAP(false);
  WiFi.mode(WIFI_STA);

  WiFi.begin(configuration::ApName, configuration::Passw);
  String s = ConnectedStr;
  if (Connected())
  {
    s += WiFi.localIP().toString();
    m_display.PrintError(s.c_str(), VGA_LIME);
    return true;
  }
  return false;
}

void Network::Connect()
{
  WiFi.hostname(HostName);
  ConnectToWiFi();
  if (!Connected())
  {
    m_display.PrintError(WiFiConnectionError);
    return;
  }

  ArduinoOTA.setHostname(HostName);
  
  ArduinoOTA.onStart([this](){
    UpdateStarted = true;
  });
  ArduinoOTA.begin();
}

