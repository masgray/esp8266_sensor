#pragma once

class RemoteSensors
{
public:
  RemoteSensors();

  void begin();
  bool Read();
  bool Print();
};


