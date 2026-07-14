// Host-side tests for Playlist (pio test -e native). Covers the spec's pinned
// native cases: empty list, single track, wraparound, skip-on-error.
#include <unity.h>

#include "playlist.h"

void setUp() {}
void tearDown() {}

static Playlist make3() {
  Playlist p;
  p.setFiles({"01 alfa.mp3", "02 beto.mp3", "03 celia.mp3"});
  return p;
}

static void test_empty_list() {
  Playlist p;
  TEST_ASSERT_EQUAL_INT(0, p.count());
  TEST_ASSERT_EQUAL_INT(-1, p.currentIndex());
  TEST_ASSERT_FALSE(p.next());
  TEST_ASSERT_FALSE(p.previous());
  TEST_ASSERT_FALSE(p.markCurrentBad());
  TEST_ASSERT_EQUAL_STRING("", p.fileAt(0).c_str());
}

static void test_set_files_starts_at_first() {
  Playlist p = make3();
  TEST_ASSERT_EQUAL_INT(3, p.count());
  TEST_ASSERT_EQUAL_INT(3, p.playableCount());
  TEST_ASSERT_EQUAL_INT(0, p.currentIndex());
  TEST_ASSERT_EQUAL_STRING("01 alfa.mp3", p.fileAt(0).c_str());
}

static void test_title_strips_extension_case_insensitive() {
  Playlist p;
  p.setFiles({"Minha Musica.MP3", "semextensao"});
  TEST_ASSERT_EQUAL_STRING("Minha Musica", p.titleAt(0).c_str());
  TEST_ASSERT_EQUAL_STRING("semextensao", p.titleAt(1).c_str());
}

static void test_single_track_wraps_to_itself() {
  Playlist p;
  p.setFiles({"only.mp3"});
  TEST_ASSERT_TRUE(p.next());
  TEST_ASSERT_EQUAL_INT(0, p.currentIndex());
  TEST_ASSERT_TRUE(p.previous());
  TEST_ASSERT_EQUAL_INT(0, p.currentIndex());
}

static void test_next_previous_wrap() {
  Playlist p = make3();
  TEST_ASSERT_TRUE(p.previous());  // 0 -> wraps back to 2
  TEST_ASSERT_EQUAL_INT(2, p.currentIndex());
  TEST_ASSERT_TRUE(p.next());      // 2 -> wraps forward to 0
  TEST_ASSERT_EQUAL_INT(0, p.currentIndex());
}

static void test_select() {
  Playlist p = make3();
  TEST_ASSERT_TRUE(p.select(2));
  TEST_ASSERT_EQUAL_INT(2, p.currentIndex());
  TEST_ASSERT_FALSE(p.select(3));   // out of range
  TEST_ASSERT_FALSE(p.select(-1));
  TEST_ASSERT_EQUAL_INT(2, p.currentIndex());  // unchanged on failure
}

static void test_mark_bad_advances() {
  Playlist p = make3();
  TEST_ASSERT_TRUE(p.markCurrentBad());  // 0 goes bad -> current becomes 1
  TEST_ASSERT_EQUAL_INT(1, p.currentIndex());
  TEST_ASSERT_TRUE(p.isBad(0));
  TEST_ASSERT_EQUAL_INT(2, p.playableCount());
}

static void test_navigation_skips_bad_tracks() {
  Playlist p = make3();
  p.select(1);
  p.markCurrentBad();               // 1 bad -> current 2
  TEST_ASSERT_EQUAL_INT(2, p.currentIndex());
  TEST_ASSERT_TRUE(p.next());       // 2 -> 0
  TEST_ASSERT_EQUAL_INT(0, p.currentIndex());
  TEST_ASSERT_TRUE(p.next());       // skips bad 1 -> lands on 2
  TEST_ASSERT_EQUAL_INT(2, p.currentIndex());
  TEST_ASSERT_FALSE(p.select(1));   // bad track is not selectable
}

static void test_all_bad_gives_up() {
  Playlist p = make3();
  TEST_ASSERT_TRUE(p.markCurrentBad());
  TEST_ASSERT_TRUE(p.markCurrentBad());
  TEST_ASSERT_FALSE(p.markCurrentBad());  // the last playable went bad
  TEST_ASSERT_EQUAL_INT(-1, p.currentIndex());
  TEST_ASSERT_EQUAL_INT(0, p.playableCount());
  TEST_ASSERT_FALSE(p.next());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_list);
  RUN_TEST(test_set_files_starts_at_first);
  RUN_TEST(test_title_strips_extension_case_insensitive);
  RUN_TEST(test_single_track_wraps_to_itself);
  RUN_TEST(test_next_previous_wrap);
  RUN_TEST(test_select);
  RUN_TEST(test_mark_bad_advances);
  RUN_TEST(test_navigation_skips_bad_tracks);
  RUN_TEST(test_all_bad_gives_up);
  return UNITY_END();
}
