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

namespace web
{
constexpr const char* HtmlHeader PROGMEM = R"(<!DOCTYPE HTML>
<html>
 <head>
  <meta charset="utf-8"/>
  <title>Wi-Fi weather station</title>
  <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no"/>
 </head>
 <body style="background-color: rgb(38, 84, 120); color: white; font-family: Helvetica,Arial,sans-serif;">
 <table align="center">
 <tr>
 <td>
)";

constexpr const char* HtmlFooter PROGMEM = R"( </td>
 </tr>
 </table>
 </body>
</html>
)";

constexpr const char* HtmlPostForm PROGMEM = R"(
 <form action="/save" method="POST" align="left">
 <table>
  <tr>
    <td>AP name:</td>
    <td><input type="text" name="ap_name" value="%ap_name%"></td>
  </tr>
  <tr>
    <td>Password:</td>
    <td><input type="password" name="passw" value="%passw%"></td>
  </tr>
  <tr>
    <td>MQTT server:</td>
    <td><input type="text" name="mqtt_server" value="%mqtt_server%"></td>
  </tr>
  <tr>
    <td>MQTT port:</td>
    <td><input type="text" name="mqtt_port" value="%mqtt_port%"></td>
  </tr>
  <tr>
    <td>Location:</td>
    <td><input type="text" name="location" value="%location%"></td>
  </tr>
  <tr>
    <td>Lighting brightness<br>for turn off LCD: </td>
    <td><input type="text" name="lcd_led_brightness_setpoint" value="%lcd_led_brightness_setpoint%"></td>
  </tr>
  <tr>
    <td align="center" colspan=2><input type="submit" value="Save & Restart"></td>
  </tr>
 </form>
)";

constexpr const char* percent PROGMEM = "%";
constexpr const char* ap_name PROGMEM = "ap_name";
constexpr const char* passw PROGMEM = "passw";
constexpr const char* mqtt_server PROGMEM = "mqtt_server";
constexpr const char* mqtt_port PROGMEM = "mqtt_port";
constexpr const char* location PROGMEM = "location";
constexpr const char* lcd_led_brightness_setpoint PROGMEM = "lcd_led_brightness_setpoint";
constexpr const char* text_html PROGMEM = "text/html";
constexpr const char* text_plain PROGMEM = "text/plain";
constexpr const char* pathRoot PROGMEM = "/";
constexpr const char* pathHeap PROGMEM = "/heap";
constexpr const char* pathSave PROGMEM = "/save";
constexpr const char* PageNotFound PROGMEM = "Page Not Found!\n\n";
constexpr const char* PageSaved PROGMEM = "<p>Saved.</p>\n<p>Trying to connect to Wi-Fi network...</p>\n";
}

constexpr uint8_t DNS_PORT PROGMEM = 53;
  
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

  m_webServer.on(web::pathHeap, HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, web::text_html, String(web::HtmlHeader) + String(ESP.getFreeHeap()) + String(web::HtmlFooter));
  });

  m_webServer.on(web::pathRoot, std::bind(&Network::SetWebIsRoot, this, std::placeholders::_1));
  m_webServer.on(web::pathSave, HTTP_POST, std::bind(&Network::SetWebIsSave, this, std::placeholders::_1));
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
  String s(web::HtmlPostForm);
  s.replace(String(web::percent) + web::ap_name + String(web::percent), m_configuration.GetApName());
  s.replace(String(web::percent) + web::passw + String(web::percent), m_configuration.GetPassw());
  s.replace(String(web::percent) + web::mqtt_server + String(web::percent), m_configuration.GetMqttServer());
  s.replace(String(web::percent) + web::mqtt_port + String(web::percent), m_configuration.GetMqttPortStr());
  s.replace(String(web::percent) + web::location + String(web::percent), m_configuration.GetApiLocation());
  s.replace(String(web::percent) + web::lcd_led_brightness_setpoint + String(web::percent), m_configuration.GetLcdLedBrightnessSetpointStr());
  request->send(200, web::text_html, String(web::HtmlHeader) + s + String(web::HtmlFooter));
}

void Network::SetWebIsNotFound(AsyncWebServerRequest *request)
{
  request->send(404, web::text_html, String(web::HtmlHeader) + web::PageNotFound + String(web::HtmlFooter));
}

void Network::SetWebIsSave(AsyncWebServerRequest *request)
{
  if (request->hasArg(web::ap_name))
    m_configuration.SetApName(request->arg(web::ap_name));
  if (request->hasArg(web::passw))
    m_configuration.SetPassw(request->arg(web::passw));
  if (request->hasArg(web::mqtt_server))
    m_configuration.SetMqttServer(request->arg(web::mqtt_server));
  if (request->hasArg(web::mqtt_port))
    m_configuration.SetMqttPortStr(request->arg(web::mqtt_port));
  if (request->hasArg(web::location))
    m_configuration.SetApiLocation(request->arg(web::location));
  if (request->hasArg(web::lcd_led_brightness_setpoint))
    m_configuration.SetLcdLedBrightnessSetpointStr(request->arg(web::lcd_led_brightness_setpoint));

  m_configuration.Write();
  m_isReset = true;
  request->send(200, web::text_html, String(web::HtmlHeader) + web::PageSaved + String(web::HtmlFooter));
}

