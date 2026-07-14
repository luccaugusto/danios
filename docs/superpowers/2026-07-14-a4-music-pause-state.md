# A4 Music — pause-state handoff (2026-07-14)

Work on the `a4-music` branch is **paused mid-hardware-debugging**. This doc is
the resume point: what's built, what's proven, what's broken, and exactly where
to pick up.

## Where things stand

- **Branch:** `a4-music` (NOT merged; base = `a77a4d1`, the F5 merge into main).
- **F5 (Bluetooth audio) IS merged to main** as of this session (spike deleted,
  `kSectionNames` conflict resolved).
- **A4 (Music app) + A4.1 (album folders): software-complete and reviewed.**
  All per-task reviews + the whole-branch review passed; native suite 180/180;
  `pio run -e cyd` green. Plans:
  [`2026-07-03-app-music.md`](plans/2026-07-03-app-music.md) (revised 2026-07-08),
  [`2026-07-14-music-albums.md`](plans/2026-07-14-music-albums.md).
- **Hardware E2E has NEVER fully passed**: connect + browse work; **no MP3 has
  audibly played yet**. The last test (with the minimp3 build, commit `9b49138`)
  was reported "doesn't work" by the user with **no serial capture running**
  (board had been unplugged for SD re-encoding), so the failure signature is
  unknown. That capture is resume step 1.

## The debugging story so far (all serial-captured, all measured)

The A4 plan assumed "~90–150 KB free heap with BT up" citing `docs/hardware.md`
figures that were never actually recorded. Reality, measured on this device:

| Stage | Free heap (measured) |
| --- | --- |
| Boot, radios off | 105–123 KB (varies per boot) |
| After BT Classic enable | 40–51 KB (varies; ~47 KB typical post-reclaim) |
| After the A2DP **link** is up | 13–22 KB (the link itself costs **~14 KB**) |

Fix rounds, in order:

1. **Crash on connect** = `PcmRing`'s 32 KB contiguous alloc aborting
   (`bad_alloc` → `abort()`, backtrace decoded to `pcm_ring.cpp:12`).
   `esp_wifi_deinit()` in `RadioManager::stopWiFi()` was tried first and
   **measured a no-op** on arduino-esp32 core 3.x (kept, comment corrected).
2. **RAM reclaim package** (user-approved): draw buffer 30→10 rows
   (`DisplayService.h`), ring 32→8 KB with 64-byte MP3 chunks, LVGL pool
   24→18 KB. Crash fixed; **new freeze on album tap**.
3. **Freeze = LVGL pool exhaustion**: `lv_anim_start` OOM assert = silent
   `while(1)` (LV_USE_LOG was off — no serial). lv_list button rows cost
   ~440 B each + a scroll anim. Fixed: rows are now **single ellipsized
   labels** (`addRowLabel` in MusicApp.cpp), pool settled at **20 KB**
   (18 froze, 24 unneeded — see the measured note in `include/lv_conf.h`).
4. **Freeze on track select = arduino-libhelix**: needs ~30 KB heap per
   `open()` and its allocator **hangs in `while(true)` on OOM**
   (`.pio/libdeps/cyd/libhelix/src/utils/Allocator.h`). With the link up
   there is 13–22 KB — structurally unfittable; every track was refused by
   the interim heap gate.
5. **Decoder swapped to vendored minimp3** (`lib/minimp3/minimp3.h`, CC0):
   ~15.5 KB object + 8 KB ring, allocated in `MusicApp::buildUI` **before**
   `beginConnect` (link cost comes after), zero allocation while decoding,
   40 KB pre-alloc gate with a friendly PT error. Commit `9b49138`.
   **This build is what's flashed on the device and it has NOT been verified.**

## Known confounder: the SD card's files

The user's MP3s are **not 44.1 kHz**. The pipeline (spec contract) rejects any
non-44.1 kHz track as "bad" (greyed row, auto-skip) — there is no resampler.
So "doesn't work" on the last test may be *correct format rejection*, a new
bug, or both. A known-good test file:

```bash
ffmpeg -i input.mp3 -ar 44100 -ac 2 -codec:a libmp3lame -b:a 128k "01 boa.mp3"
```

## TEMP diagnostics still in the tree (deliberate — strip after HW pass)

- `include/lv_conf.h`: `LV_USE_LOG 1` + `LV_LOG_PRINTF` (marked TEMP).
- `src/apps/music/MusicApp.cpp` `showTracksView`: `[music] album tap` +
  `lv_mem_monitor` before/after probes (marked TEMP DIAGNOSTIC).
- `[music] heap after pipeline` and the `[music] open: ... heap` prints are
  **permanent** (they are the budget instrumentation).

## Resume checklist

1. Plug the board in, start a serial capture
   (`scratchpad/serial_capture.py` pattern — background pyserial, NOT
   `pio device monitor`; free the port before flashing). CYD = `/dev/ttyUSB0`.
2. Put at least one known-good 44.1 kHz CBR file on the card (command above).
3. Reproduce: Música → connect → tap album → tap the known-good track.
   Read the capture: expect `[music] heap after pipeline` (~23 KB free),
   `[music] open: ...`, then sound. Failure signatures to look for:
   `low heap ... refusing decoder alloc` (budget), `lv_` log lines (UI pool),
   abort backtrace (decode with
   `xtensa-esp32-elf-addr2line -pfiaC -e .pio/build/cyd/firmware.elf <addrs>`),
   silence + no output (new hang — get a backtrace via EN-button reset paired
   with the capture).
4. When sound works: run the combined A4+A4.1 hardware checklist
   (A4 plan Task 6 + albums plan Task 4 Step 3), record heap figures in
   `docs/hardware.md`, strip the TEMP diagnostics, commit.
5. Merge gate: `a4-music` → main only after the full HW pass
   (A1/A2/A3 precedent). The SDD ledger (`.superpowers/sdd/progress.md`,
   git-ignored) has the complete per-task history.

## Also on the working tree (user's, uncommitted, unrelated to A4)

- `README.md`: weather-art TODO checkboxes ticked (all 7 backgrounds drawn).
- `assets/art/weather/bg_*.png`: the drawn background sources (untracked).
  These are the user's to commit alongside the converted `.bin`s when ready.
