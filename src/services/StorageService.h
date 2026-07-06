// SD card access (VSPI: SCK 18, MISO 19, MOSI 23, CS 5 — separate bus from the
// display's HSPI, no sharing issues). Roadmap 4.9 — signatures are pinned.
#pragma once
#include <string>
#include <vector>

class StorageService {
public:
  bool begin();                                   // SD.begin(5); false if missing
  bool mounted() const;
  bool exists(const char* path);
  std::vector<std::string> listFiles(const char* dir, const char* ext); // sorted, non-recursive
  bool readLines(const char* path, std::vector<std::string>& out);      // trims \r, skips empty

private:
  bool mounted_ = false;
};
