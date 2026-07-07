// Settings -> Clock (spec §5, §6.2): "Sync now" forces an NTP re-sync (borrowing
// WiFi via TimeService); manual date/time set covers the no-WiFi case and gives
// the Oracle a correct day to work from.
#include <lvgl.h>

#include <cstdio>

#include "apps/settings/Sections.h"
#include "services/TimeService.h"

namespace {
struct ClockUi {
  TimeService* time;
  lv_obj_t* status;
  lv_obj_t* year;
  lv_obj_t* month;
  lv_obj_t* day;
  lv_obj_t* hour;
  lv_obj_t* minute;
};
ClockUi ui;

void refreshStatus() {
  char clock[6];
  ui.time->hhmm(clock);
  const LocalDate d = ui.time->today();
  if (ui.time->isTimeKnown()) {
    lv_label_set_text_fmt(ui.status, "Agora: %04d-%02d-%02d %s", d.year, d.month,
                          d.day, clock);
  } else {
    lv_label_set_text(ui.status, "Relógio não ajustado");
  }
}

void syncClicked(lv_event_t*) {
  lv_label_set_text(ui.status, "Sincronizando...");
  lv_refr_now(nullptr);  // repaint before the blocking sync
  ui.time->syncNow();
  refreshStatus();
}

// Builds "2026\n2027\n..." style roller option strings once, statically.
lv_obj_t* makeRoller(lv_obj_t* parent, int from, int to, int sel) {
  static char buf[1024];
  size_t off = 0;
  for (int v = from; v <= to; ++v) {
    off += snprintf(buf + off, sizeof(buf) - off, "%02d%s", v,
                    v == to ? "" : "\n");
  }
  lv_obj_t* r = lv_roller_create(parent);
  lv_roller_set_options(r, buf, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(r, 3);
  lv_roller_set_selected(r, sel - from, LV_ANIM_OFF);
  return r;
}

void applyClicked(lv_event_t*) {
  const LocalDate d{
      static_cast<int16_t>(2026 + lv_roller_get_selected(ui.year)),
      static_cast<int8_t>(1 + lv_roller_get_selected(ui.month)),
      static_cast<int8_t>(1 + lv_roller_get_selected(ui.day))};
  // Day roller always offers 1..31; mktime in setManual normalizes overshoot
  // (e.g. Feb 31 -> Mar 2/3), documented behavior for this simple UI.
  ui.time->setManual(d, static_cast<int>(lv_roller_get_selected(ui.hour)),
                     static_cast<int>(lv_roller_get_selected(ui.minute)));
  refreshStatus();
}
}  // namespace

void buildClockSection(lv_obj_t* parent, TimeService& time) {
  ui = {};
  ui.time = &time;

  ui.status = lv_label_create(parent);

  lv_obj_t* syncBtn = lv_btn_create(parent);
  lv_obj_t* syncLbl = lv_label_create(syncBtn);
  lv_label_set_text(syncLbl, LV_SYMBOL_REFRESH " Sincronizar agora");
  lv_obj_add_event_cb(syncBtn, syncClicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* caption = lv_label_create(parent);
  lv_label_set_text(caption, "Ajuste manual  (A / M / D / h / m)");

  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  const LocalDate d = time.isTimeKnown() ? time.today() : LocalDate{2026, 7, 3};
  const int mins = time.isTimeKnown() ? time.minutesSinceMidnight() : 720;
  ui.year = makeRoller(row, 2026, 2045, d.year);
  ui.month = makeRoller(row, 1, 12, d.month);
  ui.day = makeRoller(row, 1, 31, d.day);
  ui.hour = makeRoller(row, 0, 23, mins / 60);
  ui.minute = makeRoller(row, 0, 59, mins % 60);

  lv_obj_t* applyBtn = lv_btn_create(parent);
  lv_obj_t* applyLbl = lv_label_create(applyBtn);
  lv_label_set_text(applyLbl, LV_SYMBOL_OK " Aplicar");
  lv_obj_add_event_cb(applyBtn, applyClicked, LV_EVENT_CLICKED, nullptr);

  refreshStatus();
}
