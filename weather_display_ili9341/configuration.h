#pragma once

#include <Arduino.h>

#include <pgmspace.h>

class Configuration
{
public:
  Configuration();

  bool Read();
  bool Write();

  const char* GetApName() const { return m_apName.c_str(); }
  void SetApName(const char* text) { m_apName = text; }
  void SetApName(const String& text) { m_apName = text; }
  
  const char* GetPassw() const { return m_passw.c_str(); }
  void SetPassw(const char* text) { m_passw = text; }
  void SetPassw(const String& text) { m_passw = text; }
  
  const char* GetMqttServer() const { return m_mqttServer.c_str(); }
  void SetMqttServer(const char* text) { m_mqttServer = text; }
  void SetMqttServer(const String& text) { m_mqttServer = text; }
  
  const char* GetMqttPortStr() const { return m_mqttPortStr.c_str(); }
  uint16_t GetMqttPort() const { return m_mqttPortStr.toInt(); }
  void SetMqttPortStr(const char* text) { m_mqttPortStr = text; }
  void SetMqttPortStr(const String& text) { m_mqttPortStr = text; }

  const char* GetApiLocation() const { return m_apiLocation.c_str(); }
  void SetApiLocation(const char* text) { m_apiLocation = text; }
  void SetApiLocation(const String& text) { m_apiLocation = text; }

private:
  String m_apName;
  String m_passw;
  String m_mqttServer;
  String m_mqttPortStr;
  String m_apiLocation;
};

