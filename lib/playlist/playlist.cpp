#include "playlist.h"

#include <cctype>

void Playlist::setFiles(std::vector<std::string> files) {
  files_ = std::move(files);
  bad_.assign(files_.size(), false);
  current_ = files_.empty() ? -1 : 0;
}

int Playlist::count() const { return static_cast<int>(files_.size()); }

int Playlist::playableCount() const {
  int n = 0;
  for (bool b : bad_) {
    if (!b) ++n;
  }
  return n;
}

int Playlist::currentIndex() const { return current_; }

std::string Playlist::fileAt(int i) const {
  if (i < 0 || i >= count()) return "";
  return files_[static_cast<size_t>(i)];
}

std::string Playlist::titleAt(int i) const {
  std::string f = fileAt(i);
  if (f.size() >= 4) {
    std::string tail = f.substr(f.size() - 4);
    for (char& c : tail) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (tail == ".mp3") return f.substr(0, f.size() - 4);
  }
  return f;
}

bool Playlist::isBad(int i) const {
  return i >= 0 && i < count() && bad_[static_cast<size_t>(i)];
}

bool Playlist::select(int i) {
  if (i < 0 || i >= count() || bad_[static_cast<size_t>(i)]) return false;
  current_ = i;
  return true;
}

bool Playlist::step(int dir) {
  if (current_ < 0) return false;
  const int n = count();
  for (int k = 1; k <= n; ++k) {  // k == n lands back on current (single-track wrap)
    const int i = ((current_ + dir * k) % n + n) % n;
    if (!bad_[static_cast<size_t>(i)]) {
      current_ = i;
      return true;
    }
  }
  return false;
}

bool Playlist::next() { return step(1); }

bool Playlist::previous() { return step(-1); }

bool Playlist::markCurrentBad() {
  if (current_ < 0) return false;
  bad_[static_cast<size_t>(current_)] = true;
  if (step(1)) return true;
  current_ = -1;  // nothing playable left
  return false;
}
