#pragma once

#include "configuration.h"
#include "network.h"
#include "display.h"
#include "local_sensors.h"
#include "remote_sensors.h"

class Application: public RunState
{
public:
  Application();

  void begin();
  void loop();

private:
  Configuration m_configuration;
  Network m_network;
  Display m_display;
  LocalSensors m_localSensors;
  RemoteSensors m_remoteSensors;
  bool m_isRun = true;
};


