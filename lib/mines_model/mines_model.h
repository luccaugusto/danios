// lib/mines_model/mines_model.h — minesweeper board (spec
// 2026-07-15-minesweeper-design). Pure C++17, no Arduino/LVGL; randomness is
// an injected rng (seeded LCG in native tests, esp_random() on device).
// Mines are placed on the FIRST reveal, excluding the tapped cell and its 8
// neighbors, so the first tap always opens an area. reveal() on an
// already-revealed number chords (Task 2). All coordinates are bounds-checked
// no-ops when invalid.
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

enum class CellView : uint8_t { Hidden, Flagged, Revealed };
enum class GameState : uint8_t { Fresh, Playing, Won, Lost };

class MinesBoard {
 public:
  MinesBoard(uint8_t rows, uint8_t cols, uint16_t mines,
             std::function<uint32_t()> rng);

  GameState state() const { return state_; }
  void reveal(uint8_t r, uint8_t c);
  void toggleFlag(uint8_t r, uint8_t c);

  CellView view(uint8_t r, uint8_t c) const;
  uint8_t adjacent(uint8_t r, uint8_t c) const;  // meaningful when Revealed
  bool isMine(uint8_t r, uint8_t c) const;       // UI uses it post-loss
  uint16_t flagsPlaced() const { return flags_; }
  uint16_t mineCount() const { return mines_; }
  uint8_t rows() const { return rows_; }
  uint8_t cols() const { return cols_; }

 private:
  struct Cell {
    bool mine = false;
    bool flagged = false;
    bool revealed = false;
    uint8_t adj = 0;
  };

  size_t idx(int r, int c) const { return static_cast<size_t>(r) * cols_ + c; }
  bool inBounds(int r, int c) const {
    return r >= 0 && r < rows_ && c >= 0 && c < cols_;
  }
  void placeMines(uint8_t safeR, uint8_t safeC);
  void floodReveal(uint8_t r, uint8_t c);
  void chord(uint8_t r, uint8_t c);
  void checkWin();

  uint8_t rows_;
  uint8_t cols_;
  uint16_t mines_;
  std::function<uint32_t()> rng_;
  GameState state_ = GameState::Fresh;
  uint16_t flags_ = 0;
  uint16_t revealedCount_ = 0;
  std::vector<Cell> cells_;
};
