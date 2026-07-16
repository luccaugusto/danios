#include "mines_model.h"

#include <cstdlib>
#include <utility>

MinesBoard::MinesBoard(uint8_t rows, uint8_t cols, uint16_t mines,
                       std::function<uint32_t()> rng)
    : rows_(rows),
      cols_(cols),
      mines_(mines),
      rng_(std::move(rng)),
      cells_(static_cast<size_t>(rows) * cols) {}

void MinesBoard::placeMines(uint8_t safeR, uint8_t safeC) {
  // Candidates: every cell outside the 3x3 zone around the first tap,
  // row-major. Partial Fisher-Yates picks mines_ distinct ones.
  std::vector<uint16_t> cand;
  cand.reserve(cells_.size());
  for (int r = 0; r < rows_; ++r)
    for (int c = 0; c < cols_; ++c) {
      const bool nearTap = std::abs(r - static_cast<int>(safeR)) <= 1 &&
                           std::abs(c - static_cast<int>(safeC)) <= 1;
      if (!nearTap) cand.push_back(static_cast<uint16_t>(idx(r, c)));
    }
  // Clamp mines_ to available candidates: over-requested mines would cause UB
  // in the modulo below and make the game unwinnable (safe cell count < 0).
  if (mines_ > cand.size()) mines_ = static_cast<uint16_t>(cand.size());
  for (uint16_t i = 0; i < mines_; ++i) {
    const uint16_t j =
        i + static_cast<uint16_t>(rng_() % static_cast<uint32_t>(cand.size() - i));
    std::swap(cand[i], cand[j]);
    cells_[cand[i]].mine = true;
  }
  for (int r = 0; r < rows_; ++r)
    for (int c = 0; c < cols_; ++c) {
      if (cells_[idx(r, c)].mine) continue;
      uint8_t n = 0;
      for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc)
          if (inBounds(r + dr, c + dc) && cells_[idx(r + dr, c + dc)].mine) ++n;
      cells_[idx(r, c)].adj = n;
    }
}

void MinesBoard::reveal(uint8_t r, uint8_t c) {
  if (!inBounds(r, c)) return;
  if (state_ == GameState::Won || state_ == GameState::Lost) return;
  Cell& cell = cells_[idx(r, c)];
  if (cell.flagged) return;
  if (state_ == GameState::Fresh) {
    placeMines(r, c);
    state_ = GameState::Playing;
  }
  if (cell.revealed) {
    chord(r, c);
    return;
  }
  if (cell.mine) {
    cell.revealed = true;
    state_ = GameState::Lost;
    return;
  }
  floodReveal(r, c);
  checkWin();
}

// Iterative flood fill (bounded explicit stack, no recursion): reveal the
// tapped safe cell; a 0-adjacency cell also queues its neighbors. Flagged
// cells are never auto-revealed (classic rule).
void MinesBoard::floodReveal(uint8_t r0, uint8_t c0) {
  std::vector<uint16_t> stack{static_cast<uint16_t>(idx(r0, c0))};
  while (!stack.empty()) {
    const uint16_t i = stack.back();
    stack.pop_back();
    Cell& cell = cells_[i];
    if (cell.revealed || cell.flagged || cell.mine) continue;
    cell.revealed = true;
    ++revealedCount_;
    if (cell.adj != 0) continue;
    const int r = i / cols_;
    const int c = i % cols_;
    for (int dr = -1; dr <= 1; ++dr)
      for (int dc = -1; dc <= 1; ++dc)
        if (inBounds(r + dr, c + dc))
          stack.push_back(static_cast<uint16_t>(idx(r + dr, c + dc)));
  }
}

void MinesBoard::chord(uint8_t, uint8_t) {}  // Task 2

void MinesBoard::checkWin() {
  if (state_ != GameState::Playing) return;
  const uint16_t safe = static_cast<uint16_t>(rows_) * cols_ - mines_;
  if (revealedCount_ >= safe) state_ = GameState::Won;
}

void MinesBoard::toggleFlag(uint8_t r, uint8_t c) {
  if (!inBounds(r, c)) return;
  if (state_ != GameState::Fresh && state_ != GameState::Playing) return;
  Cell& cell = cells_[idx(r, c)];
  if (cell.revealed) return;
  cell.flagged = !cell.flagged;
  flags_ = cell.flagged ? flags_ + 1 : flags_ - 1;
}

CellView MinesBoard::view(uint8_t r, uint8_t c) const {
  if (!inBounds(r, c)) return CellView::Hidden;
  const Cell& cell = cells_[idx(r, c)];
  if (cell.revealed) return CellView::Revealed;
  if (cell.flagged) return CellView::Flagged;
  return CellView::Hidden;
}

uint8_t MinesBoard::adjacent(uint8_t r, uint8_t c) const {
  return inBounds(r, c) ? cells_[idx(r, c)].adj : 0;
}

bool MinesBoard::isMine(uint8_t r, uint8_t c) const {
  return inBounds(r, c) && cells_[idx(r, c)].mine;
}
