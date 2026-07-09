#include "oracle_picker.h"

size_t oraclePick(uint32_t key, size_t entryCount) {
  if (entryCount < 2) return 0;  // 0 → sentinel 0; 1 → the only entry
  return static_cast<size_t>(key) % entryCount;  // placeholder walk — Task 2
                                                 // replaces with the shuffle
}
