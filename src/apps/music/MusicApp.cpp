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

// LVGL-heap guard: every list row is an lv_btn + label out of the 18 KB LVGL
// pool (trimmed from the budgeted 48 KB for bluedroid + the Music pipeline —
// see include/lv_conf.h). The cap applies PER VIEW (albums view, each album's
// tracks view). Tracks beyond the cap still play (next/previous cycle through
// the whole playlist); they just aren't tappable rows. The cap is visible in
// the UI ("+N"), never silent.
constexpr int kMaxListRows = 28;

// List rows are single clickable labels, NOT lv_list buttons: a button row is
// 3 LVGL objects (~440 B measured) and its label defaults to a continuous
// scroll animation that allocates from the LVGL pool — a 16-row album
// exhausted the pool (measured 2026-07-14, lv_anim_start OOM assert, silent
// freeze). One ellipsized label per row is ~1/3 the pool cost, no anims; only
// the nowPlaying_ label scrolls.
lv_obj_t* addRowLabel(lv_obj_t* list, const char* icon, const char* text,
                      lv_event_cb_t cb, int userIdx) {
  lv_obj_t* lbl = lv_label_create(list);
  lv_obj_set_width(lbl, LV_PCT(100));
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
  lv_label_set_text_fmt(lbl, "%s  %s", icon, text);
  lv_obj_set_style_pad_ver(lbl, 8, 0);  // touch target ~36 px
  lv_obj_set_style_pad_hor(lbl, 4, 0);
  lv_obj_add_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(lbl, cb, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<intptr_t>(userIdx)));
  return lbl;
}
}  // namespace

void MusicApp::onExit() {
  bt_.setSource(nullptr, nullptr);  // detach BEFORE the pipeline dies
  delay(20);                        // let an in-flight BT callback drain
  player_.reset();
  root_ = nowPlaying_ = playPauseLabel_ = list_ = nullptr;
  view_ = View::Albums;  // fresh browse state next session (no persistence —
  albums_.clear();       // A4 owns no NVS keys)
  looseTracks_.clear();
  browseTracks_.clear();
  browseAlbum_.clear();
  playingPrefix_.clear();
  playlist_.setFiles({});  // stale current/bad state must not survive re-entry
                           // ("nothing preloads": next session starts empty)
  pendingSelect_ = pendingAlbum_ = -1;
  pendingSelectIsBrowse_ = false;
  // The launcher requests RadioMode::None after this — BT teardown is its job.
}

void MusicApp::buildUI(lv_obj_t* parent) {
  g_self = this;
  root_ = parent;
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(root_, 8, 0);
  lv_obj_set_style_pad_row(root_, 8, 0);

  // Pipeline (~24 KB: ~15.5 KB player object + 8 KB ring) is allocated BEFORE
  // the A2DP link comes up: the link itself costs ~14 KB, and the free heap
  // left after it (13-22 KB measured 2026-07-14) no longer fits the pipeline.
  // At this point only the BT stack is enabled (~43-51 KB free).
  if (!player_) {
    if (esp_get_free_heap_size() < 40 * 1024) {  // pipeline + link + margin
      showMessage(
          "Memória insuficiente para tocar música.\n"
          "Reinicie o aparelho e abra Música primeiro.");
      return;
    }
    player_.reset(new Mp3Player());
    Serial.printf("[music] heap after pipeline: %u free, %u largest\n",
                  (unsigned)esp_get_free_heap_size(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  }

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

  // A4.1: land on the albums view (folders = albums, one level deep, plus
  // loose root tracks). Nothing preloads — the playlist stays empty until the
  // first tap, so the transport starts as a no-op ("-" in now-playing).
  albums_ = storage_.listDirs("/music");
  looseTracks_ = storage_.listFiles("/music", ".mp3");
  if (albums_.empty() && looseTracks_.empty()) {
    showMessage(
        "Nenhuma música no cartão.\n"
        "Coloque arquivos .mp3 na pasta /music.");
    return;
  }

  // Connected: hook the (pre-allocated) pipeline to the A2DP source.
  bt_.setSource(&Mp3Player::sourceCallback, player_.get());

  buildPlayerUI();
  view_ = View::Albums;
  refreshList();
  refreshNowPlaying();  // "-" + play glyph until the first tap
}

void MusicApp::tick(uint32_t /*now_ms*/) {
  if (!player_ || !player_->playing()) return;
  switch (player_->feed()) {
    case Mp3Player::Feed::Ok:
      break;
    case Mp3Player::Feed::End:
      // Track finished: auto-advance with wraparound WITHIN the playing album.
      // No ring flush — the buffered tail plays out while the next track
      // fills in behind it.
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

bool MusicApp::handleBack() {
  // Launcher top-bar hook — runs from the launcher's own button, never from
  // inside list_'s event context, so rebuilding the list here is safe.
  if (list_ != nullptr && view_ == View::Tracks) {
    showAlbumsView();
    return true;  // consumed: stepped up one level
  }
  return false;  // albums view / message / connect screen -> launcher exits
}

void MusicApp::showMessage(const char* text) {
  lv_obj_clean(root_);
  nowPlaying_ = playPauseLabel_ = list_ = nullptr;  // died with the clean
  lv_obj_t* lbl = lv_label_create(root_);
  lv_obj_set_width(lbl, LV_PCT(100));
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_label_set_text(lbl, text);
}

void MusicApp::showAlbumsView() {
  view_ = View::Albums;
  browseAlbum_.clear();
  browseTracks_.clear();
  albums_ = storage_.listDirs("/music");             // re-read: stays fresh
  looseTracks_ = storage_.listFiles("/music", ".mp3");  // across card edits
  refreshList();
}

void MusicApp::showTracksView(int albumIdx) {
  view_ = View::Tracks;
  browseAlbum_ = albums_[static_cast<size_t>(albumIdx)];
  const std::string dir = "/music/" + browseAlbum_;
  Serial.printf("[music] album tap: '%s'\n", dir.c_str());  // TEMP DIAGNOSTIC
  browseTracks_ = storage_.listFiles(dir.c_str(), ".mp3");
  lv_mem_monitor_t m;  // TEMP DIAGNOSTIC (A4 freeze): bracket the UI build
  lv_mem_monitor(&m);
  Serial.printf("[music] %d tracks; lv pool before build: %u free, %u biggest\n",
                static_cast<int>(browseTracks_.size()),
                static_cast<unsigned>(m.free_size),
                static_cast<unsigned>(m.free_biggest_size));
  refreshList();
  lv_mem_monitor(&m);
  Serial.printf("[music] lv pool after build: %u free, %u biggest\n",
                static_cast<unsigned>(m.free_size),
                static_cast<unsigned>(m.free_biggest_size));
}

std::string MusicApp::browsePrefix() const {
  return "/music/" + browseAlbum_ + "/";
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
  // Callers pick the view and call refreshList() themselves.
}

bool MusicApp::openTrack(bool flushRing) {
  const int cur = playlist_.currentIndex();
  if (cur < 0 || playingPrefix_.empty()) return false;
  const std::string path = playingPrefix_ + playlist_.fileAt(cur);
  if (!player_->open(path.c_str(), flushRing)) return false;
  refreshNowPlaying();
  refreshList();
  return true;
}

void MusicApp::changeTrack(int dir) {
  const bool wasPlaying = player_->playing();
  const bool moved = (dir > 0) ? playlist_.next() : playlist_.previous();
  if (!moved) return;  // also covers the empty not-yet-loaded playlist
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
  if (view_ == View::Albums) {
    refreshAlbumsList();
  } else {
    refreshTracksList();
  }
}

void MusicApp::refreshAlbumsList() {
  const int nAlbums = static_cast<int>(albums_.size());
  const int total = nAlbums + static_cast<int>(looseTracks_.size());
  const int shown = total < kMaxListRows ? total : kMaxListRows;
  for (int i = 0; i < shown; ++i) {
    if (i < nAlbums) {
      addRowLabel(list_, LV_SYMBOL_DIRECTORY, albums_[i].c_str(),
                  albumClicked, i);
    } else {
      const int t = i - nAlbums;  // loose root track: plays directly
      const bool current =
          (playingPrefix_ == "/music/" && t == playlist_.currentIndex());
      lv_obj_t* row =
          addRowLabel(list_, current ? LV_SYMBOL_PLAY : LV_SYMBOL_AUDIO,
                      trackTitle(looseTracks_[t]).c_str(), trackClicked, t);
      if (playingPrefix_ == "/music/" && playlist_.isBad(t)) {
        lv_obj_set_style_opa(row, LV_OPA_40, 0);
      }
      if (current) lv_obj_scroll_to_view(row, LV_ANIM_OFF);
    }
  }
  if (total > shown) {
    char more[32];
    std::snprintf(more, sizeof(more), "+%d não listados", total - shown);
    lv_list_add_text(list_, more);  // LVGL copies the text
  }
}

void MusicApp::refreshTracksList() {
  if (browseTracks_.empty()) {
    lv_list_add_text(list_, "Nenhuma música neste álbum.");
    return;  // back still works (handleBack -> albums view)
  }
  // Highlight/grey only when the browsed album IS the playing one — indexes
  // into playlist_ only line up then.
  const bool playingThis = (playingPrefix_ == browsePrefix());
  const int total = static_cast<int>(browseTracks_.size());
  const int shown = total < kMaxListRows ? total : kMaxListRows;
  for (int i = 0; i < shown; ++i) {
    const bool current = playingThis && i == playlist_.currentIndex();
    lv_obj_t* row =
        addRowLabel(list_, current ? LV_SYMBOL_PLAY : LV_SYMBOL_AUDIO,
                    trackTitle(browseTracks_[i]).c_str(), trackClicked, i);
    if (playingThis && playlist_.isBad(i)) {
      lv_obj_set_style_opa(row, LV_OPA_40, 0);
    }
    if (current) lv_obj_scroll_to_view(row, LV_ANIM_OFF);
  }
  if (total > shown) {
    char more[32];
    std::snprintf(more, sizeof(more), "+%d não listadas", total - shown);
    lv_list_add_text(list_, more);  // LVGL copies the text
  }
}

// --- event handlers ---------------------------------------------------------
// Handlers that rebuild the widget tree they run inside defer via
// lv_async_call (LVGL: never delete the current event target in its handler).

void MusicApp::playPauseClicked(lv_event_t* /*e*/) {
  if (g_self->playlist_.currentIndex() < 0) return;  // nothing loaded yet
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

void MusicApp::albumClicked(lv_event_t* e) {
  g_self->pendingAlbum_ = static_cast<int>(
      reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  lv_async_call(asyncOpenAlbum, g_self);  // showTracksView deletes this button
}

void MusicApp::asyncOpenAlbum(void* userData) {
  auto* self = static_cast<MusicApp*>(userData);
  if (self->root_ == nullptr || self->list_ == nullptr) return;  // app closed
  if (self->pendingAlbum_ < 0 ||
      self->pendingAlbum_ >= static_cast<int>(self->albums_.size())) {
    return;  // stale row
  }
  self->showTracksView(self->pendingAlbum_);
}

void MusicApp::trackClicked(lv_event_t* e) {
  g_self->pendingSelect_ = static_cast<int>(
      reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  // Albums-view loose rows target looseTracks_; tracks-view rows target
  // browseTracks_. Captured at click time; asyncSelectTrack re-checks the
  // view before acting (a synchronous handleBack can flip it in between).
  g_self->pendingSelectIsBrowse_ = (g_self->view_ == View::Tracks);
  lv_async_call(asyncSelectTrack, g_self);  // refreshList deletes this button
}

void MusicApp::asyncSelectTrack(void* userData) {
  auto* self = static_cast<MusicApp*>(userData);
  if (self->root_ == nullptr || self->player_ == nullptr) return;  // app closed
  const bool fromBrowse = self->pendingSelectIsBrowse_;
  if (fromBrowse != (self->view_ == View::Tracks)) return;  // view changed
                                             // between the tap and this call
  const std::string prefix =
      fromBrowse ? self->browsePrefix() : std::string("/music/");
  const std::vector<std::string>& src =
      fromBrowse ? self->browseTracks_ : self->looseTracks_;
  if (self->pendingSelect_ < 0 ||
      self->pendingSelect_ >= static_cast<int>(src.size())) {
    return;  // stale row — bail BEFORE touching the playing playlist
  }
  if (self->playingPrefix_ != prefix) {
    // Cross-album (or first-ever) selection: the tapped view's listing
    // becomes the playlist. Skip-bad state resets with it (spec: "bad" is
    // per-session bookkeeping, not persisted anywhere).
    self->playlist_.setFiles(src);
    self->playingPrefix_ = prefix;
  }
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
