#include "timer.h"

void GlobalOnTick(Timer* timer)
{
  timer->OnTick();
}

void Timer::Start()
{
  m_ticker.attach_ms(m_timeInMs, GlobalOnTick, this);
}

void Timer::Stop()
{
  m_ticker.detach();
  m_isElapsed = false;
}

bool Timer::IsElapsed()
{
  bool isElapsed = m_isElapsed;
  m_isElapsed = false;
  return isElapsed;
}

void Timer::OnTick()
{
  m_isElapsed = true;
}

void Timer::Reset(TimerState state)
{
  m_isElapsed = state == TimerState::Started;
}

