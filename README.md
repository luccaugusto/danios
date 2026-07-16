# danios

A small, battery-powered 2.4" touchscreen "app phone" built on an ESP32
("Cheap Yellow Display"). Home-screen launcher with five apps — **Weather,
Music, Calculator, Oracle, Pet** — plus a Settings screen. Personal gift project.

- **Board:** ESP32-2432S024 (2.4" CYD, **resistive** XPT2046 touch on the shared
  display SPI — not the capacitive C variant; see `docs/hardware.md`).
  ESP32-WROOM-32 — has Bluetooth Classic / A2DP, which the S3/C3 don't.
- **Full design:** [`docs/superpowers/specs/2026-06-03-esp32-gift-device-design.md`](docs/superpowers/specs/2026-06-03-esp32-gift-device-design.md)
  — architecture, apps, radio strategy, APIs, testing. **Approved, ready to build.**

---

## Development environment setup

We use **PlatformIO** (not the Arduino IDE) because the design calls for
off-device unit tests, and PlatformIO gives us a `native` test environment plus
clean dependency pinning in one `platformio.ini`.

Steps below are for **Arch Linux**.

```bash
# 1. Install PlatformIO Core
curl -fsSL -o /tmp/get-platformio.py \
  https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 /tmp/get-platformio.py
#   then add ~/.platformio/penv/bin to your PATH (the installer prints the exact line)

# 2. Serial port access (needs logout/login or reboot to take effect)
sudo usermod -aG uucp lucca

# 3. Plug the board in over USB-C, then check it enumerated:
dmesg | tail          # expect a ch341 (or cp210x) line + /dev/ttyUSB0
```

> **⚠️ Python 3.14 caveat:** if the PlatformIO installer chokes on a bleeding-edge
> Python, install an older one and retry:
> `pyenv install 3.12 && pyenv shell 3.12`, then re-run the installer.
> PlatformIO builds its own isolated venv, so this only affects install.

### Verify the setup

```bash
pio --version         # PlatformIO Core X.Y.Z
groups | grep uucp    # should list uucp (after re-login)
pio device list       # should show /dev/ttyUSB0 once the board is plugged in
```

### Gotchas

- **Serial permission denied** on flash → you're not in the `uucp` group yet, or
  haven't logged out/in since adding yourself. On Arch, `/dev/ttyUSB*` is owned by
  group `uucp` (not `dialout`).
- **USB-serial driver** → nothing to install; the CH340 (`ch341`) / CP2102
  (`cp210x`) drivers are in the mainline kernel and autoload on plug-in.
- **Display config is the #1 pitfall.** The CYD's exact panel wiring (pins, color
  inversion, rotation) is easy to get wrong. The working, hardware-verified
  config for this unit lives in `include/LGFX_ESP32_2432S024.hpp` — don't
  re-derive it (`docs/DISPLAY.md` has the story). External references:
  - [`edmasini/esp32-2432S024-Capacitive`](https://github.com/edmasini/esp32-2432S024-Capacitive) — the **C (capacitive) variant**, close but NOT this unit (ours is resistive)
  - [`witnessmenow/ESP32-Cheap-Yellow-Display`](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) — general CYD resources

---

## What goes where

**On the board (flash, ~4 MB)** — code + small config, flashed over USB-C:

- The firmware binary (drivers, services, app framework, and vendored libs:
  LVGL, LovyanGFX, ArduinoJson, ESP32-A2DP, arduino-audio-tools).
- **NVS** (flash key-value store): WiFi credentials, paired-Bluetooth address,
  units, brightness, sleep timeout, cached geolocation/weather. Written by the
  firmware at runtime (not pre-loaded); survives SD swaps.
- **Partition table:** OTA is a non-goal, so use a no-OTA **`huge_app`** scheme —
  LVGL + A2DP + MP3 decode makes a large binary that won't fit the default app
  slot. Set this in `platformio.ini`.

**On the microSD card (FAT32, ≤32 GB)** — bulky content editable without reflashing:

```
/music/*.mp3           ← songs
/oracle/wisdom.txt     ← one wisdom entry per line
/art/boot-logo.bin     ← boot splash logo (240x288)
/art/weather/          ← outfits, condition overlays, backgrounds
/art/icons/            ← 5 app icons + gear
/art/oracle/           ← oracle frame
/art/pet/              ← egg/baby/child/teen/adult sprites, food icons, mess icon
```

None of this is needed to *start* — the art is hand-drawn later, music and
`wisdom.txt` seeded whenever. For early bring-up you just need any FAT32 card
inserted to prove `StorageService` mounts it.

> **Rule of thumb:** code + settings → flash; big files a non-programmer should be
> able to change → SD.

---

## Build order

Bring-up sequence (from the spec's testing strategy) — each proven before
stacking the next:

```
display → touch → SD → WiFi/weather → Bluetooth/audio
```

**Milestone #1:** flash a minimal sketch that lights up the display
("hello danios" via LovyanGFX). Proves the PC can build, the board flashes,
serial permissions work, and the display config is correct.

Pure logic (calculator engine, oracle date→entry picker, weather mapping, JSON
parsing) is developed test-first in the `native` environment, no board required.

---

## Next step

Design is approved and hardware is in hand. Pick up at either:

1. **Write the implementation plan** — sequence the spec into milestones
   (milestone 0 = toolchain + display bring-up, then up the layers).
2. **Get the board alive first** — install PlatformIO, wire up the CYD display
   config, flash the "hello" sketch, then plan the rest.

---

## TODO
- [ ] draw the images for the pet app — 10 sprites, `.bin` under `S:/art/pet/`
      (paths from `lib/pet_model/pet_model.cpp`, render sizes from
      `src/apps/pet/PetApp.cpp`). Same workflow as weather:
      `assets/icons/svg_to_lvgl_bin.py <png> sd/art/pet/<name>.bin --size WxH`.
      Every slot shows a colored placeholder box until its file exists — the
      app is fully usable without art.
  - Growth-stage sprites (rendered 104×104 on the alive screen):
    - [ ] `egg.bin` — also the egg screen, rendered 120×120 there
    - [ ] `baby.bin` — days 0–2
    - [ ] `child.bin` — days 3–9
    - [ ] `teen.bin` — days 10–20
    - [ ] `adult.bin` — day 21+
  - Mess (rendered 30×30, up to 3 stacked in a row):
    - [ ] `mess.bin`
  - Food (reserved paths; the tray currently uses text buttons, so these are
    for a future tray upgrade):
    - [ ] `food_snack.bin`
    - [ ] `food_meal.bin`
    - [ ] `food_treat.bin`
- [ ] rename pet app from bichinho to Pet
- [ ] draw the images for the pomodoro app — 2 sprites. Same workflow
      as weather/pet: `assets/icons/svg_to_lvgl_bin.py <png> sd/art/pomo/<name>.bin`,
      then copy `sd/` onto the card. The app shows colored placeholder boxes
      (red = trabalho, green = pausa) until the files exist.
  - Status sprites (rendered 120×120 in `src/apps/pomodoro/PomodoroApp.cpp`):
    - [ ] `sd/art/pomo/work.bin` — work-phase sprite
    - [ ] `sd/art/pomo/break.bin` — break-phase sprite
- [ ] make app fonts a little smaller so they don't overflow
