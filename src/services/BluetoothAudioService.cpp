#include "BluetoothAudioService.h"

#include <Arduino.h>
#include <BluetoothA2DPSource.h>
#include <bt_addr.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>

#include <cstring>

namespace {
// Lazily heap-allocated, never freed: the app links exactly one A2DP source
// for its whole life. This is a deliberate deviation from the brief's plain
// `BluetoothA2DPSource a2dp;` global — a *static* instance of this ~600-byte,
// non-trivial-ctor class overflows the ESP32's fixed 124580-byte dram0_0_seg
// by 3600 bytes once linked into the full app (LVGL + WiFi + SD already use
// most of that segment; the isolated Task-0 spike never combined them, so it
// never saw this). Constructing it on first use instead of at static-init
// time closed the overflow with ~9.8 KB of headroom to spare (see task-2
// report for the measured before/after). Do not revert to a plain global.
BluetoothA2DPSource* g_a2dpPtr = nullptr;
BluetoothA2DPSource& a2dp() {
  if (g_a2dpPtr == nullptr) g_a2dpPtr = new BluetoothA2DPSource();
  return *g_a2dpPtr;
}

// Trampoline state (single service instance; BT callbacks are C-style).
AudioSourceFn g_sourceFn = nullptr;
void* g_sourceCtx = nullptr;
std::vector<BtDevice>* g_scanOut = nullptr;
uint8_t g_targetAddr[6] = {};
bool g_haveTarget = false;

int32_t frameBridge(Frame* frames, int32_t count) {
  // Frame is {int16_t channel1, channel2} — memory-compatible with the
  // interleaved stereo buffer AudioSourceFn expects.
  int16_t* buf = reinterpret_cast<int16_t*>(frames);
  int32_t written = 0;
  if (g_sourceFn != nullptr) written = g_sourceFn(buf, count, g_sourceCtx);
  if (written < count) {
    memset(buf + written * 2, 0, static_cast<size_t>(count - written) * 4);
  }
  return count;  // always feed a full buffer; the tail is silence
}

void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
  if (event != ESP_BT_GAP_DISC_RES_EVT || g_scanOut == nullptr) return;

  std::string name;
  for (int i = 0; i < param->disc_res.num_prop; ++i) {
    esp_bt_gap_dev_prop_t& p = param->disc_res.prop[i];
    if (p.type == ESP_BT_GAP_DEV_PROP_BDNAME) {
      name.assign(static_cast<char*>(p.val), p.len);
    } else if (p.type == ESP_BT_GAP_DEV_PROP_EIR) {
      uint8_t len = 0;
      uint8_t* eirName = esp_bt_gap_resolve_eir_data(
          static_cast<uint8_t*>(p.val), ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
      if (eirName != nullptr && len > 0) {
        name.assign(reinterpret_cast<char*>(eirName), len);
      }
    }
  }
  if (name.empty()) return;  // nameless results are unpickable in the UI

  const std::string addr = formatBtAddr(param->disc_res.bda);
  for (auto& d : *g_scanOut) {
    if (d.addr == addr) return;  // dedupe repeated inquiry responses
  }
  g_scanOut->push_back({name, addr});
}

// A2DP-source device filter: accept only the device we were asked to connect.
bool ssidFilter(const char* /*ssid*/, esp_bd_addr_t address, int /*rssi*/) {
  return g_haveTarget && memcmp(address, g_targetAddr, 6) == 0;
}
}  // namespace

bool BluetoothAudioService::powerOn() {
  if (!btStarted() && !btStart()) return false;
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    if (esp_bluedroid_init() != ESP_OK) return false;
  }
  if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    if (esp_bluedroid_enable() != ESP_OK) return false;
  }
  return true;
}

void BluetoothAudioService::powerOff() {
  g_sourceFn = nullptr;
  // Only tear down A2DP if it was ever brought up — calling end() would
  // otherwise lazily construct the (heap-allocated) source needlessly.
  if (g_a2dpPtr != nullptr) {
    g_a2dpPtr->end(false);  // false: keep controller memory — BT restarts later
  }
  if (btStarted()) btStop();
  Serial.printf("[bt] off, heap=%u\n", esp_get_free_heap_size());
}

std::vector<BtDevice> BluetoothAudioService::scan(uint32_t ms) {
  std::vector<BtDevice> out;
  g_scanOut = &out;
  esp_bt_gap_register_callback(gapCallback);
  // Inquiry length is in 1.28 s units, clamped to GAP's 0x01..0x30 range.
  uint8_t len = static_cast<uint8_t>(ms / 1280);
  if (len < 1) len = 1;
  if (len > 0x30) len = 0x30;
  esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, len, 0);
  delay(ms);
  esp_bt_gap_cancel_discovery();
  g_scanOut = nullptr;
  Serial.printf("[bt] scan found %u device(s)\n",
                static_cast<unsigned>(out.size()));
  return out;
}

bool BluetoothAudioService::connect(const std::string& addr) {
  if (!parseBtAddr(addr, g_targetAddr)) return false;
  g_haveTarget = true;

  a2dp().set_data_callback_in_frames(frameBridge);
  a2dp().set_ssid_callback(ssidFilter);
  a2dp().set_auto_reconnect(false);  // RadioManager owns when BT lives; no
                                      // background reconnect attempts
  // The lib may retain the name pointer — keep the backing string alive.
  static std::string liveName;
  liveName = store_.getString("bt.name", "");
  a2dp().start(liveName.c_str());

  const uint32_t start = millis();
  while (!a2dp().is_connected() && millis() - start < 15000) delay(100);
  Serial.printf("[bt] connect %s: %s\n", addr.c_str(),
                a2dp().is_connected() ? "ok" : "FAILED");
  return a2dp().is_connected();
}

void BluetoothAudioService::disconnect() {
  g_haveTarget = false;
  if (g_a2dpPtr != nullptr) g_a2dpPtr->disconnect();
}

bool BluetoothAudioService::isConnected() const {
  return g_a2dpPtr != nullptr && g_a2dpPtr->is_connected();
}

void BluetoothAudioService::setSource(AudioSourceFn fn, void* ctx) {
  g_sourceCtx = ctx;
  g_sourceFn = fn;
}

std::string BluetoothAudioService::pairedAddr() const {
  return store_.getString("bt.addr", "");
}

void BluetoothAudioService::savePaired(const BtDevice& d) {
  store_.setString("bt.addr", d.addr);
  store_.setString("bt.name", d.name);
}

void BluetoothAudioService::forgetPaired() {
  store_.remove("bt.addr");
  store_.remove("bt.name");
}
