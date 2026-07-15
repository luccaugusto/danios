// src/apps/app_catalog.h — THE one place to edit an app's launcher name or
// icon. Every App's title()/iconPath() must return the fields of its entry
// here; never hardcode a title or icon path inside an app.
//
// - title: the label under the launcher icon (and the app screen's top bar).
// - icon:  LVGL path to an RGB565 .bin on the SD card, via the LVGL FS driver
//          on drive 'S' (registered in F3), e.g. "S:/art/icons/weather.bin".
//          nullptr → launcher draws its colored-letter fallback. Keep nullptr
//          until F3 lands AND the hand-drawn art file exists on the card.
//
// App ids ("weather", "music", "calc", "oracle", "pet", "settings") are pinned
// by the roadmap (§4.5) and used as NVS/navigation keys — do NOT change ids.
#pragma once

struct AppInfo {
  const char* title;  // launcher label
  const char* icon;   // "S:/art/icons/<x>.bin" or nullptr → fallback
};

namespace catalog {
inline constexpr AppInfo kWeather{"Clima", "S:/art/icons/weather.bin"};
inline constexpr AppInfo kMusic{"Música", nullptr};
inline constexpr AppInfo kCalc{"Calculadora", "S:/art/icons/calc.bin"};
inline constexpr AppInfo kOracle{"Oráculo", "S:/art/icons/oracle.bin"};
inline constexpr AppInfo kPet{"Bichinho", "S:/art/icons/pet.bin"};
inline constexpr AppInfo kPomodoro{"Pomodoro", "S:/art/icons/pomodoro.bin"};
inline constexpr AppInfo kSettings{"Configurações", "S:/art/icons/settings.bin"};
}  // namespace catalog
