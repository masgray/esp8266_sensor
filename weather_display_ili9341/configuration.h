#pragma once

#include <Arduino.h>

#include <pgmspace.h>

constexpr const uint32_t MqttServerMaxSize PROGMEM  = 32;
constexpr const uint32_t MqttPortStrMaxSize PROGMEM = 6;
constexpr const uint32_t ApiAppIDMaxSize PROGMEM    = 33;
constexpr const uint32_t ApiLocationMaxSize PROGMEM = 33;

class Configuration
{
public:
  Configuration();

  bool Read();
  bool Write();

  const char* GetMqttServer() const { return m_mqttServer.c_str(); }
  void SetMqttServer(const char* text) { m_mqttServer = text; }
  
  const char* GetMqttPortStr() const { return m_mqttPortStr.c_str(); }
  uint16_t GetMqttPort() const { return m_mqttPortStr.toInt(); }
  void SetMqttPortStr(const char* text) { m_mqttPortStr = text; }

  const char* GetApiAppID() const { return m_apiAppID.c_str(); }
  void SetApiAppID(const char* text) { m_apiAppID = text; }

  const char* GetApiLocation() const { return m_apiLocation.c_str(); }
  void SetApiLocation(const char* text) { m_apiLocation = text; }

private:
  String m_mqttServer;
  String m_mqttPortStr;
  String m_apiAppID;
  String m_apiLocation;
};

