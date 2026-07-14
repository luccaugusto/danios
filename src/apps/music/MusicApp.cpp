#include "apps/music/MusicApp.h"

#include <Arduino.h>  // delay() in onExit
#include <esp_heap_caps.h>

#include <cstdint>
#include <cstdio>
#include <string>

#include "apps/music/BtConnectScreen.h"
#include "services/BluetoothAudioService.h"
#include "services/StorageService.h"

namespace {
MusicApp* g_self = nullptr;  // single instance (same pattern as SettingsApp)

// LVGL-heap guard: every list row is an lv_btn + label out of the 24 KB LVGL
// pool (F5 halved the budgeted 48 KB to leave headroom for bluedroid — see
// include/lv_conf.h). Tracks beyond the cap still play (next/previous cycle
// through the whole playlist); they just aren't tappable rows. The cap is
// visible in the UI ("+N"), never silent.
constexpr int kMaxListRows = 28;
}  // namespace

void MusicApp::onExit() {
  bt_.setSource(nullptr, nullptr);  // detach BEFORE the pipeline dies
  delay(20);                        // let an in-flight BT callback drain
  player_.reset();
  root_ = nowPlaying_ = playPauseLabel_ = list_ = nullptr;
  // The launcher requests RadioMode::None after this — BT teardown is its job.
}

void MusicApp::buildUI(lv_obj_t* parent) {
  g_self = this;
  root_ = parent;
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(root_, 8, 0);
  lv_obj_set_style_pad_row(root_, 8, 0);

  // Spec §4.2 steps 2-3: not linked yet -> show the connect screen. The A2DP
  // link is app-scoped and always down on entry, so this is where every
  // session starts (scan/pair/connect/forget). BtConnectScreen calls back on a
  // successful connect and we rebuild into the player. It lives in a CHILD of
  // root_ so its LV_EVENT_DELETE cleanup fires both on the swap below
  // (lv_obj_clean(root_)) and on app exit (the launcher cleans the container).
  if (!bt_.isConnected()) {
    lv_obj_t* body = lv_obj_create(root_);
    lv_obj_remove_style_all(body);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, 12, 0);
    // Defer the rebuild: onConnected runs from inside the connect screen's own
    // timer callback, and asyncRebuild deletes those widgets (LVGL rule).
    buildBtConnectScreen(body, bt_,
                         []() { lv_async_call(asyncRebuild, g_self); });
    return;
  }

  // Spec §4.2 step 4: /music -> playlist (sorted basenames, F3 contract).
  playlist_.setFiles(storage_.listFiles("/music", ".mp3"));
  if (playlist_.count() == 0) {
    showMessage(
        "Nenhuma música no cartão.\n"
        "Coloque arquivos .mp3 na pasta /music.");
    return;
  }

  // Connected: allocate the pipeline (RAM strategy) and start the player.
  if (!player_) player_.reset(new Mp3Player());
  Serial.printf("[music] heap after pipeline: %u free, %u largest\n",
                (unsigned)esp_get_free_heap_size(),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  bt_.setSource(&Mp3Player::sourceCallback, player_.get());

  buildPlayerUI();
  if (!openTrack(true)) handleBadTrack();  // load track 1, start paused —
                                           // the user picks (roadmap §1 E2E)
}

void MusicApp::tick(uint32_t /*now_ms*/) {
  if (!player_ || !player_->playing()) return;
  switch (player_->feed()) {
    case Mp3Player::Feed::Ok:
      break;
    case Mp3Player::Feed::End:
      // Track finished: auto-advance with wraparound. No ring flush — the
      // buffered tail plays out while the next track fills in behind it.
      if (!playlist_.next()) {
        player_->setPlaying(false);
        refreshNowPlaying();
        break;
      }
      if (!openTrack(false)) handleBadTrack();
      break;
    case Mp3Player::Feed::Error:
      handleBadTrack();  // spec §6.5: bad/unreadable MP3 -> skip to next
      break;
  }
}

void MusicApp::showMessage(const char* text) {
  lv_obj_clean(root_);
  nowPlaying_ = playPauseLabel_ = list_ = nullptr;  // died with the clean
  lv_obj_t* lbl = lv_label_create(root_);
  lv_obj_set_width(lbl, LV_PCT(100));
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_label_set_text(lbl, text);
}

void MusicApp::buildPlayerUI() {
  lv_obj_clean(root_);

  nowPlaying_ = lv_label_create(root_);
  lv_obj_set_width(nowPlaying_, LV_PCT(100));
  lv_label_set_long_mode(nowPlaying_, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_label_set_text(nowPlaying_, "");

  // Transport row: previous / play-pause / next.
  lv_obj_t* row = lv_obj_create(root_);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), 48);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t* prev = lv_btn_create(row);
  lv_obj_t* prevLbl = lv_label_create(prev);
  lv_label_set_text(prevLbl, LV_SYMBOL_PREV);
  lv_obj_add_event_cb(prev, prevClicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* pp = lv_btn_create(row);
  playPauseLabel_ = lv_label_create(pp);
  lv_label_set_text(playPauseLabel_, LV_SYMBOL_PLAY);
  lv_obj_add_event_cb(pp, playPauseClicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* next = lv_btn_create(row);
  lv_obj_t* nextLbl = lv_label_create(next);
  lv_label_set_text(nextLbl, LV_SYMBOL_NEXT);
  lv_obj_add_event_cb(next, nextClicked, LV_EVENT_CLICKED, nullptr);

  // Volume row (software gain — AVRCP is a spec non-goal; session-only).
  lv_obj_t* volRow = lv_obj_create(root_);
  lv_obj_remove_style_all(volRow);
  lv_obj_set_size(volRow, LV_PCT(100), 24);
  lv_obj_set_flex_flow(volRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(volRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(volRow, 8, 0);
  lv_obj_t* volIcon = lv_label_create(volRow);
  lv_label_set_text(volIcon, LV_SYMBOL_VOLUME_MAX);
  lv_obj_t* vol = lv_slider_create(volRow);
  lv_slider_set_range(vol, 0, 100);
  lv_slider_set_value(vol, player_->volume(), LV_ANIM_OFF);
  lv_obj_set_flex_grow(vol, 1);
  lv_obj_add_event_cb(vol, volumeChanged, LV_EVENT_VALUE_CHANGED, nullptr);

  list_ = lv_list_create(root_);
  lv_obj_set_width(list_, LV_PCT(100));
  lv_obj_set_flex_grow(list_, 1);
  refreshList();
}

bool MusicApp::openTrack(bool flushRing) {
  const int cur = playlist_.currentIndex();
  if (cur < 0) return false;
  const std::string path = std::string("/music/") + playlist_.fileAt(cur);
  if (!player_->open(path.c_str(), flushRing)) return false;
  refreshNowPlaying();
  refreshList();
  return true;
}

void MusicApp::changeTrack(int dir) {
  const bool wasPlaying = player_->playing();
  const bool moved = (dir > 0) ? playlist_.next() : playlist_.previous();
  if (!moved) return;
  if (openTrack(true)) {
    player_->setPlaying(wasPlaying);
    refreshNowPlaying();
  } else {
    handleBadTrack();
  }
}

void MusicApp::handleBadTrack() {
  // Spec §6.5: skip to the next track; grey the bad one out. When everything
  // is bad, land on a friendly state — never a silent dead player.
  const bool wasPlaying = player_->playing();
  player_->setPlaying(false);
  while (playlist_.markCurrentBad()) {
    if (openTrack(true)) {
      player_->setPlaying(wasPlaying);
      refreshNowPlaying();
      return;
    }
  }
  // Reachable synchronously from prevClicked/nextClicked (via changeTrack):
  // this lv_obj_clean(root_) runs from inside the click handler that owns the
  // clicked button. Safe in vendored LVGL 8.4 — event_send_core guards on the
  // widget's deleted flag after each callback — but re-verify on any LVGL
  // upgrade.
  showMessage(
      "Nenhuma música tocável no cartão.\n"
      "Use arquivos .mp3 de 44.1 kHz na pasta /music.");
}

void MusicApp::refreshNowPlaying() {
  if (nowPlaying_ == nullptr) return;
  const int cur = playlist_.currentIndex();
  lv_label_set_text(nowPlaying_,
                    cur >= 0 ? playlist_.titleAt(cur).c_str() : "-");
  lv_label_set_text(playPauseLabel_,
                    player_->playing() ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

void MusicApp::refreshList() {
  if (list_ == nullptr) return;
  lv_obj_clean(list_);
  const int shown =
      playlist_.count() < kMaxListRows ? playlist_.count() : kMaxListRows;
  for (int i = 0; i < shown; ++i) {
    const bool current = (i == playlist_.currentIndex());
    lv_obj_t* btn =
        lv_list_add_btn(list_, current ? LV_SYMBOL_PLAY : LV_SYMBOL_AUDIO,
                        playlist_.titleAt(i).c_str());
    if (playlist_.isBad(i)) lv_obj_set_style_opa(btn, LV_OPA_40, 0);
    lv_obj_add_event_cb(btn, trackClicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    if (current) lv_obj_scroll_to_view(btn, LV_ANIM_OFF);
  }
  if (playlist_.count() > shown) {
    char more[32];
    std::snprintf(more, sizeof(more), "+%d não listadas",
                  playlist_.count() - shown);
    lv_list_add_text(list_, more);  // LVGL copies the text
  }
}

// --- event handlers ---------------------------------------------------------
// Handlers that rebuild the widget tree they run inside defer via
// lv_async_call (LVGL: never delete the current event target in its handler).

void MusicApp::playPauseClicked(lv_event_t* /*e*/) {
  g_self->player_->setPlaying(!g_self->player_->playing());
  g_self->refreshNowPlaying();
}

void MusicApp::prevClicked(lv_event_t* /*e*/) { g_self->changeTrack(-1); }

void MusicApp::nextClicked(lv_event_t* /*e*/) { g_self->changeTrack(+1); }

void MusicApp::volumeChanged(lv_event_t* e) {
  lv_obj_t* slider = lv_event_get_target(e);
  g_self->player_->setVolume(
      static_cast<uint8_t>(lv_slider_get_value(slider)));
}

void MusicApp::trackClicked(lv_event_t* e) {
  g_self->pendingSelect_ = static_cast<int>(
      reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  lv_async_call(asyncSelectTrack, g_self);  // refreshList deletes this button
}

void MusicApp::asyncSelectTrack(void* userData) {
  auto* self = static_cast<MusicApp*>(userData);
  if (self->root_ == nullptr || self->player_ == nullptr) return;  // app closed
  if (!self->playlist_.select(self->pendingSelect_)) return;  // bad/stale row
  if (self->openTrack(true)) {
    self->player_->setPlaying(true);  // tapping a song means "play it"
    self->refreshNowPlaying();
  } else {
    self->handleBadTrack();
  }
}

void MusicApp::asyncRebuild(void* userData) {
  // Deferred connect->player swap (BtConnectScreen's onConnected posts this).
  auto* self = static_cast<MusicApp*>(userData);
  if (self->root_ == nullptr) return;  // app closed meanwhile
  lv_obj_clean(self->root_);   // deletes the connect screen (its timer cleanup
  self->buildUI(self->root_);  // fires) -> now connected -> builds the player
}
