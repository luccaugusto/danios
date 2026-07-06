#include "StorageService.h"

#include <SD.h>
#include <fs_names.h>

bool StorageService::begin() {
  // VSPI default pins (SCK 18, MISO 19, MOSI 23) + CS 5. Three separate buses
  // on this board; the display's HSPI is untouched.
  mounted_ = SD.begin(5);
  return mounted_;
}

bool StorageService::mounted() const {
  return mounted_;
}

bool StorageService::exists(const char* path) {
  return mounted_ && SD.exists(path);
}

std::vector<std::string> StorageService::listFiles(const char* dir, const char* ext) {
  std::vector<FsEntry> entries;
  if (mounted_) {
    File d = SD.open(dir);
    if (d && d.isDirectory()) {
      for (File f = d.openNextFile(); f; f = d.openNextFile()) {
        // arduino-esp32 3.x File::name() returns the basename (no path).
        entries.push_back(FsEntry{std::string(f.name()), f.isDirectory()});
        f.close();
      }
    }
    if (d) d.close();
  }
  return filterAndSortNames(entries, ext ? std::string(ext) : std::string());
}

bool StorageService::readLines(const char* path, std::vector<std::string>& out) {
  out.clear();
  if (!mounted_) return false;
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return false;
  }
  std::string line;
  auto flush = [&out, &line]() {
    while (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) out.push_back(line);
    line.clear();
  };
  while (f.available()) {
    const char c = static_cast<char>(f.read());
    if (c == '\n') {
      flush();
    } else {
      line += c;
    }
  }
  flush();  // last line may lack a trailing newline
  f.close();
  return true;
}
