#include "local_sensors.h"
#include "display.h"

constexpr int GPIO_I2C_DATA PROGMEM = 2;
constexpr int GPIO_I2C_CLK PROGMEM = 4;

LocalSensors::LocalSensors(Display& display)
  : m_display(display)
  , m_roomTHSensor(GPIO_I2C_DATA, GPIO_I2C_CLK)
  , m_timerForReadSensors(1000, TimerState::Started)
{
}

void LocalSensors::begin()
{
  m_roomTHSensor.begin();
  m_timerForReadSensors.Start();
}

void LocalSensors::loop()
{
  if (m_timerForReadSensors.IsElapsed())
  {
    if (Read())
      Print();
    else
      m_timerForReadSensors.Reset(TimerState::Started);
  }
}

bool LocalSensors::Read()
{
  if (m_roomTemperature.isGood)
    m_roomTemperature.pred = m_roomTemperature.value;
  if (m_roomHumidity.isGood)
    m_roomHumidity.pred = m_roomHumidity.value;

  if (!m_roomTHSensor.read(m_roomTemperature.value, m_roomHumidity.value))
    return false;

  CalcAvarage(m_roomTemperature);
  CalcAvarage(m_roomHumidity);
  
  return true;
}

void LocalSensors::Print()
{
  if (m_roomTemperature.isGood)
  {
    m_display.DrawNumber(m_roomTemperature.value, 24, 42, true);
    m_display.DrawArrow(m_roomTemperature.value, m_roomTemperature.r, 100, 47);
  }

  if (m_roomHumidity.isGood)
  {
    m_display.DrawNumber(m_roomHumidity.value, 152, 42, false);
    m_display.DrawArrow(m_roomHumidity.value, m_roomHumidity.r, 216, 47);
  }
}

