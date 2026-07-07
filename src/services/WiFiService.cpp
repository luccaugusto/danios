#include "WiFiService.h"

#include <Arduino.h>
#include <WiFi.h>

bool WiFiService::hasCredentials() const {
  return !store_.getString("wifi.ssid", "").empty();
}

void WiFiService::setCredentials(const std::string& ssid,
                                 const std::string& pass) {
  store_.setString("wifi.ssid", ssid);
  store_.setString("wifi.pass", pass);
}

void WiFiService::forget() {
  store_.remove("wifi.ssid");
  store_.remove("wifi.pass");
  WiFi.disconnect();
}

std::vector<WifiNetwork> WiFiService::scan() {
  std::vector<WifiNetwork> out;
  const int16_t n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/false);
  for (int16_t i = 0; i < n; ++i) {
    out.push_back({std::string(WiFi.SSID(i).c_str()),
                   static_cast<int8_t>(WiFi.RSSI(i)),
                   WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
  }
  WiFi.scanDelete();
  return out;
}

bool WiFiService::connect(uint32_t timeout_ms) {
  const std::string ssid = store_.getString("wifi.ssid", "");
  if (ssid.empty()) return false;
  const std::string pass = store_.getString("wifi.pass", "");

  WiFi.begin(ssid.c_str(), pass.c_str());
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeout_ms) {
      Serial.printf("[wifi] connect to \"%s\" timed out\n", ssid.c_str());
      return false;
    }
    delay(100);
  }
  Serial.printf("[wifi] connected, ip=%s\n", WiFi.localIP().toString().c_str());
  return true;
}

bool WiFiService::isConnected() const { return WiFi.status() == WL_CONNECTED; }
