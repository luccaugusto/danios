// lib/playlist/playlist.h — pure playlist state (roadmap §3, A4). std C++17
// only, zero Arduino/LVGL includes. Entries are the sorted basenames from
// StorageService::listFiles("/music", ".mp3"); the UI prepends "/music/".
//
// "Bad" tracks are files the decoder rejected this session (spec §6.5:
// unreadable MP3 -> skip to next track). They stay in the list, greyed out,
// but navigation and selection skip them.
#pragma once

#include <string>
#include <vector>

// Display title rule shared by Playlist::titleAt and the Music UI's
// non-playlist listings (A4.1): basename minus a trailing ".mp3", any case.
std::string trackTitle(const std::string& file);

class Playlist {
 public:
  void setFiles(std::vector<std::string> files);  // resets current to 0 (or -1)
  int count() const;
  int playableCount() const;
  int currentIndex() const;             // -1 when empty or nothing playable
  std::string fileAt(int i) const;      // "" when out of range
  std::string titleAt(int i) const;     // basename minus trailing ".mp3"/".MP3"
  bool isBad(int i) const;
  bool select(int i);                   // false: out of range or bad
  bool next();                          // wraps, skips bad; false if none playable
  bool previous();
  bool markCurrentBad();                // mark + advance; false when none remain

 private:
  bool step(int dir);                   // move current to next playable in dir

  std::vector<std::string> files_;
  std::vector<bool> bad_;
  int current_ = -1;
};
