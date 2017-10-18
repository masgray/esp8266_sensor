#include "application.h"

#include <pgmspace.h>

constexpr const char* Error PROGMEM= "Error reading config!";

Application::Application()
  : m_network(m_configuration, m_display, this, &m_remoteSensors)
  , m_localSensors(m_display)
  , m_remoteSensors(m_configuration, m_display)
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
}

void Application::loop()
{
  if (IsStopped())
    return;
  m_network.loop();

  if (IsStopped())
    return;
  m_localSensors.loop();

  if (IsStopped())
    return;
  m_remoteSensors.loop();
}

