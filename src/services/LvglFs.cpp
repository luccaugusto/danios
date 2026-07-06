#include "LvglFs.h"

#include <SD.h>
#include <lvgl.h>

#include <cstdio>

namespace {

void* fsOpen(lv_fs_drv_t* /*drv*/, const char* path, lv_fs_mode_t mode) {
  if (mode != LV_FS_MODE_RD) return nullptr;  // read-only driver
  char full[128];
  snprintf(full, sizeof(full), "%s%s", (path[0] == '/') ? "" : "/", path);
  File f = SD.open(full, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return nullptr;
  }
  return new File(f);  // heap-owned handle; freed in fsClose
}

lv_fs_res_t fsClose(lv_fs_drv_t* /*drv*/, void* file_p) {
  File* f = static_cast<File*>(file_p);
  f->close();
  delete f;
  return LV_FS_RES_OK;
}

lv_fs_res_t fsRead(lv_fs_drv_t* /*drv*/, void* file_p, void* buf, uint32_t btr,
                   uint32_t* br) {
  File* f = static_cast<File*>(file_p);
  *br = static_cast<uint32_t>(f->read(static_cast<uint8_t*>(buf), btr));
  return LV_FS_RES_OK;
}

lv_fs_res_t fsSeek(lv_fs_drv_t* /*drv*/, void* file_p, uint32_t pos,
                   lv_fs_whence_t whence) {
  File* f = static_cast<File*>(file_p);
  SeekMode m = SeekSet;
  if (whence == LV_FS_SEEK_CUR) m = SeekCur;
  else if (whence == LV_FS_SEEK_END) m = SeekEnd;
  return f->seek(pos, m) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

lv_fs_res_t fsTell(lv_fs_drv_t* /*drv*/, void* file_p, uint32_t* pos_p) {
  File* f = static_cast<File*>(file_p);
  *pos_p = static_cast<uint32_t>(f->position());
  return LV_FS_RES_OK;
}

lv_fs_drv_t g_drv;  // must outlive registration — static storage

}  // namespace

void lvglFsRegisterSd() {
  lv_fs_drv_init(&g_drv);
  g_drv.letter = 'S';
  g_drv.open_cb = fsOpen;
  g_drv.close_cb = fsClose;
  g_drv.read_cb = fsRead;
  g_drv.seek_cb = fsSeek;
  g_drv.tell_cb = fsTell;
  lv_fs_drv_register(&g_drv);
}

bool lvglFsExists(const char* path) {
  lv_fs_file_t f;
  if (lv_fs_open(&f, path, LV_FS_MODE_RD) != LV_FS_RES_OK) return false;
  lv_fs_close(&f);
  return true;
}
