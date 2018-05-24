#pragma once

#include <Ticker.h>

class Timer;

void GlobalOnTick(Timer* timer);

enum class TimerState
{
  Started,
  Stopped
};

class Timer
{
public:
  Timer(uint32_t ms, TimerState initialState = TimerState::Stopped)
    : m_timeInMs(ms)
    , m_isElapsed(initialState == TimerState::Started)
  {
  }

  void Start();
  void Stop();
  bool IsElapsed();
  void Reset(TimerState state);
  
  void OnTick();
  
private:
  uint32_t m_timeInMs = 0;
  Ticker m_ticker;
  bool m_isElapsed = false;
};


