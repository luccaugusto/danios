// src/apps/weather/WeatherFetch.h — the fetch/cache pipeline shared by the
// boot-time prefetch (spec §3.4 step 4) and the Weather app (A3). Callers
// must already have WiFi connected (RadioManager/WiFiService are their job —
// this file never touches radio power).
#pragma once

#include <settings_store.h>

class TimeService;

// Geolocate if needed (auto mode + no stored coords, or first fetch of a new
// local day — which also refreshes the fixed TZ offset), fetch the Open-Meteo
// forecast, and cache it (NVS wx.json + wx.day). Returns true when a fresh
// forecast was parsed and cached.
bool weatherRefresh(ISettingsStore& store, TimeService& time);
