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

constexpr const char* WebPostForm PROGMEM = R"(<!DOCTYPE HTML>
<html>
 <head>
  <meta charset="utf-8">
  <title>Wi-Fi weather station</title>
 </head>
 <body>  
 <form action="/save" method="POST">
  <p>AP name: <input type="text" name="ap_name" value="%ap_name%"></p>
  <p>Password: <input type="password" name="passw" value="%passw%"></p>
  <p>MQTT server: <input type="text" name="mqtt_server" value="%mqtt_server%"></p>
  <p>MQTT port: <input type="text" name="mqtt_port" value="%mqtt_port%"></p>
  <p>Location: <input type="text" name="location" value="%location%"></p>
  <p><input type="submit" value="Save"></p>
 </form>
 </body>
</html>
)";

static bool UpdateStarted = false;

void Network::OnMqttMessageArrived(char* topic, uint8_t* payload, unsigned int length)
{
  m_mqttConsumer->OnDataArrived(topic, payload, length);
}

Network::Network(Configuration& configuration, Display& display, RunState* runState, IMqttConsumer* mqttConsumer)
  : m_configuration(configuration)
  , m_display(display)
  , m_runState(runState)
  , m_mqttConsumer(mqttConsumer)
  , m_mqttClient(m_wifiClient)
  , m_webServer(80)
{
}

Network::~Network()
{
}

void Network::begin()
{
  Connect();

  m_mqttClient.setServer(m_configuration.GetMqttServer(), m_configuration.GetMqttPort());
  m_mqttClient.setCallback(std::bind(&Network::OnMqttMessageArrived, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  if (m_wifiMode == WiFiMode::ClientMode)
    MqttConnect();

  m_webServer.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  m_webServer.on("/", std::bind(&Network::SetWebIsRoot, this, std::placeholders::_1));
  m_webServer.on("/save", HTTP_POST, std::bind(&Network::SetWebIsSave, this, std::placeholders::_1));
  m_webServer.onNotFound(std::bind(&Network::SetWebIsNotFound, this, std::placeholders::_1));
  
  m_webServer.begin();
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
    
  if (!m_mqttClient.connected() && m_wifiMode == WiFiMode::ClientMode)
  {
    MqttConnect();
  }

  if (m_runState->IsStopped())
    return;
  if (m_wifiMode == WiFiMode::ClientMode)
    m_mqttClient.loop();
  else if (m_dnsServer)
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
  constexpr uint8_t DNS_PORT = 53;
  m_dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
  return m_wifiMode;
}

void Network::Connect()
{
  WiFi.hostname(HostName);
  ConnectToWiFi();
  if (!Connected())
  {
    m_display.PrintError("WiFi connection error!");
    return;
  }

  ArduinoOTA.setHostname(HostName);
  
  ArduinoOTA.onStart([this](){
    UpdateStarted = true;
  });
  ArduinoOTA.begin();
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

void Network::SetWebIsRoot(AsyncWebServerRequest* request)
{
  String s(WebPostForm);
  s.replace("%ap_name%", m_configuration.GetApName());
  s.replace("%passw%", m_configuration.GetPassw());
  s.replace("%mqtt_server%", m_configuration.GetMqttServer());
  s.replace("%mqtt_port%", m_configuration.GetMqttPortStr());
  s.replace("%location%", m_configuration.GetApiLocation());
  request->send(200, "text/html", s);
}

void Network::SetWebIsNotFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Page Not Found\n\n");
}

void Network::SetWebIsSave(AsyncWebServerRequest *request)
{
  if (request->hasArg("ap_name"))
    m_configuration.SetApName(request->arg("ap_name"));
  if (request->hasArg("passw"))
    m_configuration.SetPassw(request->arg("passw"));
  if (request->hasArg("mqtt_server"))
    m_configuration.SetMqttServer(request->arg("mqtt_server"));
  if (request->hasArg("mqtt_port"))
    m_configuration.SetMqttPortStr(request->arg("mqtt_port"));
  if (request->hasArg("location"))
    m_configuration.SetApiLocation(request->arg("location"));

  m_configuration.Write();
  m_isReset = true;
  request->send(200, "text/plain", String("Saved\n\n"));
}

