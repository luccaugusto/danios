// danios WiFiService — scan/credentials/connect (spec §3.1). Assumes the
// radio is already in RadioState::WiFiOn (RadioManager owns power).
#pragma once
#include <settings_store.h>

#include <string>
#include <vector>

struct WifiNetwork {
  std::string ssid;
  int8_t rssi;
  bool secured;
};

class WiFiService {
 public:
  explicit WiFiService(ISettingsStore& store) : store_(store) {}

  bool hasCredentials() const;
  void setCredentials(const std::string& ssid, const std::string& pass);  // + saves
  void forget();
  std::vector<WifiNetwork> scan();               // blocking, ~2 s
  bool connect(uint32_t timeout_ms = 15000);     // stored creds; blocking
  bool isConnected() const;

 private:
  ISettingsStore& store_;
};
