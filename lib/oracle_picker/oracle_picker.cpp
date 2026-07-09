#include "oracle_picker.h"

#include <date_utils.h>

#include <utility>
#include <vector>

namespace {

// splitmix64 — tiny deterministic mixer; the state advances per call. Not
// cryptographic, just unpredictable-to-a-human quote ordering.
uint64_t splitmix64(uint64_t& s) {
  s += 0x9E3779B97F4A7C15ull;
  uint64_t z = s;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

}  // namespace

size_t oraclePickAt(int32_t daySerial, size_t entryCount) {
  if (entryCount < 2) return 0;  // 0 → sentinel 0; 1 → the only entry
  const int32_t n = static_cast<int32_t>(entryCount);
  int32_t cycle = daySerial / n;
  int32_t pos = daySerial % n;
  if (pos < 0) {  // floor semantics so pre-epoch serials still walk cleanly
    pos += n;
    cycle -= 1;
  }

  // Fresh Fisher-Yates permutation per (cycle, entryCount). O(n) time and
  // memory — runs once per app open on a list of at most a few hundred lines.
  std::vector<size_t> perm(entryCount);
  for (size_t i = 0; i < entryCount; ++i) perm[i] = i;
  uint64_t state = (static_cast<uint64_t>(static_cast<int64_t>(cycle))) ^
                   (static_cast<uint64_t>(entryCount) << 32);
  for (size_t i = entryCount - 1; i > 0; --i) {
    const size_t j = static_cast<size_t>(splitmix64(state) % (i + 1));
    std::swap(perm[i], perm[j]);
  }
  return perm[static_cast<size_t>(pos)];
}

size_t oraclePick(uint32_t key, size_t entryCount) {
  if (entryCount < 2) return 0;
  const int32_t serial = daysBetween(LocalDate{1970, 1, 1}, fromDateKey(key));
  return oraclePickAt(serial, entryCount);
}
