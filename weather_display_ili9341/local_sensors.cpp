#include "local_sensors.h"
#include "display.h"
#include "configuration.h"

constexpr int GPIO_I2C_DATA PROGMEM = 2;
constexpr int GPIO_I2C_CLK PROGMEM = 4;
constexpr int AnalogSensorPin PROGMEM = A0;

LocalSensors::LocalSensors(Configuration& configuration, Display& display)
  : m_configuration(configuration)
  , m_display(display)
  , m_roomTHSensor(GPIO_I2C_DATA, GPIO_I2C_CLK)
  , m_timerForReadSensors(1000, TimerState::Started)
  , m_timerForReadAnalogue(2000, TimerState::Started)
{
}

void LocalSensors::begin()
{
  m_roomTHSensor.begin();
  m_timerForReadSensors.Start();
  m_timerForReadAnalogue.Start();
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
  if (m_timerForReadAnalogue.IsElapsed())
  {
    int sensorValue = 1023 - analogRead(AnalogSensorPin);
    m_roomLight.value = sensorValue * 100 / 1024;
    m_roomLight.isGood = true;
    m_display.TurnLcdLedOnOff(m_roomLight.value > m_configuration.GetLcdLedBrightnessSetpoint());
  }
}

bool LocalSensors::Read()
{
  if (m_roomTemperature.isGood)
    m_roomTemperature.pred = m_roomTemperature.value;
  if (m_roomHumidity.isGood)
    m_roomHumidity.pred = m_roomHumidity.value;

  if (!m_roomTHSensor.read(m_roomTemperature.value, m_roomHumidity.value))
  {
    m_roomTemperature.isGood = false;
    m_roomHumidity.isGood = false;
    return false;
  }
  m_roomTemperature.isGood = m_roomTemperature.value != NAN;
  m_roomHumidity.isGood = m_roomHumidity.value != NAN;

  CalcAvarage(m_roomTemperature);
  CalcAvarage(m_roomHumidity);
  
  return true;
}

void LocalSensors::Print()
{
  if (m_roomTemperature.isGood)
  {
    m_display.SetBigFont();
    m_display.DrawNumber(m_roomTemperature.value, 24, 42, true);
    m_display.DrawArrow(m_roomTemperature.value, m_roomTemperature.r, 100, 47);
  }

  if (m_roomHumidity.isGood)
  {
    m_display.SetBigFont();
    m_display.DrawNumber(m_roomHumidity.value, 160, 42, false);
    m_display.DrawArrow(m_roomHumidity.value, m_roomHumidity.r, 216, 47);
  }

  if (m_roomLight.isGood)
  {
    m_display.SetSmallFont();
    m_display.DrawNumber(m_roomLight.value, 22, 298, false);
  }
}

