// lib/fs_names/fs_names.h — std C++ only. Pure filename filtering/sorting for
// SD directory listings (used by StorageService::listFiles and A4 playlist).
#pragma once
#include <string>
#include <vector>

struct FsEntry {
  std::string name;  // basename only, no path
  bool isDir;
};

// Case-insensitive suffix match, e.g. hasExtension("A.MP3", ".mp3") == true.
// Empty ext matches everything.
bool hasExtension(const std::string& name, const std::string& ext);

// True for dotfiles (".DS_Store" and friends) that should never be listed.
bool isHiddenName(const std::string& name);

// Keep non-directory, non-hidden entries matching ext; return names sorted
// ascending byte-wise (deterministic, locale-free).
std::vector<std::string> filterAndSortNames(const std::vector<FsEntry>& entries,
                                            const std::string& ext);
