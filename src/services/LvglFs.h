// LVGL v8 filesystem driver: drive letter 'S' -> SD card (read-only).
// Register once after a successful SD mount; then any LVGL widget can load
// "S:/art/..." paths (roadmap 4.1).
#pragma once

void lvglFsRegisterSd();

// True if `path` (e.g. "S:/art/icons/calc.bin") opens for read. Returns false
// when the file is missing OR the S drive was never registered (no SD card).
bool lvglFsExists(const char* path);
