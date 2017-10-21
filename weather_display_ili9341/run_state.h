#pragma once

class RunState
{
public:
  void Pause() { m_isRun = false; }
  void Resume() { m_isRun = false; }
  bool IsRun() { return m_isRun; }
  bool IsStopped() { return !m_isRun; }

private:
  bool m_isRun = true;
};

