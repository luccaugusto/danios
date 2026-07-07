// Settings -> WiFi (spec §5): scan nearby networks, tap one, type the password
// on the LVGL keyboard, connect and save. Fully on-device, no phone involved.
#include <lvgl.h>

#include "apps/settings/Sections.h"
#include "core/App.h"  // RadioMode
#include "services/RadioManager.h"
#include "services/WiFiService.h"

namespace {
struct WifiUi {
  RadioManager* radio;
  WiFiService* wifi;
  lv_obj_t* body;
  lv_obj_t* list;        // scan results
  lv_obj_t* status;      // one-line status label
  char pendingSsid[33];  // network tapped, awaiting password
};
WifiUi ui;  // one Settings screen at a time (single LVGL task) — safe

void setStatus(const char* msg) {
  lv_label_set_text(ui.status, msg);
  lv_refr_now(nullptr);  // repaint before a blocking connect/scan
}

void rebuildForgetRow();

void connectWithPassword(const char* pass) {
  ui.wifi->setCredentials(ui.pendingSsid, pass);
  setStatus("Conectando...");
  if (ui.wifi->connect()) {
    setStatus("Conectado " LV_SYMBOL_OK);
  } else {
    setStatus("Falhou - verifique a senha");
  }
  rebuildForgetRow();
}

void keyboardEvent(lv_event_t* e) {
  lv_obj_t* kb = lv_event_get_current_target(e);
  lv_obj_t* ta = lv_keyboard_get_textarea(kb);
  if (lv_event_get_code(e) == LV_EVENT_READY) {  // checkmark pressed
    connectWithPassword(lv_textarea_get_text(ta));
  }
  // READY or CANCEL: tear the modal down. Async — never delete an ancestor
  // of the object whose event is still being dispatched.
  lv_obj_del_async(lv_obj_get_parent(kb));
}

void askPassword() {
  // Full-screen modal on the top layer: textarea + keyboard.
  lv_obj_t* modal = lv_obj_create(lv_layer_top());
  lv_obj_set_size(modal, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* prompt = lv_label_create(modal);
  lv_label_set_text_fmt(prompt, "Senha de %s:", ui.pendingSsid);

  lv_obj_t* ta = lv_textarea_create(modal);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_password_mode(ta, true);
  lv_obj_set_width(ta, LV_PCT(100));

  lv_obj_t* kb = lv_keyboard_create(modal);
  lv_keyboard_set_textarea(kb, ta);
  lv_obj_add_event_cb(kb, keyboardEvent, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(kb, keyboardEvent, LV_EVENT_CANCEL, nullptr);
}

void networkClicked(lv_event_t* e) {
  lv_obj_t* btn = lv_event_get_current_target(e);
  // Button text is "ssid  (-60)" — the SSID was stashed as user data instead.
  const char* ssid = static_cast<const char*>(lv_event_get_user_data(e));
  (void)btn;
  snprintf(ui.pendingSsid, sizeof(ui.pendingSsid), "%s", ssid);
  askPassword();
}

void scanClicked(lv_event_t*) {
  setStatus("Buscando...");
  lv_obj_clean(ui.list);
  auto nets = ui.wifi->scan();
  if (nets.empty()) {
    lv_list_add_text(ui.list, "Nenhuma rede encontrada");
  }
  for (auto& n : nets) {
    char row[64];
    snprintf(row, sizeof(row), "%s (%d)%s", n.ssid.c_str(), n.rssi,
             n.secured ? "" : " aberta");
    lv_obj_t* btn = lv_list_add_btn(ui.list, LV_SYMBOL_WIFI, row);
    // The button outlives `nets`; copy the SSID into the button's user data.
    char* owned = static_cast<char*>(lv_mem_alloc(n.ssid.size() + 1));
    memcpy(owned, n.ssid.c_str(), n.ssid.size() + 1);
    lv_obj_add_event_cb(btn, networkClicked, LV_EVENT_CLICKED, owned);
    lv_obj_add_event_cb(
        btn, [](lv_event_t* ev) { lv_mem_free(lv_event_get_user_data(ev)); },
        LV_EVENT_DELETE, owned);
  }
  setStatus("Toque numa rede para conectar");
}

void forgetClicked(lv_event_t*) {
  ui.wifi->forget();
  setStatus("Rede esquecida");
  rebuildForgetRow();
}

lv_obj_t* forgetBtn = nullptr;

void rebuildForgetRow() {
  if (forgetBtn) {
    lv_obj_del(forgetBtn);
    forgetBtn = nullptr;
  }
  if (!ui.wifi->hasCredentials()) return;
  forgetBtn = lv_btn_create(ui.body);
  lv_obj_t* lbl = lv_label_create(forgetBtn);
  lv_label_set_text(lbl, LV_SYMBOL_TRASH " Esquecer rede");
  lv_obj_add_event_cb(forgetBtn, forgetClicked, LV_EVENT_CLICKED, nullptr);
}

void bodyDeleted(lv_event_t*) {
  // Radio-while-open rule: release WiFi when the section goes away.
  forgetBtn = nullptr;
  ui.radio->request(RadioMode::None);
}
}  // namespace

void buildWifiSection(lv_obj_t* parent, RadioManager& radio,
                      WiFiService& wifi) {
  ui = {};
  ui.radio = &radio;
  ui.wifi = &wifi;
  ui.body = parent;

  ui.status = lv_label_create(parent);

  lv_obj_t* scanBtn = lv_btn_create(parent);
  lv_obj_t* scanLbl = lv_label_create(scanBtn);
  lv_label_set_text(scanLbl, LV_SYMBOL_REFRESH " Buscar");
  lv_obj_add_event_cb(scanBtn, scanClicked, LV_EVENT_CLICKED, nullptr);

  ui.list = lv_list_create(parent);
  lv_obj_set_width(ui.list, LV_PCT(100));
  lv_obj_set_flex_grow(ui.list, 1);

  rebuildForgetRow();
  lv_obj_add_event_cb(parent, bodyDeleted, LV_EVENT_DELETE, nullptr);

  if (radio.request(RadioMode::WiFi)) {
    setStatus(wifi.isConnected() ? "Conectado " LV_SYMBOL_OK
                                 : "Toque em Buscar para procurar redes");
  } else {
    setStatus("Rádio indisponível");
  }
}

// Temporary stub — Task 7 replaces this when ClockSection.cpp lands.
void buildClockSection(lv_obj_t* parent, TimeService&) {
  lv_label_set_text(lv_label_create(parent), "Relógio em breve");
}
