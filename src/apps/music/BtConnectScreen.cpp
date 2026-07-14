#include "apps/music/BtConnectScreen.h"

#include <string>

#include "services/BluetoothAudioService.h"

namespace {
struct BtUi {
  BluetoothAudioService* bt;
  std::function<void()> onConnected;  // fired once the A2DP link is up
  lv_obj_t* status;
  lv_obj_t* list;
  lv_obj_t* pairedRow;       // "Pareada: <addr>" + Conectar/Esquecer
  lv_timer_t* connectTimer;  // pending async-connect poll, if any
  BtDevice connectTarget;    // device being connected (persist it on success)
  uint32_t connectStart;     // lv_tick at attempt start (for the timeout)
};
BtUi ui;  // one Music connect screen at a time (single LVGL task) — safe

void setStatus(const char* msg) {
  lv_label_set_text(ui.status, msg);
  lv_refr_now(nullptr);  // repaint before the blocking scan/connect
}

void rebuildPairedRow();

// Async connect (bt->beginConnect is non-blocking). The library re-inquires for
// ~13 s (one cycle) before it even attempts the link, and may run a second cycle
// if the speaker isn't seen on the first — so the cutoff must clear two inquiry
// cycles plus SDP/AVDTP setup, or a slow-to-answer speaker reads as a failure.
// Polling isConnected() (rather than a blocking wait) keeps the UI live and
// repainting the whole time.
constexpr uint32_t kConnectPollMs = 500;
constexpr uint32_t kConnectTimeoutMs = 35000;

void finishConnect(lv_timer_t* t) {
  ui.connectTimer = nullptr;
  lv_timer_del(t);
  rebuildPairedRow();  // reflect the new paired device (or restore on failure)
}

void connectPoll(lv_timer_t* t) {
  if (ui.bt->isConnected()) {
    // Reconnect passes a nameless BtDevice — don't clobber the stored bt.name.
    if (!ui.connectTarget.name.empty()) ui.bt->savePaired(ui.connectTarget);
    setStatus("Conectado " LV_SYMBOL_OK);
    auto cb = ui.onConnected;  // copy: the callback rebuilds the widget tree
    finishConnect(t);
    if (cb) cb();  // MusicApp swaps in the player (deferred via lv_async_call)
  } else if (lv_tick_elaps(ui.connectStart) >= kConnectTimeoutMs) {
    setStatus("Não foi possível conectar");
    finishConnect(t);
  }
}

void connectTo(const BtDevice& d) {
  if (ui.connectTimer != nullptr) return;  // an attempt is already in flight
  if (!ui.bt->beginConnect(d.addr)) {
    setStatus("Endereço inválido");
    return;
  }
  ui.connectTarget = d;
  ui.connectStart = lv_tick_get();
  setStatus("Conectando... (pode levar ~20 s)");
  ui.connectTimer = lv_timer_create(connectPoll, kConnectPollMs, nullptr);
}

void deviceClicked(lv_event_t* e) {
  const BtDevice* d = static_cast<BtDevice*>(lv_event_get_user_data(e));
  connectTo(*d);
}

void scanClicked(lv_event_t*) {
  if (ui.connectTimer != nullptr) {  // scan()'s GAP inquiry would collide with
    setStatus("Aguarde a conexão terminar");  // the in-flight connect's own
    return;                                    // discovery — refuse until done
  }
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
  setStatus("Toque numa caixa para conectar");
}

void reconnectClicked(lv_event_t*) {
  BtDevice d{"", ui.bt->pairedAddr()};  // fast reconnect by addr (no re-pair)
  connectTo(d);
}

void forgetClicked(lv_event_t*) {
  if (ui.connectTimer != nullptr) {  // don't forget mid-connect: the poll would
    setStatus("Aguarde a conexão terminar");  // then race the stored addr
    return;
  }
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

  lv_obj_t* forget = lv_btn_create(ui.pairedRow);
  lv_label_set_text(lv_label_create(forget), LV_SYMBOL_TRASH " Esquecer");
  lv_obj_add_event_cb(forget, forgetClicked, LV_EVENT_CLICKED, nullptr);
}

void bodyDeleted(lv_event_t*) {
  // Cancel a pending connect poll so it can't fire against deleted widgets —
  // this fires both on the connect→player swap and on app exit, because Music
  // builds this screen into a child container of its root (see Task 5).
  if (ui.connectTimer) {
    lv_timer_del(ui.connectTimer);
    ui.connectTimer = nullptr;
  }
}
}  // namespace

void buildBtConnectScreen(lv_obj_t* parent, BluetoothAudioService& bt,
                          std::function<void()> onConnected) {
  ui = {};
  ui.bt = &bt;
  ui.onConnected = std::move(onConnected);

  ui.status = lv_label_create(parent);

  lv_obj_t* scanBtn = lv_btn_create(parent);
  lv_label_set_text(lv_label_create(scanBtn), LV_SYMBOL_REFRESH " Buscar");
  lv_obj_add_event_cb(scanBtn, scanClicked, LV_EVENT_CLICKED, nullptr);

  ui.pairedRow = lv_obj_create(parent);
  lv_obj_set_width(ui.pairedRow, LV_PCT(100));
  // Height must hug its content: a plain lv_obj defaults to LV_DPI_DEF (130 px)
  // tall, which would eat the vertical space and squeeze the flex_grow scan list
  // below it down to a few unusable pixels.
  lv_obj_set_height(ui.pairedRow, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(ui.pairedRow, LV_FLEX_FLOW_ROW_WRAP);

  ui.list = lv_list_create(parent);
  lv_obj_set_width(ui.list, LV_PCT(100));
  lv_obj_set_flex_grow(ui.list, 1);

  rebuildPairedRow();
  lv_obj_add_event_cb(parent, bodyDeleted, LV_EVENT_DELETE, nullptr);

  setStatus("Toque em Buscar para procurar caixas");
}
