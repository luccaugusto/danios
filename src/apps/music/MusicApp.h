// src/apps/music/MusicApp.h — Music app (A4, spec §4.2). Thin LVGL wrapper:
// connect screen + track list + transport + volume; ordering/skip logic lives
// in lib/playlist, the decode pipeline in Mp3Player, the speaker connect UI in
// BtConnectScreen. requiredRadio() = Bluetooth — the launcher brings BT up
// before onEnter and tears it down on goHome.
//
// Flow on open (spec §4.2): BT is powered but not linked (the A2DP link is
// app-scoped, so it is always down on entry) -> show the connect screen
// (scan/pair/connect/forget); on connect, swap to the player and scan /music
// for the playlist.
#pragma once

#include <playlist.h>

#include <memory>

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

 private:
  void buildPlayerUI();
  void showMessage(const char* text);  // full-screen message (empty/all-bad)
  bool openTrack(bool flushRing);
  void changeTrack(int dir);
  void handleBadTrack();
  void refreshNowPlaying();
  void refreshList();
  static void playPauseClicked(lv_event_t* e);
  static void prevClicked(lv_event_t* e);
  static void nextClicked(lv_event_t* e);
  static void volumeChanged(lv_event_t* e);
  static void trackClicked(lv_event_t* e);
  static void asyncRebuild(void* userData);   // connect screen -> player swap
  static void asyncSelectTrack(void* userData);

  StorageService& storage_;
  BluetoothAudioService& bt_;
  Playlist playlist_;
  std::unique_ptr<Mp3Player> player_;  // ~63 KB pipeline, alive only in-app
  int pendingSelect_ = -1;
  lv_obj_t* root_ = nullptr;
  lv_obj_t* nowPlaying_ = nullptr;
  lv_obj_t* playPauseLabel_ = nullptr;
  lv_obj_t* list_ = nullptr;
};
