#include "bt_addr.h"

#include <cstdio>

namespace {
int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
}  // namespace

bool parseBtAddr(const std::string& s, uint8_t out[6]) {
  if (s.size() != 17) return false;
  for (int i = 0; i < 6; ++i) {
    const int base = i * 3;
    if (i > 0 && s[base - 1] != ':') return false;
    const int hi = hexVal(s[base]);
    const int lo = hexVal(s[base + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}

std::string formatBtAddr(const uint8_t addr[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1],
           addr[2], addr[3], addr[4], addr[5]);
  return buf;
}
