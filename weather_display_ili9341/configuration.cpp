#include "configuration.h"

#include <ArduinoJson.h>
#include <FS.h>

constexpr const char* DefaultMqttServer PROGMEM= "192.168.0.3";
constexpr const char* DefaultMqttPort PROGMEM= "1883";
constexpr const char* DefaultApiAppID PROGMEM= "";
constexpr const char* DefaultApiLocation PROGMEM= "Moscow,ru";

Configuration::Configuration()
{
  m_mqttServer.reserve(MqttServerMaxSize);
  m_mqttServer = DefaultMqttServer;
  
  m_mqttPortStr.reserve(MqttPortStrMaxSize);
  m_mqttPortStr = DefaultMqttPort;

  m_apiAppID.reserve(ApiAppIDMaxSize);
  m_apiAppID = DefaultApiAppID;

  m_apiLocation.reserve(ApiLocationMaxSize);
  m_apiLocation = DefaultApiLocation;
}

bool Configuration::Read()
{
  if (!SPIFFS.begin()) 
    return false;
  
  if (!SPIFFS.exists("/config.json"))
    return false;

  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) 
    return false;

  size_t size = configFile.size();
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) 
    return false;
    
  SetMqttServer(json["MqttServer"]);
  SetMqttPortStr(json["MqttPort"]);
  SetApiAppID(json["ApiAppID"]);
  SetApiLocation(json["ApiLocation"]);
  return true;
}

bool Configuration::Write()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["MqttServer"] = m_mqttServer;
  json["MqttPort"] = m_mqttPortStr;
  json["ApiAppID"] = m_apiAppID;
  json["ApiLocation"] = m_apiLocation;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
    return false;

  json.printTo(configFile);
  configFile.close();
  return true;
}


