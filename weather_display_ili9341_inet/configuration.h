#pragma once

#include "pass.h"

struct Configuration
{
  const char* GetApName() const { return ssid; }
  const char* GetPassw() const { return password; }
  const char* GetApiLocation() const { return MyApiAppID; }
};

