// lib/weather_model/weather_model.h — the spec's data->art bridge (A3,
// roadmap §3). Pure std C++17, zero Arduino/LVGL includes; all mapping is
// native-tested. Maker-tunable constants (band edges, WMO groups, art file
// names) live here and in the .cpp — tweak them, rerun the tests.
#pragma once

#include <cstdint>
#include <string>

// Outfit is chosen by temperature band (spec table: <0, 0-9, 10-19, 20-27, >=28 °C).
enum class TempBand : uint8_t { Freezing, Cold, Mild, Warm, Hot };

// Overlay + background are chosen by WMO weather_code group (spec table).
enum class Condition : uint8_t { Clear, Cloudy, Fog, Rain, Snow, Storm, Unknown };

TempBand tempBand(float celsius);
Condition conditionFromWmo(int code);
