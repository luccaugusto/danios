#include "SettingsService.h"

void SettingsService::begin() {
  prefs_.begin("danios", /*readOnly=*/false);
}

uint32_t SettingsService::getU32(const char* key, uint32_t def) {
  return prefs_.getUInt(key, def);
}
void SettingsService::setU32(const char* key, uint32_t v) {
  prefs_.putUInt(key, v);
}

int32_t SettingsService::getI32(const char* key, int32_t def) {
  return prefs_.getInt(key, def);
}
void SettingsService::setI32(const char* key, int32_t v) {
  prefs_.putInt(key, v);
}

float SettingsService::getFloat(const char* key, float def) {
  return prefs_.getFloat(key, def);
}
void SettingsService::setFloat(const char* key, float v) {
  prefs_.putFloat(key, v);
}

bool SettingsService::getBool(const char* key, bool def) {
  return prefs_.getBool(key, def);
}
void SettingsService::setBool(const char* key, bool v) {
  prefs_.putBool(key, v);
}

std::string SettingsService::getString(const char* key, const std::string& def) {
  String s = prefs_.getString(key, String(def.c_str()));
  return std::string(s.c_str());
}
void SettingsService::setString(const char* key, const std::string& v) {
  prefs_.putString(key, v.c_str());
}

void SettingsService::remove(const char* key) {
  prefs_.remove(key);
}
