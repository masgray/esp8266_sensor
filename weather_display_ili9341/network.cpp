#include "network.h"
#include "configuration.h"
#include "display.h"

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>

constexpr const char *prefix PROGMEM= "unit1";
constexpr const char *deviceId PROGMEM= "unit1_device2";
constexpr const char *Device1Sensor1 PROGMEM= "unit1/device1/sensor1/status";
constexpr const char *Device1Sensor2 PROGMEM= "unit1/device1/sensor2/status";
constexpr const char *Device1Sensor3 PROGMEM= "unit1/device1/sensor3/status";
constexpr const char *Device1Error PROGMEM= "unit1/device1/error/status";
constexpr const char *Device1Hello PROGMEM= "unit1/device1/hello/status";

constexpr const char* HostName PROGMEM= "EspWeatherStationDisplay";
constexpr const uint16_t MqttDefaultPort PROGMEM= 1883;
constexpr const char *DefaultApPassword PROGMEM= "DefaultPassword0001";

namespace keys
{
constexpr const char* MqttServer_name  PROGMEM= "server";
constexpr const char* MqttServer_descr PROGMEM= "MQTT server";
constexpr const char* MqttServerPort_name  PROGMEM= "port";
constexpr const char* MqttServerPort_descr PROGMEM= "MQTT port";
constexpr const char* ApiAppId_name  PROGMEM= "ApiAppId";
constexpr const char* ApiAppId_descr PROGMEM= "APPID for api.openweathermap.org";
constexpr const char* ApiLocation_name  PROGMEM= "ApiLocation";
constexpr const char* ApiLocation_descr PROGMEM= "Location for api.openweathermap.org in form of \"City,country code\" (Moscow,ru)";
}

static bool shouldSaveConfig = false;

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

void Network::begin()
{
  m_parameterMqttServer.reset(new WiFiManagerParameter(keys::MqttServer_name, keys::MqttServer_descr, m_configuration.GetMqttServer(), MqttServerMaxSize));
  m_parameterMqttPort.reset(new WiFiManagerParameter(keys::MqttServerPort_name, keys::MqttServerPort_descr, m_configuration.GetMqttPortStr(), MqttPortStrMaxSize));
  m_parameterApiAppID.reset(new WiFiManagerParameter(keys::ApiAppId_name, keys::ApiAppId_descr, m_configuration.GetApiAppID(), ApiAppIDMaxSize));
  m_parameterApiLocation.reset(new WiFiManagerParameter(keys::ApiLocation_name, keys::ApiLocation_descr, m_configuration.GetApiLocation(), ApiLocationMaxSize));
  
  Connect();

  m_mqttClient.setServer(m_configuration.GetMqttServer(), m_configuration.GetMqttPort());
  m_mqttClient.setCallback(OnMqttMessageArrived);
  MqttConnect();
}

void Network::loop()
{
  if (!m_runState->IsStopped() && !Connected())
    Connect();
  ArduinoOTA.handle();

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
}

void configModeCallback(WiFiManager*)
{
  static char msg[64];
  constexpr const char* text PROGMEM="Connect to Wi-Fi network \"%s\"\nOpen page at http://%s\nto configure station.";
  auto ip = WiFi.softAPIP().toString();
  sprintf(msg, text, HostName, ip.c_str());
  m_display.PrintError(msg);
}

void saveConfigCallback() 
{
  shouldSaveConfig = true;
}

void Network::ConnectToWiFi()
{
  WiFi.disconnect(true);

  m_wifiManager.setAPCallback(configModeCallback);
  m_wifiManager.setSaveConfigCallback(saveConfigCallback);
  m_wifiManager.setMinimumSignalQuality(20);
  m_wifiManager.addParameter(parameterMqttServer.get());
  m_wifiManager.addParameter(parameterMqttPort.get());
  m_wifiManager.addParameter(parameterApiAppID.get());
  m_wifiManager.addParameter(parameterApiLocation.get());

  if (!m_wifiManager.autoConnect(HostName, DefaultApPassword))
  {
    constexpr const char* Error PROGMEM="Web configuring error!";
    m_display.PrintError(Error);
    ESP.reset();
    delay(5000);
  }

  m_configuration.SetMqttServer(parameterMqttServer->getValue());
  m_configuration.SetMqttPortStr(MqttPortStr, parameterMqttPort->getValue());
  m_configuration.SetApiAppID(ApiAppID, parameterApiAppID->getValue());
  m_configuration.SetApiLocation(ApiLocation, parameterApiLocation->getValue());

  if (shouldSaveConfig)
  {
    m_configuration.Write();
  }
}

void Network::Connect()
{
  ConnectToWiFi();
  if (!Connected())
  {
    return;
  }

  WiFi.hostname(HostName);
  ArduinoOTA.setHostname(HostName);
  
  ArduinoOTA.onStart([](){
    m_runState->Pause() = false;
  });
  ArduinoOTA.begin();
}

void Network::MqttConnect()
{
  if (!m_mqttClient.connect(deviceId))
  {
    return;
  }

  if (!m_mqttClient.subscribe(Device1Sensor1))
  {
    return;
  }
  if (!m_mqttClient.subscribe(Device1Sensor2))
  {
    return;
  }
  if (!m_mqttClient.subscribe(Device1Sensor3))
  {
    return;
  }
  if (!m_mqttClient.subscribe(Device1Error))
  {
    return;
  }
}

