// lib/launcher_model/launcher_model.h — std C++17 only, no Arduino/LVGL.
// Pure bookkeeping behind src/core/Launcher: registration order → grid slots.
#pragma once

#include <string>
#include <vector>

struct GridSlot {
  int row;
  int col;
};

class LauncherModel {
 public:
  explicit LauncherModel(int columns);

  // Returns the registration index (0-based), or -1 for a duplicate/empty id.
  // Registration order = grid order (roadmap §4.5). inGrid=false entries
  // (Settings) are openable but get no grid slot.
  int registerApp(const std::string& id, bool inGrid = true);

  int count() const;      // all registered entries
  int gridCount() const;  // only inGrid entries

  int indexOf(const std::string& id) const;      // registration index, -1 unknown
  int gridIndexOf(const std::string& id) const;  // grid position, -1 if not in grid

  // Precondition: 0 <= gridIndex < gridCount().
  const std::string& idAtGrid(int gridIndex) const;
  GridSlot slotOf(int gridIndex) const;

  // Badge (red dot) and enabled (greyed icon) bookkeeping. Setters return
  // false when the id is unknown. Defaults: badge=false, enabled=true.
  bool setBadge(const std::string& id, bool on);
  bool badgeAtGrid(int gridIndex) const;
  bool setEnabled(const std::string& id, bool en);
  bool enabledAtGrid(int gridIndex) const;
  bool enabled(const std::string& id) const;  // unknown id → false
  bool canOpen(const std::string& id) const;  // known && enabled

 private:
  struct Entry {
    std::string id;
    bool inGrid;
    bool badge;
    bool enabled;
  };

  int columns_;
  std::vector<Entry> entries_;     // registration order
  std::vector<int> gridToEntry_;   // grid index → entries_ index
};
