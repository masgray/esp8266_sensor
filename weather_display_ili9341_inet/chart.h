#pragma once

#include <Arduino.h>
#include <pgmspace.h>

constexpr double ChartLeft PROGMEM = 6;
constexpr double ChartRight PROGMEM = 236;
constexpr double ChartTop PROGMEM = 199;
constexpr double ChartBottom PROGMEM = 284;
constexpr uint32_t HistoryDepth PROGMEM = ChartRight - ChartLeft;

using History = float[HistoryDepth];

