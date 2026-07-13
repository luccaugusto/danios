#include "weather_model.h"

TempBand tempBand(float celsius) {
  if (celsius < 0.0f) return TempBand::Freezing;
  if (celsius < 10.0f) return TempBand::Cold;
  if (celsius < 20.0f) return TempBand::Mild;
  if (celsius < 28.0f) return TempBand::Warm;
  return TempBand::Hot;
}

Condition conditionFromWmo(int code) {
  switch (code) {
    case 0: case 1: return Condition::Clear;
    case 2: case 3: return Condition::Cloudy;
    case 45: case 48: return Condition::Fog;
    case 95: case 96: case 99: return Condition::Storm;
    default: break;
  }
  if ((code >= 51 && code <= 57) ||   // drizzle
      (code >= 61 && code <= 67) ||   // rain
      (code >= 80 && code <= 82)) {   // rain showers
    return Condition::Rain;
  }
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) {  // snow
    return Condition::Snow;
  }
  return Condition::Unknown;
}

float toDisplayTemp(float celsius, bool fahrenheit) {
  return fahrenheit ? celsius * 9.0f / 5.0f + 32.0f : celsius;
}

const char* conditionLabelPt(Condition c) {
  switch (c) {
    case Condition::Clear:  return "Ensolarado";
    case Condition::Cloudy: return "Nublado";
    case Condition::Fog:    return "Neblina";
    case Condition::Rain:   return "Chuva";
    case Condition::Snow:   return "Neve";
    case Condition::Storm:  return "Tempestade";
    default:                return "--";
  }
}

namespace {
// Maker-tunable art table (spec "Data -> art bridge"). Indexed by TempBand.
const char* kOutfit[] = {
    "S:/art/weather/outfit_freezing.bin", "S:/art/weather/outfit_cold.bin",
    "S:/art/weather/outfit_mild.bin", "S:/art/weather/outfit_warm.bin",
    "S:/art/weather/outfit_hot.bin"};
}  // namespace

ArtSlots artSlots(TempBand band, Condition cond, bool isDay) {
  ArtSlots s{kOutfit[static_cast<int>(band)], nullptr, nullptr};
  switch (cond) {
    case Condition::Clear:
      if (isDay) s.overlay = "S:/art/weather/ov_sunglasses.bin";
      s.background = isDay ? "S:/art/weather/bg_clear.bin"
                           : "S:/art/weather/bg_clear_night.bin";
      break;
    case Condition::Cloudy:
      s.background = "S:/art/weather/bg_cloudy.bin";
      break;
    case Condition::Fog:
      s.background = "S:/art/weather/bg_fog.bin";
      break;
    case Condition::Rain:
      s.overlay = "S:/art/weather/ov_umbrella.bin";
      s.background = "S:/art/weather/bg_rain.bin";
      break;
    case Condition::Snow:
      s.overlay = "S:/art/weather/ov_scarf.bin";
      s.background = "S:/art/weather/bg_snow.bin";
      break;
    case Condition::Storm:
      s.overlay = "S:/art/weather/ov_umbrella.bin";
      s.background = "S:/art/weather/bg_storm.bin";
      break;
    default:
      break;  // Unknown: plain background, no accessory
  }
  return s;
}
