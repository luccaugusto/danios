# Vendor reference notes — NoosaHydro/2.4inch_ESP32-2432S024

> **RESOLUTION (2026-07-04):** the NACK mystery is solved — our unit is the
> **RESISTIVE variant** (XPT2046 on the shared display SPI: CLK 14, MOSI 13,
> MISO 12, CS 33, PENIRQ 36). There is no CST820 on the board; the entire
> CST820 section below applies only to the C variant. Diag data + the working
> calibration are in `.superpowers/sdd/progress.md` and
> `include/LGFX_ESP32_2432S024C.hpp`. Beware: the repo's 7_1 LovyanGFX
> example uses the 2.8" CYD's separate touch pins (25/32/39) — WRONG for this
> board; the schematic + factory R demo (shared bus) are what matched.

**Cloned to:** `reference/2.4inch_ESP32-2432S024/` (gitignored; re-clone from
https://github.com/NoosaHydro/2.4inch_ESP32-2432S024 if missing).
This is a fan-maintained mirror of the manufacturer's (Shenzhen Jingcai /
"Sunton") support pack for our exact board, **including the factory demo that
shipped on our unit** (the touchscreen demo in the repo README's front photo).

Written 2026-07-04 while F1 Task 6 is paused on the CST820-NACKs-every-poll bug.
Read alongside `.superpowers/sdd/progress.md` (the debug ledger — updated with
these findings).

---

## Map of the repo (what's worth opening)

| Path | What it is |
| --- | --- |
| `1-Demo/Demo_Arduino/1_2_Factory_samples_Capacitive_touch/` | **The factory demo source for our (capacitive) variant.** Contains the vendor's `CST820.cpp/.h` driver — the single most useful file for our bug. |
| `1-Demo/Demo_Arduino/Factory_samples_Capacitive_touch_with_OTA/` | Repo owner's copy of the same demo, builds with OTA; CST820 driver is byte-identical (one uninitialized-var fix). |
| `1-Demo/Demo_Arduino/1_1_Factory_samples_Resistive_touch/` | Factory demo for the **resistive** (XPT2046) variant — see "board variants" below. |
| `1-Demo/Demo_Arduino/7_1_Touch_button_ILI9341_LovyanGFX/` | LovyanGFX touch-button example — but for the **resistive** variant (`Touch_XPT2046`), not ours. |
| `8-Burn operation/Burn files/Factory_samples_Capacitive_touch.bin` | **Prebuilt factory firmware.** Flash it back to prove/disprove hardware: if factory touch works, our code is the problem. |
| `5-Schematic/*.png` | Full board schematics — touch wiring + pull-ups confirmed (below). |
| `2-Specification/`, `6-User_Manual/` | Board spec + getting-started PDFs. |
| `4-Driver_IC_Data_Sheet/` | Datasheets: ESP32-WROOM-32, flash, panel module (`JC2432A024N规格书.pdf`), amp. No CST820 datasheet here. |
| `cnd-micropython/bin/touchtest.py` | Repo owner's minimal MicroPython CST820 poke — a second working reference (see below). |
| `Downloads/2.4inch_ESP32-2432S024.zip` (→ `Downloads/unzipped/`) | Pristine manufacturer snapshot; same content as the repo folders (repo owner added `lv_conf.h`/`User_Setup.h` copies inside demo dirs on top of it). |
| `7-Character&Picture_Molding_Tool/*.rar` | Windows utilities (font tools, CH340 driver) — irrelevant, left unextracted. |

## Board variants — one PCB, two touch options (schematic p.2)

The PCB has footprints for **both** touch systems; only one is populated:

- **Capacitive (ESP32-2432S024C, ours):** CST820 on I²C — `CTP_SDA=IO33`,
  `CTP_SCL=IO32`, `CTP_RST=IO25`, `CTP_INT=IO21` (via 0 Ω R25), addr `0x15`,
  with **4.7 kΩ pull-ups to 3.3 V (R22/R23) on SDA/SCL** on the board.
  → Our pin/addr constants in `TouchService.cpp` are confirmed correct.
- **Resistive (R variant):** XPT2046 on SPI — `TP_CS=IO33`, `TP_CLK=IO14`,
  `TP_DIN=IO13`, `TP_OUT=IO12`, `TP_IRQ=IO36`. Note it **reuses IO33** (our
  SDA) as chip-select — on an R board, I²C at 0x15 NACKs forever.
  A full-bus I²C scan finding *nothing* would point here (unlikely: the
  as-shipped demo on our unit was the capacitive one, but cheap to rule out).

## The vendor CST820 driver — differences from our `TouchService`

Source: `1_2_Factory_samples_Capacitive_touch/Factory_samples_Capacitive_touch/CST820.{h,cpp}`.
Vendor `begin()` sequence, in order:

1. `Wire.begin(sda, scl)` — **default 100 kHz** (we use 400 kHz).
2. **INT pin (IO21) driven as OUTPUT: HIGH 1 ms → LOW 1 ms.** A wake wiggle
   *before* reset. **We never touch INT at all** — this is the biggest delta.
3. RST low 10 ms → high, then **300 ms** wait (we do the same, ✓).
4. Write reg `0xFE = 0xFF` — disable auto-sleep (we write `0x01`; datasheet
   says any non-zero works, but `0xFF` is the proven value).

Vendor read paths (their `getTouch`):

- Single-register reads use repeated-start **and spin in a `do{}while` until
  `requestFrom` returns data** — i.e. *the vendor expects occasional NACKs
  even on working hardware* and just retries. Our code treats any NACK as
  "not touched" (fine) but our one-shot `0xFE` write in `begin()` is
  fire-and-forget — if that write lands during a NACK window, auto-sleep
  stays enabled and the chip goes dark. (Already flagged as Task-4 Minor.)
- Their 4-byte burst read uses `endTransmission(true)` — **full STOP, not
  repeated start** — before `requestFrom`. Our 6-byte burst uses repeated
  start. Supports ledger hypothesis #3.
- Read layout matches ours: `0x01` gesture, `0x02` FingerNum, `0x03/0x04`
  X (12-bit), `0x05/0x06` Y.

Second working reference — `cnd-micropython/bin/touchtest.py`: SoftI2C 400 kHz,
**no RST pulse, no INT wiggle**, just `0xFE=0xFF` then 6-byte reads at `0x01`
in a loop. So the chip *can* respond without the reset dance right after
power-on; the anti-sleep write is the one thing both working references do.

## ⚠️ Touch coordinate space — our transform assumption is probably wrong

The factory capacitive demo runs the display at **`setRotation(0)`, 240 wide ×
320 tall**, and feeds CST820 raw x/y **straight into LVGL with no transform**.
So the CST820's raw space is **portrait 240×320** (x∈[0,239], y∈[0,319]),
matching the *panel*, not the display silicon.

`TouchService::begin()` currently assumes raw is landscape 320×240 with
`swap_xy=true, mirror_x=true, mirror_y=true` (copied from the display's
rotation-7 fix). The display's landscape-native *controller* quirk does not
apply to the touch chip. Expect the corner test to demand something like
`raw_w=240, raw_h=320, swap_xy=false` (mirrors TBD by test — our portrait is
rotation 7, which includes mirrors relative to rotation 0). **Fix the NACKs
first, then let the four-corner serial test decide the flags** — that's what
it's for. `lib/touch_transform` itself doesn't need changes, just different
config values.

## Updated debugging playbook (merged into `.superpowers/sdd/progress.md`)

1. **Replicate vendor `begin()` verbatim** as one experiment: 100 kHz, INT
   wiggle (OUTPUT high 1 ms/low 1 ms) before RST pulse, `0xFE=0xFF`, and
   verify the write ACKed (retry until it does).
2. Capture serial **while a finger is held down** (`python3
   .superpowers/sdd/serial_capture.py --seconds 15`) — still the key
   discriminator between sleep-gating and dead-bus.
3. If still dead: full I²C bus scan → distinguishes asleep-chip (nothing ACKs
   ever) from wrong-variant/hardware (also check board for the TP-C FPC ribbon
   near the panel, and that it's seated).
4. Hardware sanity fallback: flash the factory image —
   `esptool.py --port /dev/ttyUSB0 write_flash 0x0 "reference/2.4inch_ESP32-2432S024/8-Burn operation/Burn files/Factory_samples_Capacitive_touch.bin"`
   (it's a full-flash image per the burn instructions; our firmware reflashes
   over it with `pio run -t upload` afterwards). Factory touch working ⇒ 100%
   our code; not working ⇒ hardware/FPC.
5. After first successful read: re-check the coordinate-space section above
   before trusting the corner test's PASS/FAIL semantics.
