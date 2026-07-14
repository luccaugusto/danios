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
  std::unique_ptr<Mp3Player> player_;  // ~38 KB pipeline, alive only in-app
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
