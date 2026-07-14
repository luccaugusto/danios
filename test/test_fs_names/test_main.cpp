#include <unity.h>
#include <fs_names.h>

void setUp() {}
void tearDown() {}

void test_has_extension_case_insensitive() {
  TEST_ASSERT_TRUE(hasExtension("song.mp3", ".mp3"));
  TEST_ASSERT_TRUE(hasExtension("SONG.MP3", ".mp3"));
  TEST_ASSERT_TRUE(hasExtension("icon.bin", ".BIN"));
  TEST_ASSERT_TRUE(hasExtension("mixed.Mp3", ".mP3"));
}

void test_has_extension_rejects_wrong_and_short() {
  TEST_ASSERT_FALSE(hasExtension("song.wav", ".mp3"));
  TEST_ASSERT_FALSE(hasExtension("mp3", ".mp3"));      // shorter than ext
  TEST_ASSERT_FALSE(hasExtension("", ".mp3"));
  TEST_ASSERT_FALSE(hasExtension("song.mp3x", ".mp3")); // suffix only
}

void test_empty_ext_matches_everything() {
  TEST_ASSERT_TRUE(hasExtension("anything.xyz", ""));
  TEST_ASSERT_TRUE(hasExtension("noext", ""));
}

void test_hidden_name_detection() {
  TEST_ASSERT_TRUE(isHiddenName(".DS_Store"));
  TEST_ASSERT_TRUE(isHiddenName(".hidden"));
  TEST_ASSERT_FALSE(isHiddenName("song.mp3"));
  TEST_ASSERT_FALSE(isHiddenName(""));
}

void test_filter_skips_dirs_and_hidden() {
  std::vector<FsEntry> in = {
      {"song.mp3", false},
      {".DS_Store", false},   // hidden -> skipped
      {"albums", true},       // directory -> skipped
      {"other.mp3", false},
  };
  auto out = filterAndSortNames(in, ".mp3");
  TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)out.size());
  TEST_ASSERT_EQUAL_STRING("other.mp3", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("song.mp3", out[1].c_str());
}

void test_filter_by_extension_case_insensitive() {
  std::vector<FsEntry> in = {
      {"b.MP3", false},
      {"readme.txt", false},
      {"a.mp3", false},
  };
  auto out = filterAndSortNames(in, ".mp3");
  TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)out.size());
  TEST_ASSERT_EQUAL_STRING("a.mp3", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("b.MP3", out[1].c_str());
}

void test_sort_is_bytewise_ascending() {
  std::vector<FsEntry> in = {
      {"b.bin", false},
      {"C.bin", false},   // 'C' (0x43) < 'a' (0x61) byte-wise
      {"a.bin", false},
  };
  auto out = filterAndSortNames(in, ".bin");
  TEST_ASSERT_EQUAL_UINT32(3u, (uint32_t)out.size());
  TEST_ASSERT_EQUAL_STRING("C.bin", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("a.bin", out[1].c_str());
  TEST_ASSERT_EQUAL_STRING("b.bin", out[2].c_str());
}

void test_empty_ext_keeps_all_files_but_not_dirs() {
  std::vector<FsEntry> in = {
      {"wisdom.txt", false},
      {"art", true},
      {"notes.md", false},
  };
  auto out = filterAndSortNames(in, "");
  TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)out.size());
  TEST_ASSERT_EQUAL_STRING("notes.md", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("wisdom.txt", out[1].c_str());
}

void test_dir_filter_keeps_only_visible_dirs() {
  std::vector<FsEntry> in = {
      {"song.mp3", false},  // file -> skipped
      {"Zebra", true},
      {".Trash", true},     // hidden dir -> skipped
      {"Abba", true},
  };
  auto out = filterAndSortDirNames(in);
  TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)out.size());
  TEST_ASSERT_EQUAL_STRING("Abba", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("Zebra", out[1].c_str());
}

void test_dir_filter_empty_input() {
  auto out = filterAndSortDirNames({});
  TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)out.size());
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_has_extension_case_insensitive);
  RUN_TEST(test_has_extension_rejects_wrong_and_short);
  RUN_TEST(test_empty_ext_matches_everything);
  RUN_TEST(test_hidden_name_detection);
  RUN_TEST(test_filter_skips_dirs_and_hidden);
  RUN_TEST(test_filter_by_extension_case_insensitive);
  RUN_TEST(test_sort_is_bytewise_ascending);
  RUN_TEST(test_empty_ext_keeps_all_files_but_not_dirs);
  RUN_TEST(test_dir_filter_keeps_only_visible_dirs);
  RUN_TEST(test_dir_filter_empty_input);
  return UNITY_END();
}
