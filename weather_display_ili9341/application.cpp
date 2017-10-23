#include "application.h"

#include <pgmspace.h>

constexpr const char* Error PROGMEM = "Error reading config!";
constexpr const char* WebPostForm PROGMEM = R"(<!DOCTYPE HTML>
<html>
 <head>
  <meta charset="utf-8">
  <title>Wi-Fi weather station</title>
 </head>
 <body>  
 <form action="/save" method="post">
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

Application::Application()
  : m_network(m_configuration, m_display, this, &m_remoteSensors)
  , m_localSensors(m_display)
  , m_remoteSensors(m_configuration, m_display)
  , m_webServer(80)
{
}

void Application::begin()
{
  m_display.begin();
  
  if (!m_configuration.Read())
  {
    m_display.PrintError(Error);
  }
    
  m_network.begin();
  m_localSensors.begin();
  m_remoteSensors.begin();

  m_webServer.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  m_webServer.on("/", [this](AsyncWebServerRequest *request){
    this->SetWebIsRoot(request);
  });

  m_webServer.on("/save", HTTP_POST, [this](AsyncWebServerRequest *request){
    this->SetWebIsSave(request);
  });

  m_webServer.onNotFound([this](AsyncWebServerRequest *request){
    this->SetWebIsNotFound(request);
  });
  m_webServer.begin();
}

void Application::loop()
{
  m_network.loop();

  if (IsStopped())
    return;
  m_localSensors.loop();

  if (IsStopped())
    return;
  m_remoteSensors.loop();

  if (m_webIsRoot)
  {
    m_webIsRoot = false;
    WebHandleRoot();
  }
  if (m_webIsNotFound)
  {
    m_webIsNotFound = false;
    WebHandleNotFound();
  }
  if (m_webIsSave)
  {
    m_webIsSave = false;
    WebHandleSave();
  }
}

void Application::SetWebIsRoot(AsyncWebServerRequest* request)
{
  m_requestRoot = request;
  m_webIsRoot = true;
}

void Application::SetWebIsNotFound(AsyncWebServerRequest *request)
{
  m_requestNotFound = request;
  m_webIsNotFound = true;
}

void Application::SetWebIsSave(AsyncWebServerRequest *request)
{
  m_requestSave = request;
  m_webIsSave = true;
}

void Application::WebHandleRoot()
{
  if (!m_requestRoot)
    return;
  String s(WebPostForm);
  s.replace("%ap_name%", m_configuration.GetApName());
  s.replace("%passw%", m_configuration.GetPassw());
  s.replace("%mqtt_server%", m_configuration.GetMqttServer());
  s.replace("%mqtt_port%", m_configuration.GetMqttPortStr());
  s.replace("%location%", m_configuration.GetApiLocation());
  m_requestRoot->send(200, "text/html", s);
  m_requestRoot = nullptr;
}

void Application::WebHandleSave()
{
  if (!m_requestSave)
    return;
  String message = "Saved\n\n";
  m_requestSave->send(404, "text/plain", message);
  
  if (m_requestSave->hasArg("ap_name"))
    m_configuration.SetApName(m_requestSave->arg("ap_name"));
  if (m_requestSave->hasArg("passw"))
    m_configuration.SetPassw(m_requestSave->arg("passw"));
  if (m_requestSave->hasArg("mqtt_server"))
    m_configuration.SetMqttServer(m_requestSave->arg("mqtt_server"));
  if (m_requestSave->hasArg("mqtt_port"))
    m_configuration.SetMqttPortStr(m_requestSave->arg("mqtt_port"));
  if (m_requestSave->hasArg("location"))
    m_configuration.SetApiLocation(m_requestSave->arg("location"));

  m_configuration.Write();
  ESP.reset();
  delay(5000);
}

void Application::WebHandleNotFound()
{
  if (!m_requestNotFound)
    return;
  m_requestNotFound->send(404, "text/plain", "File Not Found\n\n");
  m_requestNotFound = nullptr;
}


