#pragma once

#include <UTFT.h>

class Display
{
public:
  Display();

  void begin();

  void DrawNumber(float number, int x, int y, bool withPlus, int charsWidth, int precision);
  void DrawArrow(float value, float valueR, int x, int y);
  void PrintError(const char* msg);
  void SetSmallFont();
  void SetBigFont();
  void PrintLastUpdated(int x, int y, uint32_t deltaTime);
  void DrawWind(float windDir, int x, int y);

private:
  UTFT m_tft;

  int m_displayWidth = 0;
  int m_displayHeight = 0;
};


