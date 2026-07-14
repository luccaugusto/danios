#include "fs_names.h"

#include <algorithm>
#include <cctype>

bool hasExtension(const std::string& name, const std::string& ext) {
  if (ext.empty()) return true;
  if (name.size() < ext.size()) return false;
  const size_t off = name.size() - ext.size();
  for (size_t i = 0; i < ext.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(name[off + i])) !=
        std::tolower(static_cast<unsigned char>(ext[i]))) {
      return false;
    }
  }
  return true;
}

bool isHiddenName(const std::string& name) {
  return !name.empty() && name[0] == '.';
}

std::vector<std::string> filterAndSortNames(const std::vector<FsEntry>& entries,
                                            const std::string& ext) {
  std::vector<std::string> out;
  for (const auto& e : entries) {
    if (e.isDir) continue;
    if (isHiddenName(e.name)) continue;
    if (!hasExtension(e.name, ext)) continue;
    out.push_back(e.name);
  }
  std::sort(out.begin(), out.end());
  return out;
}

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
