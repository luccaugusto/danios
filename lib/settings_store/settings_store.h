// lib/settings_store/settings_store.h — std C++ only
#pragma once
#include <cstdint>
#include <map>
#include <string>

class ISettingsStore {
public:
  virtual ~ISettingsStore() = default;
  virtual uint32_t    getU32(const char* key, uint32_t def) = 0;
  virtual void        setU32(const char* key, uint32_t v) = 0;
  virtual int32_t     getI32(const char* key, int32_t def) = 0;
  virtual void        setI32(const char* key, int32_t v) = 0;
  virtual float       getFloat(const char* key, float def) = 0;
  virtual void        setFloat(const char* key, float v) = 0;
  virtual bool        getBool(const char* key, bool def) = 0;
  virtual void        setBool(const char* key, bool v) = 0;
  virtual std::string getString(const char* key, const std::string& def) = 0;
  virtual void        setString(const char* key, const std::string& v) = 0;
  virtual void        remove(const char* key) = 0;
};

// In-memory implementation for native tests. One map per type, matching NVS
// behavior where a key read as the wrong type yields the caller's default.
class FakeSettingsStore : public ISettingsStore {
public:
  uint32_t getU32(const char* key, uint32_t def) override {
    auto it = u32_.find(key);
    return it == u32_.end() ? def : it->second;
  }
  void setU32(const char* key, uint32_t v) override { u32_[key] = v; }

  int32_t getI32(const char* key, int32_t def) override {
    auto it = i32_.find(key);
    return it == i32_.end() ? def : it->second;
  }
  void setI32(const char* key, int32_t v) override { i32_[key] = v; }

  float getFloat(const char* key, float def) override {
    auto it = flt_.find(key);
    return it == flt_.end() ? def : it->second;
  }
  void setFloat(const char* key, float v) override { flt_[key] = v; }

  bool getBool(const char* key, bool def) override {
    auto it = bool_.find(key);
    return it == bool_.end() ? def : it->second;
  }
  void setBool(const char* key, bool v) override { bool_[key] = v; }

  std::string getString(const char* key, const std::string& def) override {
    auto it = str_.find(key);
    return it == str_.end() ? def : it->second;
  }
  void setString(const char* key, const std::string& v) override { str_[key] = v; }

  void remove(const char* key) override {
    u32_.erase(key);
    i32_.erase(key);
    flt_.erase(key);
    bool_.erase(key);
    str_.erase(key);
  }

private:
  std::map<std::string, uint32_t>    u32_;
  std::map<std::string, int32_t>     i32_;
  std::map<std::string, float>       flt_;
  std::map<std::string, bool>        bool_;
  std::map<std::string, std::string> str_;
};
