// Settings -> Bluetooth (spec §5): scan, pick, connect, forget a speaker.
// This is where the Music app redirects when nothing is paired (spec §4.2).
// Also hosts the F5 E2E gate: the "Play test tone" row.
#include <lvgl.h>

#include <cmath>
#include <string>

#include "apps/settings/Sections.h"
#include "core/App.h"  // RadioMode
#include "services/BluetoothAudioService.h"
#include "services/RadioManager.h"

namespace {
struct BtUi {
  RadioManager* radio;
  BluetoothAudioService* bt;
  lv_obj_t* status;
  lv_obj_t* list;
  lv_obj_t* pairedRow;   // "Pareada: <addr>" + Conectar/Esquecer
  float tonePhase;
  lv_timer_t* toneTimer;  // pending test-tone timer, if any
  bool radioUp;           // radio.request(Bluetooth) result — gates anything
                          // that would drive the BT stack
};
BtUi ui;  // one Settings screen at a time (single LVGL task) — safe

void setStatus(const char* msg) {
  lv_label_set_text(ui.status, msg);
  lv_refr_now(nullptr);  // repaint before blocking scan/connect
}

int32_t toneSource(int16_t* buf, int32_t frames, void* ctx) {
  if (ctx == nullptr) return 0;
  float* phase = static_cast<float*>(ctx);
  constexpr float kStep = 2.0f * 3.14159265f * 440.0f / 44100.0f;
  for (int32_t i = 0; i < frames; ++i) {
    const int16_t s = static_cast<int16_t>(8000.0f * sinf(*phase));
    buf[i * 2] = s;
    buf[i * 2 + 1] = s;
    *phase += kStep;
    if (*phase > 6.2831853f) *phase -= 6.2831853f;
  }
  return frames;
}

void toneTimerDone(lv_timer_t* t) {
  ui.bt->setSource(nullptr, nullptr);  // back to silence
  ui.toneTimer = nullptr;
  lv_timer_del(t);
  setStatus("Tom concluído " LV_SYMBOL_OK);
}

void toneClicked(lv_event_t*) {
  if (!ui.bt->isConnected()) {
    setStatus("Conecte uma caixa primeiro");
    return;
  }
  ui.tonePhase = 0.0f;
  ui.bt->setSource(toneSource, &ui.tonePhase);
  setStatus("Tocando 440 Hz...");
  ui.toneTimer = lv_timer_create(toneTimerDone, 2000, nullptr);
}

void rebuildPairedRow();

void connectTo(const BtDevice& d) {
  if (!ui.radioUp) {
    setStatus("Bluetooth indisponível");
    return;
  }
  setStatus("Conectando...");
  if (ui.bt->connect(d.addr)) {
    // Reconnect passes a nameless BtDevice — don't clobber the stored bt.name.
    if (!d.name.empty()) ui.bt->savePaired(d);
    setStatus("Conectado " LV_SYMBOL_OK);
  } else {
    setStatus("Não foi possível conectar");
  }
  rebuildPairedRow();
}

void deviceClicked(lv_event_t* e) {
  const BtDevice* d = static_cast<BtDevice*>(lv_event_get_user_data(e));
  connectTo(*d);
}

void scanClicked(lv_event_t*) {
  setStatus("Buscando ~8 s (caixa em pareamento?)");
  lv_obj_clean(ui.list);
  auto found = ui.bt->scan();
  if (found.empty()) lv_list_add_text(ui.list, "Nenhuma caixa encontrada");
  for (auto& d : found) {
    lv_obj_t* btn = lv_list_add_btn(ui.list, LV_SYMBOL_BLUETOOTH, d.name.c_str());
    // Copy the device into button-owned memory (found dies with this scope).
    BtDevice* owned = new BtDevice(d);
    lv_obj_add_event_cb(btn, deviceClicked, LV_EVENT_CLICKED, owned);
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* ev) {
          delete static_cast<BtDevice*>(lv_event_get_user_data(ev));
        },
        LV_EVENT_DELETE, owned);
  }
  setStatus("Toque numa caixa para parear");
}

void reconnectClicked(lv_event_t*) {
  BtDevice d{"", ui.bt->pairedAddr()};
  connectTo(d);
}

void forgetClicked(lv_event_t*) {
  ui.bt->disconnect();
  ui.bt->forgetPaired();
  setStatus("Esquecida");
  rebuildPairedRow();
}

void rebuildPairedRow() {
  lv_obj_clean(ui.pairedRow);
  const std::string addr = ui.bt->pairedAddr();
  lv_obj_t* lbl = lv_label_create(ui.pairedRow);
  if (addr.empty()) {
    lv_label_set_text(lbl, "Nenhuma caixa pareada");
    return;
  }
  lv_label_set_text_fmt(lbl, "Pareada: %s", addr.c_str());

  lv_obj_t* conn = lv_btn_create(ui.pairedRow);
  lv_label_set_text(lv_label_create(conn), "Conectar");
  lv_obj_add_event_cb(conn, reconnectClicked, LV_EVENT_CLICKED, nullptr);
  if (!ui.radioUp) lv_obj_add_state(conn, LV_STATE_DISABLED);  // radio down

  lv_obj_t* forget = lv_btn_create(ui.pairedRow);
  lv_label_set_text(lv_label_create(forget), LV_SYMBOL_TRASH " Esquecer");
  lv_obj_add_event_cb(forget, forgetClicked, LV_EVENT_CLICKED, nullptr);
}

void bodyDeleted(lv_event_t*) {
  ui.bt->setSource(nullptr, nullptr);
  ui.radio->request(RadioMode::None);  // radio-while-open rule
  if (ui.toneTimer) {
    lv_timer_del(ui.toneTimer);
    ui.toneTimer = nullptr;
  }
}
}  // namespace

void buildBluetoothSection(lv_obj_t* parent, RadioManager& radio,
                           BluetoothAudioService& bt) {
  ui = {};
  ui.radio = &radio;
  ui.bt = &bt;
  // Set before rebuildPairedRow() runs below so it can consult ui.radioUp
  // when deciding whether to disable the "Conectar" button.
  ui.radioUp = radio.request(RadioMode::Bluetooth);

  ui.status = lv_label_create(parent);

  lv_obj_t* scanBtn = lv_btn_create(parent);
  lv_label_set_text(lv_label_create(scanBtn), LV_SYMBOL_REFRESH " Buscar");
  lv_obj_add_event_cb(scanBtn, scanClicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* toneBtn = lv_btn_create(parent);
  lv_label_set_text(lv_label_create(toneBtn),
                    LV_SYMBOL_AUDIO " Tocar tom de teste");
  lv_obj_add_event_cb(toneBtn, toneClicked, LV_EVENT_CLICKED, nullptr);

  ui.pairedRow = lv_obj_create(parent);
  lv_obj_set_width(ui.pairedRow, LV_PCT(100));
  lv_obj_set_flex_flow(ui.pairedRow, LV_FLEX_FLOW_ROW_WRAP);

  ui.list = lv_list_create(parent);
  lv_obj_set_width(ui.list, LV_PCT(100));
  lv_obj_set_flex_grow(ui.list, 1);

  rebuildPairedRow();
  lv_obj_add_event_cb(parent, bodyDeleted, LV_EVENT_DELETE, nullptr);

  if (ui.radioUp) {
    setStatus("Toque em Buscar para procurar caixas");
  } else {
    lv_obj_add_state(scanBtn, LV_STATE_DISABLED);  // no radio -> no scanning
    lv_obj_add_state(toneBtn, LV_STATE_DISABLED);  // no radio -> no tone
    setStatus("Bluetooth indisponível");
  }
}
