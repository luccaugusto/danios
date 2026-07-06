# danios app spec — Music

**Extracted:** 2026-07-06 from the [master spec](../2026-06-03-esp32-gift-device-design.md) §4.2, §6.3, §6.5, §7.3, §8.
**Interfaces:** the [roadmap](../../plans/2026-07-03-danios-roadmap.md) §4 is authoritative — never rename its names/paths/keys.
**Roadmap slot:** A4 (`lib/playlist/` reserved in roadmap §3). Depends on F3 (SD) + F5 (Bluetooth) — the last app that can start.

---

## What it is

MP3 playback from the SD card, streamed over Bluetooth (A2DP source) to a
speaker/earbuds. App id `"music"` (pinned), replaces the `musicStub`
registration in `src/main.cpp`. `requiredRadio()` = `Bluetooth`.

## Flow on open

1. Launcher already requested Bluetooth via `RadioManager`.
2. Auto-connect to the **last paired device**
   (`BluetoothAudioService::pairedAddr()`, NVS `bt.addr`).
3. **If nothing is paired → redirect to Settings → Bluetooth** to pick a
   device (spec §6.5 — friendly message + navigation, not a dead end).
4. Scan `/music` on the SD card (`StorageService::listFiles("/music", ".mp3")`,
   sorted, non-recursive) to build the playlist.

## Playback pipeline

Decode MP3 (**libhelix** via **arduino-audio-tools**, pschatzmann) → PCM →
feed the A2DP source callback → streamed to the speaker. The callback contract
is pinned (roadmap §4.10):

```cpp
// fill up to `frames` stereo int16 frames at 44100 Hz, return frames written.
// Return 0 = silence.
using AudioSourceFn = int32_t (*)(int16_t* stereo_buf, int32_t frames, void* ctx);
```

The Music app plugs its decoder in with
`BluetoothAudioService::setSource(fn, ctx)`; the streaming pump runs from the
main loop (`tick()` / service tick — spec §3.5).

**RAM constraint (critical):** no PSRAM. Budget from the roadmap: BT Classic
~64 KB + MP3 decode ~30 KB alongside LVGL buffers/heap and SD reads. Keep MP3s
at a moderate, **constant** bitrate. Buffers must be sized deliberately;
measure free heap in About after integration.

## Controls (touch UI)

Play/pause, next, previous, scrollable track list, volume, now-playing title
(from **filename**; ID3 tag only if straightforward). Progress bar = optional
polish. **AVRCP is a non-goal** (no control from the speaker's buttons).

## Architecture (roadmap conventions)

- **Pure logic:** `lib/playlist/` — std C++17 only: track ordering,
  next/previous with wraparound, current-index bookkeeping, skip-bad-track
  logic. Native-tested in `test/test_playlist/` (empty list, single track,
  wrap, skip-on-error).
- **Thin UI wrapper:** `src/apps/music/MusicApp.{h,cpp}` — an `App` (roadmap
  §4.5) consuming `BluetoothAudioService` (§4.10) and `StorageService` (§4.9).
- Decoder wiring lives with the app (it owns the SD-file → PCM path), not in
  the service — the service only moves PCM frames.

## Errors (spec §4.2, §6.5)

| Situation | Behavior |
| --- | --- |
| No music on card | Friendly empty state ("put .mp3 files in /music"). |
| No paired device | Redirect to Settings → Bluetooth. |
| Bad/unreadable MP3 | Skip to next track (playlist logic handles it). |
| SD missing | App disabled in launcher (F3 wiring). |

## Name & icon

Launcher label and icon come from `catalog::kMusic` in
`src/apps/app_catalog.h`. Icon file (when drawn): `S:/art/icons/music.bin`;
`nullptr` until then.

## E2E outcome (roadmap §1)

Pick an MP3 from the SD card, hear it on the Bluetooth speaker.
