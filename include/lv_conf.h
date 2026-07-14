/**
 * danios lv_conf.h — LVGL 8.4 configuration for the ESP32-2432S024C.
 *
 * Only deliberate overrides live here; every other option falls back to the
 * LVGL 8.4 default via lv_conf_internal.h. Constraints: 520 KB SRAM, NO PSRAM
 * (roadmap §2) — keep the LVGL heap at 48 KB and features lean.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/*==================== COLOR ====================*/
/* RGB565. LV_COLOR_16_SWAP stays 0: the DisplayService flush callback hands
 * LVGL's buffer to LovyanGFX as lgfx::rgb565_t*, and LovyanGFX converts to the
 * panel's big-endian order during the SPI write (see docs/DISPLAY.md,
 * "LVGL glue"). Image assets stay standard non-swapped RGB565. */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/*==================== MEMORY ====================*/
/* LVGL heap from internal SRAM. Roadmap §2 budgeted 48 KB, but that static
 * pool starves bluedroid on this PSRAM-less WROOM-32: esp_bluedroid_init()
 * alone needs ~61 KB of internal DRAM, and with the 48 KB LVGL pool reserved
 * the full app (LVGL + SD + WiFi residuals) left only ~12 KB of usable heap
 * after BT enable — enough to scan, but the A2DP *connect* then OOM'd
 * allocating its L2CAP/SDP control blocks and bluedroid asserted deep in its
 * own out-of-memory cleanup (vQueueDelete on a never-created semaphore). This
 * UI's live LVGL usage is far under 24 KB, so halving the pool hands ~24 KB
 * back to the heap (~36 KB free after BT enable) — ample for connect.
 * A4 (2026-07-14): sized by measurement during the Music RAM reclaim.
 * 18 KB froze with lv_list-button rows (3 objs + scroll anim per row,
 * ~440 B/row; lv_anim_start OOM-asserted -> silent while(1)). Music's list
 * rows are now single ellipsized labels (~1/3 the cost, no anims), which is
 * what makes 20 KB safe: player UI + a full 28-row view fits with headroom.
 * If a future UI reintroduces button rows or row anims, re-measure with
 * lv_mem_monitor before trusting this number. */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (20U * 1024U)

/*==================== HAL ====================*/
/* Tick straight from Arduino millis(): no lv_tick_inc() calls anywhere. */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DISP_DEF_REFR_PERIOD 30   /* ms — screen refresh cadence */
#define LV_INDEV_DEF_READ_PERIOD 30  /* ms — touch poll cadence     */

/*==================== LOGGING / DEBUG ====================*/
#define LV_USE_LOG 1          /* TEMP DIAGNOSTIC (A4 freeze): printf -> UART0 */
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_USE_PERF_MONITOR 0 /* flip to 1 to see FPS/CPU overlay */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

/*==================== FONTS ====================*/
/* Montserrat 14 (default UI text). More sizes cost flash; enable per-plan as
 * UIs need them (F2+ picks its own heading size).
 *
 * LV_FONT_MONTSERRAT_14 stays on (1) because LVGL's default theme references
 * it directly. The actual UI default font is montserrat_pt_14 (F3-PT): a
 * custom-generated 14px Montserrat that adds Latin-1 accented glyphs
 * (0xA0-0xFF) needed for Portuguese UI text, on top of the same ASCII +
 * FontAwesome symbol glyphs LVGL's built-in fonts carry. See
 * src/assets/fonts/montserrat_pt_14.c for the generation command. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(montserrat_pt_14)
#define LV_FONT_DEFAULT &montserrat_pt_14

/*==================== WIDGETS ====================*/
/* v8 enables the core set by default. Extras this device will not use stay
 * off (flash savings). Later plans flip individual ones back on if needed
 * (e.g. A5 Pet may want LV_USE_ANIMIMG for sprites).
 *
 * Deliberately KEPT ON (defaults) because later foundation plans rely on
 * them: btn, label, img, list (F2 settings menu), slider (F3 brightness),
 * switch (F3 units), dropdown (F3 sleep), roller (F4 clock), textarea +
 * keyboard (F4 wifi password), btnmatrix (A1 calculator), msgbox (F3
 * disabled-app hint), bar, arc, checkbox, table. */
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_LED 0
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

#endif /* LV_CONF_H */
