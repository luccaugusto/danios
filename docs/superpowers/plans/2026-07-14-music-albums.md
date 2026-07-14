# Music Album Folders (A4.1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Date:** 2026-07-14
**Spec:** [`docs/superpowers/specs/2026-07-14-music-albums-design.md`](../specs/2026-07-14-music-albums-design.md)
**Branch:** `a4-music` (rides the unmerged A4 branch — one hardware gate covers both).

**Goal:** Folders directly under `/music` are albums (one level deep): the Music app gains an albums view, per-album playlists, and back navigation, so tracks can be organized by folder.

**Architecture:** One new pure function in `lib/fs_names` (`filterAndSortDirNames`, native-tested) feeds a thin `StorageService::listDirs()`. `Playlist`'s title rule is extracted into a `trackTitle()` free function (native-tested) so non-playlist listings can share it. `MusicApp` gains a two-state view (`Albums` ↔ `Tracks`) over the existing single player UI: only the list content swaps; the playing album's prefix (`playingPrefix_`) is tracked separately from the browsed album, so browsing never disturbs playback. `Playlist` semantics and `Mp3Player` are untouched.

**Tech Stack:** C++17 (both envs), LVGL 8.4 (v8 API), PlatformIO (`cyd` device env, `native` host-test env), Unity.

## Global Constraints

(From roadmap §2 + the A4 plan — every task inherits these.)

- **Board:** ESP32 CYD, **no PSRAM**. LVGL pool is **24 KB** (`include/lv_conf.h`), hence `kMaxListRows = 28` — the cap applies **per view** and stays 28.
- **LVGL:** `lvgl@8.4.0` v8 API; UI code runs on the Arduino loop task only. Never delete the widget tree an event handler is running inside — defer via `lv_async_call` (existing MusicApp pattern).
- **Radio rule:** unchanged — `requiredRadio()` = `Bluetooth`; the app never touches `RadioManager`/`esp_bt_*`.
- **TDD, native-first:** pure logic in `lib/<module>/` with **zero Arduino/LVGL includes**, tested via `pio test -e native`. Include form is flat: `<fs_names.h>`, `<playlist.h>`.
- **A4 owns no NVS keys** — no persistence of last album/track/volume.
- **UI language: Portuguese.** New strings in this plan: `"Nenhuma música neste álbum."`, `"+%d não listados"` (albums-view overflow).
- **`Mp3Player` untouched. `Playlist` class behavior untouched** — the only `lib/playlist` change is extracting `titleAt`'s body into a `trackTitle()` free function (`titleAt` delegates to it; all 10 existing playlist tests must keep passing unmodified).
- **`StorageService::listFiles` untouched** (roadmap-pinned signature); `listDirs` is a new sibling.
- **Commits:** small, conventional (`feat:`, `refactor:`, `docs:`).

### Plan-specific facts (verified against the tree at commit 40bbbec)

- `lib/fs_names/fs_names.{h,cpp}` exist with `FsEntry{name,isDir}`, `hasExtension`, `isHiddenName`, `filterAndSortNames` (drops dirs — the reason folders are invisible today).
- `test/test_fs_names/test_main.cpp` has 8 `RUN_TEST` lines; `test/test_playlist/test_main.cpp` has 9; the full native suite is **177** cases and must end at **180**.
- `src/apps/music/MusicApp.{h,cpp}` are as committed in 9af1794 (heap probe after pipeline alloc, `kMaxListRows = 28`, LVGL-upgrade-hazard comment in `handleBadTrack`). Task 3 **replaces both files wholesale** — the full new contents are in this plan; do not hand-merge hunks.
- `App::handleBack()` exists (`src/core/App.h`): return `true` to consume back, `false` to let the Launcher go home. The launcher's back arrow is a launcher-owned widget, so rebuilding the app's list from `handleBack()` is not an LVGL self-deletion hazard.
- `BtConnectScreen`, the connect flow, `onExit` teardown ordering, and `tick()` auto-advance are correct as-is and must not regress; Task 3's replacement files keep them byte-comparable except where this plan says otherwise.

## File Structure

| File | Task | Responsibility |
| --- | --- | --- |
| Modify `lib/fs_names/fs_names.h` + `.cpp` | 1 | add `filterAndSortDirNames` (dirs-only mirror) |
| Modify `test/test_fs_names/test_main.cpp` | 1 | +2 tests for the new filter |
| Modify `src/services/StorageService.h` + `.cpp` | 1 | add `listDirs(dir)` |
| Modify `lib/playlist/playlist.h` + `.cpp` | 2 | extract `trackTitle()` free function |
| Modify `test/test_playlist/test_main.cpp` | 2 | +1 test pinning `trackTitle` |
| Replace `src/apps/music/MusicApp.h` + `.cpp` | 3 | albums/tracks views, `playingPrefix_`, `handleBack` |
| Modify `docs/superpowers/specs/apps/music.md` | 4 | A4.1 revision note |

## Task Right-Sizing Overview

1. `lib/fs_names` dir filter (native TDD) + `StorageService::listDirs`
2. `lib/playlist` `trackTitle()` extraction (native TDD)
3. `MusicApp` albums/tracks views + back navigation (device build)
4. Docs revision note + on-device verification checklist (user hardware gate)

---

### Task 1: `filterAndSortDirNames` + `StorageService::listDirs`

**Files:**
- Modify: `lib/fs_names/fs_names.h`
- Modify: `lib/fs_names/fs_names.cpp`
- Test: `test/test_fs_names/test_main.cpp`
- Modify: `src/services/StorageService.h`
- Modify: `src/services/StorageService.cpp`

**Interfaces:**
- Consumes: existing `FsEntry`, `isHiddenName`.
- Produces: `std::vector<std::string> filterAndSortDirNames(const std::vector<FsEntry>& entries);` (pure) and `std::vector<std::string> StorageService::listDirs(const char* dir);` (device). Task 3's `MusicApp::showAlbumsView` calls `storage_.listDirs("/music")`.

- [ ] **Step 1: Write the failing tests**

In `test/test_fs_names/test_main.cpp`, add after the last existing test function (before `int main`):

```cpp
void test_dir_filter_keeps_only_visible_dirs() {
  std::vector<FsEntry> in = {
      {"song.mp3", false},  // file -> skipped
      {"Zebra", true},
      {".Trash", true},     // hidden dir -> skipped
      {"Abba", true},
  };
  auto out = filterAndSortDirNames(in);
  TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)out.size());
  TEST_ASSERT_EQUAL_STRING("Abba", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("Zebra", out[1].c_str());
}

void test_dir_filter_empty_input() {
  auto out = filterAndSortDirNames({});
  TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)out.size());
}
```

and register both in `main` after the last existing `RUN_TEST`:

```cpp
  RUN_TEST(test_dir_filter_keeps_only_visible_dirs);
  RUN_TEST(test_dir_filter_empty_input);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/lucca/repos/danios && pio test -e native -f test_fs_names`
Expected: **build error** — `filterAndSortDirNames` not declared.

- [ ] **Step 3: Implement the filter**

In `lib/fs_names/fs_names.h`, add after the `filterAndSortNames` declaration:

```cpp
// Keep directory entries (drop files and hidden names); return names sorted
// ascending byte-wise. The dirs-only mirror of filterAndSortNames — album
// folders under /music (A4.1).
std::vector<std::string> filterAndSortDirNames(
    const std::vector<FsEntry>& entries);
```

In `lib/fs_names/fs_names.cpp`, add at the end:

```cpp
std::vector<std::string> filterAndSortDirNames(
    const std::vector<FsEntry>& entries) {
  std::vector<std::string> out;
  for (const auto& e : entries) {
    if (!e.isDir) continue;
    if (isHiddenName(e.name)) continue;
    out.push_back(e.name);
  }
  std::sort(out.begin(), out.end());
  return out;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /home/lucca/repos/danios && pio test -e native -f test_fs_names`
Expected: `10 Tests 0 Failures 0 Ignored` — PASSED

- [ ] **Step 5: Add `StorageService::listDirs`**

In `src/services/StorageService.h`, add after the `listFiles` declaration:

```cpp
  std::vector<std::string> listDirs(const char* dir);                    // sorted folder names, non-recursive (A4.1 albums)
```

In `src/services/StorageService.cpp`, add after the `listFiles` definition (same `FsEntry` walk, dirs filter):

```cpp
std::vector<std::string> StorageService::listDirs(const char* dir) {
  std::vector<FsEntry> entries;
  if (mounted_) {
    File d = SD.open(dir);
    if (d && d.isDirectory()) {
      for (File f = d.openNextFile(); f; f = d.openNextFile()) {
        entries.push_back(FsEntry{std::string(f.name()), f.isDirectory()});
        f.close();
      }
    }
    if (d) d.close();
  }
  return filterAndSortDirNames(entries);
}
```

- [ ] **Step 6: Verify device build**

Run: `cd /home/lucca/repos/danios && pio run -e cyd`
Expected: `SUCCESS`

- [ ] **Step 7: Commit**

```bash
cd /home/lucca/repos/danios && \
  git add lib/fs_names test/test_fs_names src/services/StorageService.h \
    src/services/StorageService.cpp && \
  git commit -m "feat: fs_names dir listing + StorageService::listDirs (A4.1 albums)"
```

---

### Task 2: extract `trackTitle()` from `Playlist::titleAt`

**Files:**
- Modify: `lib/playlist/playlist.h`
- Modify: `lib/playlist/playlist.cpp`
- Test: `test/test_playlist/test_main.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `std::string trackTitle(const std::string& file);` — basename minus a trailing `".mp3"` (any case). `Playlist::titleAt` delegates to it (behavior identical). Task 3's list rendering calls `trackTitle` for listings that are not the loaded playlist.

- [ ] **Step 1: Write the failing test**

In `test/test_playlist/test_main.cpp`, add after `test_title_strips_extension_case_insensitive`:

```cpp
static void test_track_title_free_function() {
  TEST_ASSERT_EQUAL_STRING("Minha Musica", trackTitle("Minha Musica.MP3").c_str());
  TEST_ASSERT_EQUAL_STRING("semextensao", trackTitle("semextensao").c_str());
  TEST_ASSERT_EQUAL_STRING("", trackTitle(".mp3").c_str());  // degenerate name
}
```

and register it in `main` after `RUN_TEST(test_title_strips_extension_case_insensitive);`:

```cpp
  RUN_TEST(test_track_title_free_function);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/lucca/repos/danios && pio test -e native -f test_playlist`
Expected: **build error** — `trackTitle` not declared.

- [ ] **Step 3: Extract the function**

In `lib/playlist/playlist.h`, add before `class Playlist`:

```cpp
// Display title rule shared by Playlist::titleAt and the Music UI's
// non-playlist listings (A4.1): basename minus a trailing ".mp3", any case.
std::string trackTitle(const std::string& file);
```

In `lib/playlist/playlist.cpp`, replace the `Playlist::titleAt` definition:

```cpp
std::string Playlist::titleAt(int i) const {
  std::string f = fileAt(i);
  if (f.size() >= 4) {
    std::string tail = f.substr(f.size() - 4);
    for (char& c : tail) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (tail == ".mp3") return f.substr(0, f.size() - 4);
  }
  return f;
}
```

with:

```cpp
std::string trackTitle(const std::string& file) {
  if (file.size() >= 4) {
    std::string tail = file.substr(file.size() - 4);
    for (char& c : tail) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (tail == ".mp3") return file.substr(0, file.size() - 4);
  }
  return file;
}

std::string Playlist::titleAt(int i) const { return trackTitle(fileAt(i)); }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /home/lucca/repos/danios && pio test -e native -f test_playlist`
Expected: `10 Tests 0 Failures 0 Ignored` — PASSED (the 9 originals unmodified + the new one)

Also run the full suite: `cd /home/lucca/repos/danios && pio test -e native`
Expected: **180 test cases: 180 succeeded**

- [ ] **Step 5: Commit**

```bash
cd /home/lucca/repos/danios && git add lib/playlist test/test_playlist && \
  git commit -m "refactor: extract trackTitle free function from Playlist::titleAt"
```

---

### Task 3: `MusicApp` albums/tracks views + back navigation

**Files:**
- Replace: `src/apps/music/MusicApp.h` (full contents below)
- Replace: `src/apps/music/MusicApp.cpp` (full contents below)

**Interfaces:**
- Consumes: `StorageService::listDirs` (Task 1), `trackTitle` (Task 2), everything MusicApp already consumed (`Playlist`, `Mp3Player`, `buildBtConnectScreen`, `App::handleBack` hook).
- Produces: no new external surface — `main.cpp` is untouched. Internally: `View{Albums,Tracks}`, `playingPrefix_` ("" = nothing loaded; else `"/music/"` or `"/music/<album>/"`), `showAlbumsView()`/`showTracksView(int)`, `handleBack()` override.

Behavior contract (spec): one player UI, only the list swaps; nothing preloads (transport no-ops until first tap — `playPauseClicked` guards `currentIndex() < 0`, `changeTrack` is already safe on an empty playlist); browsing never touches `playlist_` (tracks view renders from `browseTracks_`; the playlist only changes inside `asyncSelectTrack` when a track in a different set is tapped, with ring flush); wrap stays within the playing album; back = tracks → albums → home; loose root tracks play directly from the albums view; empty album shows `"Nenhuma música neste álbum."` as a list text row; per-view 28-row cap with `"+%d não listados"` (albums view) / `"+%d não listadas"` (tracks view).

- [ ] **Step 1: Replace the header**

`src/apps/music/MusicApp.h` — full new contents:

```cpp
// src/apps/music/MusicApp.h — Music app (A4, spec §4.2; A4.1 album folders,
// docs/superpowers/specs/2026-07-14-music-albums-design.md). Thin LVGL
// wrapper: connect screen, then one player UI (now-playing + transport +
// volume) whose list swaps between the albums view (folders under /music +
// loose root tracks) and an album's tracks view. Ordering/skip logic lives in
// lib/playlist, the decode pipeline in Mp3Player, the speaker connect UI in
// BtConnectScreen. requiredRadio() = Bluetooth — the launcher brings BT up
// before onEnter and tears it down on goHome.
//
// Flow on open: BT powered but not linked (the A2DP link is app-scoped) ->
// connect screen; on connect, the albums view. Nothing preloads: the playlist
// stays empty (transport no-ops) until the first track tap. Albums are one
// folder level deep; next/previous and auto-advance wrap within the playing
// album. Browsing another album never disturbs playback — the playing
// album's prefix (playingPrefix_) is tracked apart from the browsed one.
#pragma once

#include <playlist.h>

#include <memory>
#include <string>
#include <vector>

#include "apps/app_catalog.h"
#include "apps/music/Mp3Player.h"
#include "core/App.h"

class StorageService;
class BluetoothAudioService;

class MusicApp : public App {
 public:
  MusicApp(StorageService& storage, BluetoothAudioService& bt)
      : storage_(storage), bt_(bt) {}

  const char* id() const override { return "music"; }
  const char* title() const override { return catalog::kMusic.title; }
  const char* iconPath() const override { return catalog::kMusic.icon; }
  RadioMode requiredRadio() const override { return RadioMode::Bluetooth; }
  void onEnter() override {}
  void buildUI(lv_obj_t* parent) override;
  void onExit() override;
  void tick(uint32_t now_ms) override;
  bool handleBack() override;  // tracks view -> albums view; else launcher exits

 private:
  enum class View : uint8_t { Albums, Tracks };

  void buildPlayerUI();
  void showMessage(const char* text);  // full-screen message (empty/all-bad)
  void showAlbumsView();               // (re)list /music, render albums + loose
  void showTracksView(int albumIdx);   // list one album, render its tracks
  std::string browsePrefix() const;    // "/music/<browseAlbum_>/"
  bool openTrack(bool flushRing);
  void changeTrack(int dir);
  void handleBadTrack();
  void refreshNowPlaying();
  void refreshList();                  // re-render list_ for the current view
  void refreshAlbumsList();
  void refreshTracksList();
  static void playPauseClicked(lv_event_t* e);
  static void prevClicked(lv_event_t* e);
  static void nextClicked(lv_event_t* e);
  static void volumeChanged(lv_event_t* e);
  static void albumClicked(lv_event_t* e);
  static void trackClicked(lv_event_t* e);   // tracks view AND loose root rows
  static void asyncRebuild(void* userData);  // connect screen -> player swap
  static void asyncOpenAlbum(void* userData);
  static void asyncSelectTrack(void* userData);

  StorageService& storage_;
  BluetoothAudioService& bt_;
  Playlist playlist_;                  // the PLAYING set's tracks (basenames)
  std::unique_ptr<Mp3Player> player_;  // ~63 KB pipeline, alive only in-app
  View view_ = View::Albums;
  std::vector<std::string> albums_;        // folder names under /music (sorted)
  std::vector<std::string> looseTracks_;   // root *.mp3 basenames (sorted)
  std::vector<std::string> browseTracks_;  // tracks of the album on screen
  std::string browseAlbum_;    // "" until an album is opened
  std::string playingPrefix_;  // "" = nothing loaded; "/music/" = loose set;
                               // "/music/<album>/" = that album
  int pendingSelect_ = -1;     // deferred track tap (index into target set)
  bool pendingSelectIsBrowse_ = false;  // true: target = browseTracks_
  int pendingAlbum_ = -1;      // deferred album tap
  lv_obj_t* root_ = nullptr;
  lv_obj_t* nowPlaying_ = nullptr;
  lv_obj_t* playPauseLabel_ = nullptr;
  lv_obj_t* list_ = nullptr;
};
```

- [ ] **Step 2: Replace the implementation**

`src/apps/music/MusicApp.cpp` — full new contents:

```cpp
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
// include/lv_conf.h). The cap applies PER VIEW (albums view, each album's
// tracks view). Tracks beyond the cap still play (next/previous cycle through
// the whole playlist); they just aren't tappable rows. The cap is visible in
// the UI ("+N"), never silent.
constexpr int kMaxListRows = 28;
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

  // Connected: allocate the pipeline (RAM strategy) and start the player.
  if (!player_) player_.reset(new Mp3Player());
  Serial.printf("[music] heap after pipeline: %u free, %u largest\n",
                (unsigned)esp_get_free_heap_size(),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
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
  browseTracks_ = storage_.listFiles(dir.c_str(), ".mp3");
  refreshList();
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
      lv_obj_t* btn =
          lv_list_add_btn(list_, LV_SYMBOL_DIRECTORY, albums_[i].c_str());
      lv_obj_add_event_cb(btn, albumClicked, LV_EVENT_CLICKED,
                          reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    } else {
      const int t = i - nAlbums;  // loose root track: plays directly
      const bool current =
          (playingPrefix_ == "/music/" && t == playlist_.currentIndex());
      lv_obj_t* btn =
          lv_list_add_btn(list_, current ? LV_SYMBOL_PLAY : LV_SYMBOL_AUDIO,
                          trackTitle(looseTracks_[t]).c_str());
      if (playingPrefix_ == "/music/" && playlist_.isBad(t)) {
        lv_obj_set_style_opa(btn, LV_OPA_40, 0);
      }
      lv_obj_add_event_cb(btn, trackClicked, LV_EVENT_CLICKED,
                          reinterpret_cast<void*>(static_cast<intptr_t>(t)));
      if (current) lv_obj_scroll_to_view(btn, LV_ANIM_OFF);
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
    lv_obj_t* btn =
        lv_list_add_btn(list_, current ? LV_SYMBOL_PLAY : LV_SYMBOL_AUDIO,
                        trackTitle(browseTracks_[i]).c_str());
    if (playingThis && playlist_.isBad(i)) {
      lv_obj_set_style_opa(btn, LV_OPA_40, 0);
    }
    lv_obj_add_event_cb(btn, trackClicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    if (current) lv_obj_scroll_to_view(btn, LV_ANIM_OFF);
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
  // browseTracks_. Captured at click time — the view can't change before the
  // async call runs (both run on the LVGL task).
  g_self->pendingSelectIsBrowse_ = (g_self->view_ == View::Tracks);
  lv_async_call(asyncSelectTrack, g_self);  // refreshList deletes this button
}

void MusicApp::asyncSelectTrack(void* userData) {
  auto* self = static_cast<MusicApp*>(userData);
  if (self->root_ == nullptr || self->player_ == nullptr) return;  // app closed
  const bool fromBrowse = self->pendingSelectIsBrowse_;
  const std::string prefix =
      fromBrowse ? self->browsePrefix() : std::string("/music/");
  const std::vector<std::string>& src =
      fromBrowse ? self->browseTracks_ : self->looseTracks_;
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
```

- [ ] **Step 3: Verify device build + full native suite**

Run: `cd /home/lucca/repos/danios && pio run -e cyd`
Expected: `SUCCESS` (RAM/Flash within a fraction of a percent of the A4 figures — note them).

Run: `cd /home/lucca/repos/danios && pio test -e native`
Expected: **180 test cases: 180 succeeded**

- [ ] **Step 4: Commit**

```bash
cd /home/lucca/repos/danios && git add src/apps/music/MusicApp.h \
  src/apps/music/MusicApp.cpp && \
  git commit -m "feat: Music album folders — albums view, per-album playlists, back nav"
```

---

### Task 4: docs revision note + on-device verification (user hardware gate)

**Files:**
- Modify: `docs/superpowers/specs/apps/music.md`
- Verification: hardware (CYD + BT speaker + SD card), extends the pending A4 Task 6 pass.

- [ ] **Step 1: Add the spec revision note**

In `docs/superpowers/specs/apps/music.md`, directly after the existing `**Revised:** 2026-07-08 ...` line, add:

```markdown
**Revised 2026-07-14 (A4.1):** folders directly under `/music` are **albums** (one level deep) — the app opens on an albums view, playlists are per-album, and back walks tracks → albums → home. Supersedes this spec's "flat, non-recursive" scan of `/music`. Design: [`2026-07-14-music-albums-design.md`](../2026-07-14-music-albums-design.md).
```

- [ ] **Step 2: Commit**

```bash
cd /home/lucca/repos/danios && git add docs/superpowers/specs/apps/music.md && \
  git commit -m "docs: music spec revision — /music album folders (A4.1)"
```

- [ ] **Step 3: On-device verification (USER — extends the pending A4 Task 6 checklist)**

Prepare the card: `/music/Album A/` and `/music/Album B/` with 2-3 known-good 44.1 kHz CBR files each, two loose `.mp3`s at `/music` root, one empty folder `/music/Vazio/`, and one folder nested inside an album (`/music/Album A/inner/` — must be invisible).

- Albums view lists: Album A, Album B, Vazio (folder icon), then the two loose tracks — sorted, folders first.
- Tap Album A → its tracks; tap a track → plays; ▶ prefix on the row; auto-advance stays inside Album A and wraps to its first track after its last.
- Back arrow: tracks → albums (music keeps playing), albums → home.
- While Album A plays, browse Album B (no playback disturbance), tap a B track → switch is prompt (ring flush audible as a clean cut, no old-track tail).
- Loose root track tap plays directly from the albums view; ▶ shows on its row.
- Tap Vazio → "Nenhuma música neste álbum." with back working. `inner/` never appears.
- Transport before any tap: play/pause, next, prev all do nothing; now-playing shows "-".
- Re-check the A4 heap figure: `[music] heap after pipeline` unchanged from the A4 run.

- [ ] **Step 4: Record the outcome**

Append the album-navigation outcome to the same `docs/hardware.md` note the A4 Task 6 gate records (one combined entry for the a4-music branch).

---

## Definition of done

- [ ] `pio test -e native` — 180/180, including the 2 new `test_fs_names` cases and 1 new `test_playlist` case; the 9 original playlist tests unmodified
- [ ] `pio run -e cyd` — SUCCESS
- [ ] Albums view: folders (one level) + loose root tracks; per-album playlists; wrap within album; back = tracks → albums → home
- [ ] Browsing never disturbs playback; cross-album tap flushes the ring
- [ ] Empty album shows "Nenhuma música neste álbum."; nested folders ignored; entirely empty `/music` keeps the A4 message
- [ ] `Playlist` class behavior and `Mp3Player` untouched (`titleAt` delegates to `trackTitle`, all else identical); `listFiles` signature untouched
- [ ] No new NVS keys; strings PT; `kMaxListRows` still 28, applied per view
- [ ] Hardware checklist (Task 4 Step 3) passed as part of the combined a4-music gate
