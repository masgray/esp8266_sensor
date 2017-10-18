#include "display.h"

#include <pgmspace.h>

extern uint8_t Retro8x16[];
extern uint8_t Arial_round_16x24[];
extern unsigned short Background[];

struct Vector
{
  float x;
  float y;
};

constexpr int GPIO_RS PROGMEM = 5;

Display::Display()
  : m_tft(ILI9341_S5P, NOTINUSE, NOTINUSE, GPIO_RS)
{
}

void Display::begin()
{
  pinMode(GPIO_RS, OUTPUT);
  
  m_tft.InitLCD(PORTRAIT);
  m_tft.clrScr();
  m_tft.setColor(VGA_WHITE);
  m_tft.setBackColor(38, 84, 120);

  m_displayWidth = m_tft.getDisplayXSize();
  m_displayHeight = m_tft.getDisplayYSize();

  m_tft.drawBitmap(0, 0, m_displayWidth, m_displayHeight, Background);
}

void DrawStable(UTFT& tft, int x, int y)
{
  tft.setColor(38, 84, 120);
  tft.fillRect(x-2, y, x+2, y + 8);
  tft.setColor(VGA_WHITE);
}

void DrawUp(UTFT& tft, int x, int y)
{
  DrawStable(tft, x, y);
  tft.drawLine(x, y, x, y + 8);
  tft.drawLine(x, y, x - 2, y + 4);
  tft.drawLine(x, y, x + 2, y + 4);
}

void DrawDown(UTFT& tft, int x, int y)
{
  DrawStable(tft, x, y);
  tft.drawLine(x, y, x, y + 8);
  tft.drawLine(x, y + 8, x - 2, y + 4);
  tft.drawLine(x, y + 8, x + 2, y + 4);
}

void Display::DrawArrow(float value, float valueR, int x, int y)
{
  if (valueR == NAN)
    return;
  if (value > valueR)
    DrawUp(m_tft, x, y);
  else if (value < valueR)
    DrawDown(m_tft, x, y);
  else
    DrawStable(m_tft, x, y);
}

int CalcY(float value, float valueMin, float valueMax)
{
  double yPos = ChartBottom - (ChartBottom - ChartTop) / (valueMax - valueMin) * (value - valueMin);
  return yPos;
}

void Display::DrawChart(float valueMin, float valueMax, const History& history, int historyIndex)
{
  m_tft.setColor(38, 84, 120);
  m_tft.fillRect(ChartLeft, ChartTop, ChartRight, ChartBottom);
  m_tft.setColor(VGA_LIME);

  if (historyIndex < 2)
  {
    m_tft.drawPixel(ChartLeft, CalcY(history[0], valueMin, valueMax));
  }
  else
  {
    for (int i = 1; i < historyIndex; ++i)
    {
      m_tft.drawLine(ChartLeft + i - 1, CalcY(history[i - 1], valueMin, valueMax), ChartLeft + i, CalcY(history[i], valueMin, valueMax));
    }
  }
  m_tft.setColor(VGA_WHITE);
}

void Display::DrawNumber(float number, int x, int y, bool withPlus, int charsWidth, int precision)
{
  static char msg[32];
  static char msg2[32];
  dtostrf(number, charsWidth, precision, msg);
  char* p = msg;
  while(*p == ' ' && *p != 0)
    ++p;
  if (withPlus && number >= 0.0)
    sprintf(msg2, "+%s ", p);
  else
    sprintf(msg2, "%s ", p);
  m_tft.print(msg2, x, y);
}

void Display::PrintError(const char* msg)
{
  SetSmallFont();
  m_tft.setColor(38, 84, 120);
  m_tft.fillRect(ChartLeft, ChartTop, ChartRight, ChartBottom);
  m_tft.setColor(VGA_RED);
  m_tft.print(msg, ChartLeft + 2, ChartTop + 2);
  m_tft.setColor(VGA_WHITE);
  delay(15000);
}

void Display::SetSmallFont()
{
  m_tft.setFont(Retro8x16);
}

void Display::SetBigFont()
{
  m_tft.setFont(Arial_round_16x24);
}

void Display::PrintLastUpdated(int x, int y, uint32_t deltaTime)
{
  static char msg[32];
  if (deltaTime < 100)
    sprintf(msg, "%d ", deltaTime);
  else
    sprintf(msg, "-  ");
  SetSmallFont();
  m_tft.print(msg, x, y);
}

void ClearWindArrow(UTFT& tft, int x, int y)
{
  tft.setColor(38, 84, 120);
  tft.fillRect(x - 8, y - 8, x + 8, y + 8);
  tft.setColor(VGA_WHITE);
}

void DrawWindArrow(UTFT& tft, int x, int y, Vector point1, Vector point2, Vector point3, Vector point4)
{
  ClearWindArrow(tft, x, y);
  tft.drawLine(x + point1.x, y + point1.y, x + point2.x, y + point2.y);
  tft.drawLine(x + point1.x, y + point1.y, x + point3.x, y + point3.y);
  tft.drawLine(x + point1.x, y + point1.y, x + point4.x, y + point4.y);
}

void Rotate(Vector& point, float angle)
{
  Vector rotatedPoint;
  float rad = angle * 2 * PI / 360;
  rotatedPoint.x = point.x * cos(rad) + point.y * sin(rad);
  rotatedPoint.y = point.x * sin(rad) - point.y * cos(rad);
  point = rotatedPoint;
}

void Display::DrawWind(float windDir, int x, int y)
{
  Vector point1{0, -8};
  Rotate(point1, windDir);
  Vector point2{0, 8};
  Rotate(point2, windDir);
  Vector point3{-2, -2};
  Rotate(point3, windDir);
  Vector point4{2, -2};
  Rotate(point4, windDir);
  DrawWindArrow(m_tft, x, y, point1, point2, point3, point4);
}


