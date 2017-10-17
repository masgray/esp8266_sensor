#pragma once

class LocalSensors
{
public:
  LocalSensors();

  void begin();
  bool Read();
  bool Print();
};


