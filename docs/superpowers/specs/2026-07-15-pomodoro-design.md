# Pomodoro app — design

**Date:** 2026-07-15
**Status:** approved
**Context:** replaces the deferred Music app (A4) in the launcher lineup,
alongside Minesweeper (see `2026-07-15-minesweeper-design.md`).

## Goal

A pomodoro timer app: one Start/Stop button, configurable work/break section
lengths (default 25 min work / 5 min break), a big status sprite showing the
current phase, and a launcher badge while the timer runs. The timer keeps
counting when the user leaves the app.

## Non-goals

- Long breaks after N cycles, daily pomodoro counters, statistics.
- Audio or LED alerts (no audio path with Music deferred; cue is visual only).
- Timer persistence across reboot — a reboot cancels the timer.

## Architecture

House pattern, two units:

1. **`lib/pomodoro_model/`** (pure C++17, zero Arduino/LVGL) — the timer
   state machine, driven entirely by caller-supplied `millis()` timestamps.
2. **`src/apps/pomodoro/PomodoroApp`** — thin LVGL UI + NVS config
   persistence + launcher badge feed.

Catalog entry `catalog::kPomodoro{"Pomodoro", nullptr}` (icon path becomes
`"S:/art/icons/pomodoro.bin"` once the art exists, per app_catalog rules).
App id `"pomodoro"`, `requiredRadio() == RadioMode::None`. Registered in
`main.cpp` before Settings (Settings stays the last grid icon).

### PomoTimer public API

```cpp
enum class PomoPhase : uint8_t { Idle, Work, Break };

struct PomoConfig {
  uint16_t work_min  = 25;
  uint16_t break_min = 5;
};

class PomoTimer {
 public:
  void       configure(const PomoConfig& c);  // ignored unless Idle
  PomoConfig config() const;
  void       start(uint32_t now_ms);          // Idle -> Work
  void       stop();                          // any -> Idle
  PomoPhase  phase(uint32_t now_ms);          // resolves elapsed transitions
  uint32_t   remainingMs(uint32_t now_ms);    // 0 when Idle
  bool       running() const;                 // phase != Idle
};
```

## Timer semantics

- All time math is `uint32_t` millis with unsigned subtraction, so the
  49.7-day `millis()` wrap is safe as long as the timer is queried at least
  once per wrap period — guaranteed by the ~1 Hz badge recompute in `loop()`.
- `phase(now)` / `remainingMs(now)` resolve transitions lazily with a
  catch-up loop: while `now - phase_start >= phase_len`, advance
  Work→Break→Work and add the consumed length to `phase_start`. Being away
  for several sections resolves correctly on the next query.
- The cycle runs Work→Break→Work forever until `stop()`.
- `configure()` while running is ignored (UI also disables the steppers);
  config changes take effect on the next `start()`.
- The model has **no TimeService dependency** — relative time only, works
  identically whether or not NTP ever synced.

## Background behavior

The `PomodoroApp` instance is a boot-time static (like every app), so the
model lives across enter/exit. Leaving the app does not stop the timer.

- `main.cpp` recomputes `launcher.setBadge("pomodoro", timer.running())`
  on boot and ~1 Hz from `loop()`, exactly like the Pet badge. The recompute
  calls `phase(millis())` so background transitions also resolve there.
- On boot the timer is Idle (no persistence — see non-goals), so the badge
  recompute is trivially false until the user starts a timer.

## UI (portrait 240×320, top bar as in other apps)

Single screen, top to bottom:

- **Status sprite** — 120×120 image: `S:/art/pomo/work.bin` during Work,
  `S:/art/pomo/break.bin` during Break. Until the art exists, a colored
  placeholder box (red = work, green = break, gray = idle) with a text
  label, following the Pet placeholder pattern. Idle shows the work sprite
  dimmed (or gray placeholder).
- **Countdown label** — large `mm:ss` of `remainingMs`, updated in `tick()`
  (~1 Hz is enough). Shows the configured work length while Idle.
- **Start/Stop button** — one big toggle: "Iniciar" when Idle, "Parar" when
  running.
- **Config steppers** — two rows, disabled while running, persisted to NVS
  on change:
  - `Trabalho: NN min  [−][+]` — 5-min steps, clamped 5–60.
  - `Pausa: NN min  [−][+]` — 1-min steps, clamped 1–15.

**Phase-flip cue:** when `tick()` observes the phase changed while the app
is open, the sprite swaps and a full-screen overlay flashes 3 quick blinks.

## Persistence

Via `ISettingsStore` (NVS): `pomo_work_min` (u32, default 25),
`pomo_break_min` (u32, default 5). Loaded in `setDeps`/first enter, written
on stepper change. Running state is RAM-only.

## Error handling

No radio, no I/O beyond NVS. Out-of-range NVS values (manual tampering /
old versions) are clamped into the stepper ranges on load.

## Art (append to README TODO)

- `sd/art/icons/pomodoro.bin` — launcher icon, wired in `app_catalog.h`.
- `sd/art/pomo/work.bin`, `sd/art/pomo/break.bin` — status sprites,
  rendered 120×120. Same PNG → `svg_to_lvgl_bin.py` workflow as weather/pet.
  App fully usable with placeholders until drawn.

## Testing

- **Native (`lib/pomodoro_model` tests, test-first):** start/stop
  transitions; work→break→work rollover at exact boundaries; catch-up over
  multiple missed sections; remainingMs at boundaries; millis-wrap math
  (timestamps straddling 0xFFFFFFFF); configure ignored while running;
  clamping semantics live in the UI, not the model.
- **On-device:** start → leave app → badge dot visible → re-enter shows
  correct remaining time; phase flip flashes + swaps sprite; steppers
  disabled while running; values persist across reboot; reboot mid-timer
  yields Idle + no badge.
