#include "launcher_model.h"

LauncherModel::LauncherModel(int columns) : columns_(columns < 1 ? 1 : columns) {}

int LauncherModel::registerApp(const std::string& id, bool inGrid) {
  if (id.empty() || indexOf(id) >= 0) return -1;
  entries_.push_back(Entry{id, inGrid, /*badge=*/false, /*enabled=*/true});
  const int idx = static_cast<int>(entries_.size()) - 1;
  if (inGrid) gridToEntry_.push_back(idx);
  return idx;
}

int LauncherModel::count() const { return static_cast<int>(entries_.size()); }

int LauncherModel::gridCount() const { return static_cast<int>(gridToEntry_.size()); }

int LauncherModel::indexOf(const std::string& id) const {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].id == id) return static_cast<int>(i);
  }
  return -1;
}

int LauncherModel::gridIndexOf(const std::string& id) const {
  for (size_t g = 0; g < gridToEntry_.size(); ++g) {
    if (entries_[static_cast<size_t>(gridToEntry_[g])].id == id) {
      return static_cast<int>(g);
    }
  }
  return -1;
}

const std::string& LauncherModel::idAtGrid(int gridIndex) const {
  return entries_[static_cast<size_t>(gridToEntry_[static_cast<size_t>(gridIndex)])].id;
}

GridSlot LauncherModel::slotOf(int gridIndex) const {
  return GridSlot{gridIndex / columns_, gridIndex % columns_};
}
