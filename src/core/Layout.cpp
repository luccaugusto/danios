#include "core/Layout.h"

namespace layout {

bool kLandscape = false;
lv_coord_t kScreenW = 240;
lv_coord_t kScreenH = 320;
lv_coord_t kAppW = 240;
lv_coord_t kAppH = 320 - kTopBarH;
int kGridCols = 3;

void init(bool landscape) {
  kLandscape = landscape;
  kScreenW = landscape ? 320 : 240;
  kScreenH = landscape ? 240 : 320;
  kAppW = kScreenW;
  kAppH = kScreenH - kTopBarH;
  kGridCols = landscape ? 4 : 3;
}

}  // namespace layout
