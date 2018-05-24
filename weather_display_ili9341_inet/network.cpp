#include "network.h"
#include "configuration.h"
#include "display.h"
#include "consts.h"
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

Network::Network(Configuration& configuration, Display& display, RunState* runState)
  : m_configuration(configuration)
  , m_display(display)
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
  
  if (m_dnsServer)
    m_dnsServer->processNextRequest();

  if (m_isReset)
  {
    delay(1000);
    ESP.restart();
    delay(5000);
  }
}

bool Network::Connected()
{
  return (m_wifiMode == WiFiMode::AccessPointMode ? true : WiFi.waitForConnectResult() == WL_CONNECTED);
  //WiFi.status() == WL_CONNECTED
}

Network::WiFiMode Network::ConnectToWiFi()
{
  m_display.PrintError(Connecting, VGA_LIME);
  WiFi.disconnect(true);
  WiFi.enableAP(false);
  WiFi.mode(WIFI_STA);

  WiFi.begin(m_configuration.GetApName(), m_configuration.GetPassw());
  String s = ConnectedStr;
  if (Connected())
  {
    s += WiFi.localIP().toString();
    m_display.PrintError(s.c_str(), VGA_LIME);
    m_wifiMode = WiFiMode::ClientMode;
    return m_wifiMode;
  }
  
  WiFi.enableAP(true);
  WiFi.mode(WIFI_AP);
  s += WiFi.softAPIP().toString();
  m_display.PrintError(s.c_str());
  m_wifiMode = WiFiMode::AccessPointMode;
  m_dnsServer.reset(new DNSServer());
  m_dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  m_dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
  return m_wifiMode;
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

