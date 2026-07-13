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
