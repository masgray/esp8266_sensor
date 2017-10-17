#include "application.h"

Application::Application()
{
}

void Application::begin()
{
  m_display.begin();
  m_configuration.Read();
  m_network.begin();
  m_localSensors.begin();
  m_remoteSensors.begin();
}

void Application::loop()
{
  m_network.loop();
}

