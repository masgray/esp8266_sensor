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
constexpr const char* ConnectedStr PROGMEM ="IP:";
constexpr const char* UpdatingFirmwareStr PROGMEM ="Updating firmware...";
//constexpr const char* Error PROGMEM ="Web configuring error!";
//constexpr const char* ConfigModeText PROGMEM ="Connect to Wi-Fi network\n\"%s\"\nOpen page at\nhttp://%s\nto configure station.";

//namespace keys
//{
//constexpr const char* MqttServer_name  PROGMEM = "server";
//constexpr const char* MqttServer_descr PROGMEM = "MQTT server";
//constexpr const char* MqttServerPort_name  PROGMEM = "port";
//constexpr const char* MqttServerPort_descr PROGMEM = "MQTT port";
//constexpr const char* ApiLocation_name  PROGMEM = "ApiLocation";
//constexpr const char* ApiLocation_descr PROGMEM = "Location for api.openweathermap.org in form of \"City,country code\" (Moscow,ru)";
//}

//static bool shouldSaveConfig = false;
static bool UpdateStarted = false;

struct NetworkContext
{
  IMqttConsumer* mqttConsumer = nullptr;
};

static NetworkContext s_networkContext;

void OnMqttMessageArrived(char* topic, uint8_t* payload, unsigned int length)
{
  if (s_networkContext.mqttConsumer)
    s_networkContext.mqttConsumer->OnDataArrived(topic, payload, length);
}

Network::Network(Configuration& configuration, Display& display, RunState* runState, IMqttConsumer* mqttConsumer)
  : m_configuration(configuration)
  , m_display(display)
  , m_runState(runState)
  , m_mqttConsumer(mqttConsumer)
  , m_mqttClient(m_wifiClient)
{
  s_networkContext.mqttConsumer = mqttConsumer;
}

Network::~Network()
{
  s_networkContext.mqttConsumer = nullptr;
}

void Network::begin()
{
  m_isAp = !Connect();

  m_mqttClient.setServer(m_configuration.GetMqttServer(), m_configuration.GetMqttPort());
  m_mqttClient.setCallback(OnMqttMessageArrived);
  if (!m_isAp)
    MqttConnect();
}

void Network::loop()
{
  if (!Connected() && !m_isAp)
    m_isAp= Connect();
  ArduinoOTA.handle();
  if (UpdateStarted)
  {
    UpdateStarted = false;
    m_runState->Pause();
    m_display.PrintError(UpdatingFirmwareStr, 1, VGA_LIME);
  }

  if (m_runState->IsStopped())
    return;

  if (!Connected())
    return;
    
  if (!m_mqttClient.connected())
  {
    MqttConnect();
  }

  if (m_runState->IsStopped())
    return;
  m_mqttClient.loop();
}

bool Network::Connected()
{
  return WiFi.waitForConnectResult() == WL_CONNECTED;
  //return WiFi.status() == WL_CONNECTED;
}

//void saveConfigCallback() 
//{
//  shouldSaveConfig = true;
//}

bool Network::ConnectToWiFi()
{
  m_display.PrintError(Connecting, 1000, VGA_LIME);
  WiFi.disconnect(true);
  WiFi.enableAP(false);
  WiFi.mode(WIFI_STA);

//  if (!m_wifiManager.autoConnect(HostName))
//  {
//    m_display.PrintError(Error);
//    ESP.reset();
//    delay(5000);
//  }
  WiFi.begin(m_configuration.GetApName(), m_configuration.GetPassw());
  int n = 30;
  while (!Connected() && n)
  {
    delay(500);
    --n;
  }
  String s = ConnectedStr;
  if (!Connected())
  {
    WiFi.enableAP(true);
    WiFi.mode(WIFI_AP);
    s += WiFi.softAPIP().toString();
    m_display.PrintError(s.c_str());
  }
  else
  {
    s += WiFi.localIP().toString();
    m_display.PrintError(s.c_str(), 1000, VGA_LIME);
  }

//  if (shouldSaveConfig)
//  {
//    m_configuration.Write();
//  }
}

bool Network::Connect()
{
  WiFi.hostname(HostName);
  if (!ConnectToWiFi())
    return false;
  if (!Connected())
    return true;

  ArduinoOTA.setHostname(HostName);
  
  ArduinoOTA.onStart([this](){
    UpdateStarted = true;
  });
  ArduinoOTA.begin();
  return true;
}

bool Network::MqttConnect()
{
  if (!m_mqttClient.connect(deviceId) ||
      !m_mqttClient.subscribe(Device1Sensor1) || 
      !m_mqttClient.subscribe(Device1Sensor2) ||
      !m_mqttClient.subscribe(Device1Sensor3) ||
      !m_mqttClient.subscribe(Device1Error))
    return false;
  return m_mqttClient.connected();
}

