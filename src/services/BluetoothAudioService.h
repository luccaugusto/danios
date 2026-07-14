// danios BluetoothAudioService — A2DP source (spec §3.1): discover speakers,
// connect to the paired one, and stream whatever AudioSourceFn provides.
// Power state belongs to RadioManager (powerOn/powerOff are for it alone).
#pragma once
#include <settings_store.h>

#include <cstdint>
#include <string>
#include <vector>

struct BtDevice {
  std::string name;
  std::string addr;  // "AA:BB:CC:DD:EE:FF" (bt_addr wire format)
};

// Audio callback contract (roadmap §4.10): fill up to `frames` stereo int16
// frames at 44100 Hz, return frames actually written. Return 0 = silence.
using AudioSourceFn = int32_t (*)(int16_t* stereo_buf, int32_t frames,
                                  void* ctx);

class BluetoothAudioService {
 public:
  explicit BluetoothAudioService(ISettingsStore& store) : store_(store) {}

  // --- RadioManager-only power hooks (Task 3). Apps: use RadioManager. ---
  bool powerOn();   // controller + bluedroid up (idempotent)
  void powerOff();  // A2DP end + controller stop. NEVER releases BT Classic
                    // memory (esp_bt_controller_mem_release is one-way and
                    // Music re-enters Bluetooth every session).

  std::vector<BtDevice> scan(uint32_t ms = 8000);  // blocking GAP discovery
  // Non-blocking: kicks off the A2DP source connect and returns immediately
  // (true = attempt started, false = bad address). The library re-discovers
  // the target by inquiry (~13 s/cycle) before the link comes up, so callers
  // must poll isConnected() rather than expect a result here — see
  // BluetoothSection's connectPoll. Returning connected/false synchronously
  // would either lie (timeout too short) or freeze the UI for ~20 s.
  bool beginConnect(const std::string& addr);
  void disconnect();
  bool isConnected() const;

  void setSource(AudioSourceFn fn, void* ctx);  // Music plugs its decoder here

  std::string pairedAddr() const;  // NVS bt.addr ("" if none)
  void savePaired(const BtDevice& d);
  void forgetPaired();

 private:
  ISettingsStore& store_;
};
