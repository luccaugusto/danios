#include <esp_system.h>

#include <cstdio>

#include "apps/settings/Sections.h"
#include "core/Version.h"
#include "services/StorageService.h"

void buildAboutSection(lv_obj_t* parent, StorageService& storage) {
  char buf[128];
  snprintf(buf, sizeof(buf),
           "danios %s\n\n"
           "Memória livre: %u bytes\n"
           "Cartão SD: %s",
           DANIOS_VERSION, static_cast<unsigned>(esp_get_free_heap_size()),
           storage.mounted() ? "montado" : "não encontrado");
  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, buf);
}
