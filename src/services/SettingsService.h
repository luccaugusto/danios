// NVS-backed ISettingsStore (namespace "danios"). Owned by main.cpp; everyone
// else takes ISettingsStore& — do not include this header outside main.cpp.
#pragma once
#include <Preferences.h>
#include <settings_store.h>

class SettingsService : public ISettingsStore {
public:
  void begin();  // opens NVS namespace "danios" read-write; call once in setup()

  uint32_t    getU32(const char* key, uint32_t def) override;
  void        setU32(const char* key, uint32_t v) override;
  int32_t     getI32(const char* key, int32_t def) override;
  void        setI32(const char* key, int32_t v) override;
  float       getFloat(const char* key, float def) override;
  void        setFloat(const char* key, float v) override;
  bool        getBool(const char* key, bool def) override;
  void        setBool(const char* key, bool v) override;
  std::string getString(const char* key, const std::string& def) override;
  void        setString(const char* key, const std::string& v) override;
  void        remove(const char* key) override;

private:
  Preferences prefs_;
};
