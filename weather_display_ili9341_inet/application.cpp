#include "application.h"

#include <pgmspace.h>

Application::Application()
  : m_network(m_display, this)
  , m_localSensors(m_display)
  , m_remoteSensors(m_display)
{
}

void Application::begin()
{
  m_display.begin();
  
  m_network.begin();
  m_localSensors.begin();
  
  while(!m_network.Connected())
  {
  }
  m_remoteSensors.begin();
}

void Application::loop()
{
  m_network.loop();

  if (IsStopped())
    return;
  m_localSensors.loop();

  if (IsStopped() || !m_network.ConnectedNoWait())
    return;
  m_remoteSensors.loop();
}

