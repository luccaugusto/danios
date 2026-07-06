// src/core/App.h
#pragma once
#include <lvgl.h>

enum class RadioMode : uint8_t { None, WiFi, Bluetooth };

class App {
public:
  virtual ~App() = default;
  virtual const char* id() const = 0;            // stable key: "weather", "music",
                                                 // "calc", "oracle", "pet", "settings"
  virtual const char* title() const = 0;         // label under launcher icon
  virtual const char* iconPath() const = 0;      // "S:/art/icons/<id>.bin", or
                                                 // nullptr → launcher draws fallback
  virtual RadioMode requiredRadio() const = 0;
  virtual void onEnter() = 0;                    // called before buildUI
  virtual void buildUI(lv_obj_t* parent) = 0;    // build widgets into parent
  virtual void onExit() = 0;                     // launcher deletes widgets AFTER this
  virtual void tick(uint32_t now_ms) {}          // called every loop while active
};
