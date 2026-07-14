// src/apps/music/BtConnectScreen.h — the Bluetooth connect screen for the Music
// app (A4). Relocated from the former Settings → Bluetooth section: scan for
// speakers, pair/connect, reconnect to the paired one, or forget it.
//
// Music's requiredRadio() is Bluetooth, so the launcher powers the BT stack up
// for the whole session — this screen needs only BluetoothAudioService, never
// RadioManager. On a successful connect it calls onConnected(); MusicApp uses
// that to swap the connect screen for the player.
#pragma once

#include <lvgl.h>

#include <functional>

class BluetoothAudioService;

void buildBtConnectScreen(lv_obj_t* parent, BluetoothAudioService& bt,
                          std::function<void()> onConnected);
