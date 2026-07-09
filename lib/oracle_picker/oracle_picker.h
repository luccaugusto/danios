// lib/oracle_picker/oracle_picker.h — date-seeded daily shuffle (roadmap §3,
// A2). Pure logic: std C++17 + lib/date_utils only, zero Arduino/LVGL.
//
// One index per civil day, stable all day: each block of entryCount
// consecutive days ("a cycle") is a shuffled permutation of the wisdom list,
// so the order is not predictable and no entry repeats until the whole list
// has been shown. Changing entryCount reshuffles — tolerated by the spec
// (the maker's list may grow/shrink between boots).
#pragma once

#include <cstddef>
#include <cstdint>

// Core walk, exposed for native tests: daySerial is the civil-day number
// (1970-01-01 = 0; may be negative — handled with floor semantics). Each
// aligned entryCount-day cycle is a Fisher-Yates permutation seeded from
// (cycle number, entryCount).
size_t oraclePickAt(int32_t daySerial, size_t entryCount);

// The shape pinned by the spec: (date_utils dateKey YYYYMMDD, list size) →
// index into the list. entryCount 0 → 0 (callers guard the empty list
// themselves); entryCount 1 → always 0.
size_t oraclePick(uint32_t key, size_t entryCount);
