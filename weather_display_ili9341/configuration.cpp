#include "configuration.h"

//https://bblanchon.github.io/ArduinoJson/
#include <ArduinoJson.h>

#include <FS.h>

constexpr const char* DefaultMqttServer PROGMEM = "192.168.0.3";
constexpr const char* DefaultMqttPort PROGMEM = "1883";
constexpr const char* DefaultApiLocation PROGMEM = "Moscow,ru";
constexpr const char* DefaultLcdLedBrightnessSetpoint PROGMEM = "20";
constexpr const char* ConfigFileName PROGMEM = "/config.json";

namespace keys
{
constexpr const char* ApName PROGMEM = "ApName";
constexpr const char* Passw PROGMEM = "Passw";
constexpr const char* MqttServer PROGMEM = "MqttServer";
constexpr const char* MqttPort PROGMEM = "MqttPort";
constexpr const char* ApiLocation PROGMEM = "ApiLocation";
constexpr const char* LcdLedBrightnessSetpoint PROGMEM = "LcdLedBrightnessSetpoint";
}

Configuration::Configuration()
  : m_mqttServer(DefaultMqttServer)
  , m_mqttPortStr(DefaultMqttPort)
  , m_apiLocation(DefaultApiLocation)
  , m_lcdLedBrightnessSetpointStr(DefaultLcdLedBrightnessSetpoint)
{
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

  SetApName(static_cast<const char*>(json[keys::ApName]));
  SetPassw(static_cast<const char*>(json[keys::Passw]));
  SetMqttServer(static_cast<const char*>(json[keys::MqttServer]));
  SetMqttPortStr(static_cast<const char*>(json[keys::MqttPort]));
  SetApiLocation(static_cast<const char*>(json[keys::ApiLocation]));
  SetLcdLedBrightnessSetpointStr(static_cast<const char*>(json[keys::LcdLedBrightnessSetpoint]));
  return true;
}

bool Configuration::Write()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json[keys::ApName] = m_apName;
  json[keys::Passw] = m_passw;
  json[keys::MqttServer] = m_mqttServer;
  json[keys::MqttPort] = m_mqttPortStr;
  json[keys::ApiLocation] = m_apiLocation;
  json[keys::LcdLedBrightnessSetpoint] = m_lcdLedBrightnessSetpointStr;

  File configFile = SPIFFS.open(ConfigFileName, "w");
  if (!configFile)
    return false;

  json.printTo(configFile);
  configFile.close();
  return true;
}


