// Host-side tests for MinesBoard (pio test -e native).
//
// Layout used by most zero-rng tests: 3x3 board, 2 mines, first tap (2,2).
// Exclusion zone around (2,2) = rows 1-2 x cols 1-2, so candidates row-major
// are (0,0),(0,1),(0,2),(1,0),(2,0) and zero-rng places mines at (0,0),(0,1):
//
//   M M 1        adj: (0,2)=1  (1,0)=2 (1,1)=2 (1,2)=1
//   2 2 1             (2,0)=0  (2,1)=0 (2,2)=0
//   0 0 0
//
// First tap (2,2) flood-reveals the bottom-left 6 cells and stops at the
// numbered border; (0,2) stays hidden (its only path is through numbers).
#include <unity.h>

#include "mines_model.h"

void setUp() {}
void tearDown() {}

static uint32_t g_seed = 1;
static uint32_t lcg() {
  g_seed = g_seed * 1664525u + 1013904223u;
  return g_seed;
}
static uint32_t zero() { return 0; }

static MinesBoard rigged3x3() {  // the layout documented above
  MinesBoard b(3, 3, 2, zero);
  b.reveal(2, 2);
  return b;
}

static void test_fresh_board_all_hidden() {
  MinesBoard b(9, 9, 10, lcg);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Fresh), static_cast<int>(b.state()));
  TEST_ASSERT_EQUAL_UINT16(10, b.mineCount());
  TEST_ASSERT_EQUAL_UINT16(0, b.flagsPlaced());
  for (uint8_t r = 0; r < 9; ++r)
    for (uint8_t c = 0; c < 9; ++c)
      TEST_ASSERT_EQUAL(static_cast<int>(CellView::Hidden),
                        static_cast<int>(b.view(r, c)));
}

static void test_first_reveal_places_mines_outside_safe_zone() {
  g_seed = 42;
  MinesBoard b(9, 9, 10, lcg);
  b.reveal(4, 4);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Playing), static_cast<int>(b.state()));
  uint16_t mines = 0;
  for (uint8_t r = 0; r < 9; ++r)
    for (uint8_t c = 0; c < 9; ++c) {
      if (b.isMine(r, c)) ++mines;
      if (r >= 3 && r <= 5 && c >= 3 && c <= 5)
        TEST_ASSERT_FALSE(b.isMine(r, c));  // 3x3 zone around the tap is clean
    }
  TEST_ASSERT_EQUAL_UINT16(10, mines);
}

static void test_first_reveal_corner_safe_zone_clips() {
  g_seed = 7;
  MinesBoard b(9, 9, 10, lcg);
  b.reveal(0, 0);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Playing), static_cast<int>(b.state()));
  TEST_ASSERT_FALSE(b.isMine(0, 0));
  TEST_ASSERT_FALSE(b.isMine(0, 1));
  TEST_ASSERT_FALSE(b.isMine(1, 0));
  TEST_ASSERT_FALSE(b.isMine(1, 1));
}

static void test_zero_rng_layout_is_the_documented_one() {
  MinesBoard b = rigged3x3();
  TEST_ASSERT_TRUE(b.isMine(0, 0));
  TEST_ASSERT_TRUE(b.isMine(0, 1));
  TEST_ASSERT_FALSE(b.isMine(0, 2));
}

static void test_flood_fill_stops_at_numbers() {
  MinesBoard b = rigged3x3();
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Playing), static_cast<int>(b.state()));
  // Revealed: the tapped zero region plus its numbered border.
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(2, 0)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(2, 1)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(2, 2)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(1, 0)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(1, 1)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(1, 2)));
  // Beyond the numbered border: untouched.
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Hidden), static_cast<int>(b.view(0, 2)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Hidden), static_cast<int>(b.view(0, 0)));
}

static void test_adjacency_counts() {
  MinesBoard b = rigged3x3();
  TEST_ASSERT_EQUAL_UINT8(2, b.adjacent(1, 0));
  TEST_ASSERT_EQUAL_UINT8(2, b.adjacent(1, 1));
  TEST_ASSERT_EQUAL_UINT8(1, b.adjacent(1, 2));
  TEST_ASSERT_EQUAL_UINT8(0, b.adjacent(2, 2));
}

static void test_reveal_last_safe_cell_wins() {
  MinesBoard b = rigged3x3();
  b.reveal(0, 2);  // the 7th and last safe cell
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Won), static_cast<int>(b.state()));
}

static void test_reveal_mine_loses() {
  MinesBoard b = rigged3x3();
  b.reveal(0, 0);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Lost), static_cast<int>(b.state()));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(0, 0)));
}

static void test_no_ops_after_game_over() {
  MinesBoard b = rigged3x3();
  b.reveal(0, 0);  // Lost
  b.reveal(0, 2);
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Hidden), static_cast<int>(b.view(0, 2)));
  b.toggleFlag(0, 2);
  TEST_ASSERT_EQUAL_UINT16(0, b.flagsPlaced());
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Lost), static_cast<int>(b.state()));
}

static void test_out_of_bounds_are_no_ops() {
  MinesBoard b(3, 3, 2, zero);
  b.reveal(3, 0);
  b.reveal(0, 3);
  b.toggleFlag(200, 200);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Fresh), static_cast<int>(b.state()));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Hidden), static_cast<int>(b.view(200, 200)));
  TEST_ASSERT_FALSE(b.isMine(200, 200));
  TEST_ASSERT_EQUAL_UINT8(0, b.adjacent(200, 200));
}

static void test_hard_preset_geometry() {  // 9x13 board = 13 rows x 9 cols
  g_seed = 99;
  MinesBoard b(13, 9, 25, lcg);
  b.reveal(6, 4);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Playing), static_cast<int>(b.state()));
  uint16_t mines = 0;
  for (uint8_t r = 0; r < 13; ++r)
    for (uint8_t c = 0; c < 9; ++c)
      if (b.isMine(r, c)) ++mines;
  TEST_ASSERT_EQUAL_UINT16(25, mines);
  TEST_ASSERT_EQUAL_UINT8(13, b.rows());
  TEST_ASSERT_EQUAL_UINT8(9, b.cols());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_board_all_hidden);
  RUN_TEST(test_first_reveal_places_mines_outside_safe_zone);
  RUN_TEST(test_first_reveal_corner_safe_zone_clips);
  RUN_TEST(test_zero_rng_layout_is_the_documented_one);
  RUN_TEST(test_flood_fill_stops_at_numbers);
  RUN_TEST(test_adjacency_counts);
  RUN_TEST(test_reveal_last_safe_cell_wins);
  RUN_TEST(test_reveal_mine_loses);
  RUN_TEST(test_no_ops_after_game_over);
  RUN_TEST(test_out_of_bounds_are_no_ops);
  RUN_TEST(test_hard_preset_geometry);
  return UNITY_END();
}
