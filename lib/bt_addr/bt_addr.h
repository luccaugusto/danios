// danios bt_addr — Bluetooth address string <-> bytes. Wire format for the
// NVS "bt.addr" key is uppercase "AA:BB:CC:DD:EE:FF". Pure std C++17.
#pragma once
#include <cstdint>
#include <string>

bool parseBtAddr(const std::string& s, uint8_t out[6]);
std::string formatBtAddr(const uint8_t addr[6]);
