#pragma once

#include "chart.h"

#include <UTFT.h>

class Display
{
public:
  Display();

  void begin();

  void DrawNumber(float number, int x, int y, bool withPlus, int precision = 0);
  void DrawArrow(float value, float valueR, int x, int y);
  void PrintError(const char* msg, word color = VGA_RED);
  void SetSmallFont();
  void SetBigFont();
  void PrintLastUpdated(int x, int y, uint32_t deltaTime);
  void DrawWind(float windDir, int x, int y);
  void DrawChart(float valueMin, float valueMax, const History& history, int historyIndex);

private:
  UTFT m_tft;

  int m_displayWidth = 0;
  int m_displayHeight = 0;
};


