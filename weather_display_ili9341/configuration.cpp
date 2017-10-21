#include "configuration.h"

#include <ArduinoJson.h>
#include <FS.h>

constexpr const char* DefaultMqttServer PROGMEM = "192.168.0.3";
constexpr const char* DefaultMqttPort PROGMEM = "1883";
constexpr const char* DefaultApiLocation PROGMEM = "Moscow,ru";
constexpr const char* ConfigFileName PROGMEM = "/config.json";

namespace keys
{
constexpr const char* MqttServer PROGMEM = "MqttServer";
constexpr const char* MqttPort PROGMEM = "MqttPort";
constexpr const char* ApiLocation PROGMEM = "ApiLocation";
}

Configuration::Configuration()
{
  m_mqttServer.reserve(MqttServerMaxSize);
  m_mqttServer = DefaultMqttServer;
  
  m_mqttPortStr.reserve(MqttPortStrMaxSize);
  m_mqttPortStr = DefaultMqttPort;

  m_apiLocation.reserve(ApiLocationMaxSize);
  m_apiLocation = DefaultApiLocation;
}

bool Configuration::Read()
{
  if (!SPIFFS.begin()) 
    return false;
  
  if (!SPIFFS.exists(ConfigFileName))
    return false;

  File configFile = SPIFFS.open(ConfigFileName, "r");
  if (!configFile) 
    return false;

  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) 
    return false;
    
  SetMqttServer(json[keys::MqttServer]);
  SetMqttPortStr(json[keys::MqttPort]);
  SetApiLocation(json[keys::ApiLocation]);
  return true;
}

bool Configuration::Write()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json[keys::MqttServer] = m_mqttServer;
  json[keys::MqttPort] = m_mqttPortStr;
  json[keys::ApiLocation] = m_apiLocation;

  File configFile = SPIFFS.open(ConfigFileName, "w");
  if (!configFile)
    return false;

  json.printTo(configFile);
  configFile.close();
  return true;
}


