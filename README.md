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

- [x] draw the images for weather app — 16 sprites, `.bin` under `S:/art/weather/`
      (paths from `lib/weather_model/weather_model.cpp`, sizes from `WeatherApp.cpp`).
      PNGs live in `assets/art/weather/`; convert with
      `assets/icons/svg_to_lvgl_bin.py <png> sd/art/weather/<name>.bin`
      (character/outfit/overlay sprites: add `--size 188x222` — 95% of the
      PNG canvas), then copy `sd/` onto the card.
  - Character, outfits, and overlays share one 198×234 canvas, exported
    pre-positioned relative to the character — the app stacks them at the
    same anchor, no offsets in code. On-device they render at 188×222.
  - Character (198×234):
    - [x] `gata-coco.bin` — base sprite, always shown
  - Outfits (198×234), one per temperature band:
    - [x] `outfit_freezing.bin` — < 0 °C (heavy coat, hat, gloves)
    - [x] `outfit_cold.bin` — 0–14 °C (jacket)
    - [x] `outfit_mild.bin` — 15–23 °C (long sleeves)
    - [x] `outfit_warm.bin` — 24–27 °C (t-shirt)
    - [x] `outfit_hot.bin` — ≥ 28 °C (shorts / tank top)
  - Overlays (198×234):
    - [x] `ov_sunglasses.bin` — clear + day (except mild)
    - [x] `ov_umbrella.bin` — rain (except mild), storm
    - [x] `ov_scarf.bin` — snow
    - [x] `ov_hat.bin` — mild + (clear day or rain)
  - Backgrounds (240×288):
    - [x] `bg_clear.bin`
    - [x] `bg_clear_night.bin`
    - [x] `bg_cloudy.bin`
    - [x] `bg_fog.bin`
    - [x] `bg_rain.bin`
    - [x] `bg_snow.bin`
    - [x] `bg_storm.bin`
- [x] make a boot screen like the wifi connecting screen we have today but that
      shows always, wifi connection happens at this step — `assets/art/boot-logo.png`
      converted to `sd/art/boot-logo.bin`, shown ≥2 s in `main.cpp` with the
      "Conectando" label in the strip below it while WiFi comes up
- [x] make a cristal ball svg for the oracle app — `assets/icons/oracle.svg`,
      converted to `sd/art/icons/oracle.bin` and wired in `app_catalog.h`
