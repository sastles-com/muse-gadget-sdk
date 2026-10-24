<!--
Copyright (c) Meta Platforms, Inc. and affiliates.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# M5Stack AtomS3R port notes

Status: **Phase 1 backend built and running on hardware (paired
2026-10-24); Phase 2 commands (`sensors.read`, `display.text`)
implemented and built, not yet run on hardware.** This file collects the
facts gathered for the port (per `AGENTS.md` in this directory: collect the
facts first, cite where each came from, don't guess pins). The overlay is
[`sdkconfig.m5stack-atoms3r`](sdkconfig.m5stack-atoms3r).

## Board facts

| Item | Value | Source |
|---|---|---|
| Module | ESP32-S3-PICO-1-N8R8: ESP32-S3, 8 MB flash, 8 MB octal PSRAM | M5Stack AtomS3R product docs |
| USB | Native USB-Serial-JTAG (`usbmodem` / `ttyACM`), Espressif `303a:1001` | ESP32-S3 (same as other S3 boards) |
| Display | 0.85" 128×128 IPS, SPI3, **two panel revisions** (below) | M5GFX autodetect |
| Button | Front/screen button on **GPIO41** (input; M5Unified sets it as the button input for AtomS3R) | `M5Unified.inl` |
| IMU | BMI270 6-axis IMU on the internal I2C bus (Phase 2 `sensors.read`); M5Unified's BMI270 class defaults to I2C address 0x69 and falls back to 0x68 | M5Unified `src/utility/imu/BMI270_Class.hpp`, `src/utility/IMU_Class.inl` |
| IR | Emitter/receiver around GPIO1/GPIO2 (port table; unused) | `M5Unified.inl` board table |
| Mic / speaker | **None** (the AtomS3R-CAM kit's Voice Base adds them; the bare unit has neither) | M5Stack docs |

## LCD pin map (from M5GFX `src/M5GFX.cpp`, AtomS3R autodetect)

| Signal | GPIO |
|---|---|
| MOSI | 21 |
| SCLK | 15 |
| DC | 42 |
| CS | 14 |
| RST | 48 |
| MISO | — (not wired) |

SPI host `SPI3_HOST`, mode 0, 3-wire, 40 MHz write / 16 MHz read.

### Panel revisions — must auto-detect

M5GFX reads the panel ID over SPI (CS GPIO14) and branches:

- **GC9107**: ID `0x079100`. Panel 128×128, `offset_y = 32`, not readable
  in some batches (a slow 100 kHz re-probe is needed before giving up).
- **ST7735S**: ID `0x7683` or `0x897C`. Panel 128×128, memory height 132,
  `offset_x = 2`, `offset_y = 1`, `invert = true`.

A port that hard-codes one panel will show garbage on the other revision,
so `lcd_panel_init()` has to do the same ID probe.

### Backlight is not a GPIO

There is no backlight pin. `Light_M5StackAtomS3R` in M5GFX drives a chip at
I2C address **0x30** on the internal bus (**SDA GPIO45, SCL GPIO0** — the
same bus as the IMU; note SCL is the ESP32-S3 BOOT strapping pin):

- init: reg `0x00` ← `0x40`, reg `0x08` ← `0x01`, reg `0x70` ← `0x00`
- brightness: reg `0x0E` ← value

So `led_hw_init()` / the panel block in `main/led_status.c` needs an I2C
write sequence, not the usual "drive `LCD_PIN_BL` high".

## Panel driver availability

Neither GC9107 nor ST7735 appears anywhere in `main/` or the component
manifests today. Check `$IDF_PATH/components/esp_lcd` of ESP-IDF v6.0.1;
GC9107 is published as the `espressif/esp_lcd_gc9107` registry component.
Per step 5 of `AGENTS.md` in this directory, add it to
`main/idf_component.yml` and to `GADGET_REQUIRES` in `main/CMakeLists.txt`
under an `if()` for this backend.

Geometry for the status UI: the animation is 56×59 cells, so
`LCD_ANIM_SCALE=2` gives 112 ≤ 128. `LCD_H_RES=LCD_V_RES=128`.

## Remaining work (following the recipe in `AGENTS.md`)

1. `main/Kconfig.projbuild`: add `HOMEHUB_LED_BACKEND_M5STACK_ATOMS3R` to
   the `HOMEHUB_LED_BACKEND` choice and a `default y if …` line to
   `HOMEHUB_DISPLAY`.
2. `main/led_status.c`: add the AtomS3R block (pins/geometry above), the
   panel-ID probe with both init paths, the I2C backlight, and the four
   `#if` split points (includes, `lcd_draw()`/`lcd_from_be()`,
   `lcd_panel_init()`/`lcd_panel_on()`, pixel copy). SPI panels take
   big-endian RGB565 like the ideaspark.
3. `tools/board.sh`: add an `m5stack-atoms3r` case (target esp32s3,
   overlay appended to `DEFAULTS`, `PORTS="/dev/cu.usbmodem* /dev/ttyACM*"`),
   plus the usage comment line.
4. `devices/README.md`: Supported devices row, Features column, Build row.
5. `../AGENTS.md` and `../README.md` board tables.
6. Build from scratch (`tools/board.sh m5stack-atoms3r build`), run the
   overlay-applied check from step 8 of `AGENTS.md`, check the
   `check_sizes` free space, run the host tests, then flash and capture
   the boot log (`link.main: Muse Gadget starting`, orange breathing,
   BLE scan shows `MuseGadget-Disp-XXXXXX`).

## Open questions

- Flash mode (DIO vs QIO) and frequency for the PICO-1 in-package flash —
  verify against a factory firmware dump or M5's published flash settings.
- Which panel revision the target units have (the ID probe makes this
  non-blocking, but the first bring-up should log the detected ID).
- Battery/power: the bare AtomS3R is USB-powered; `read_power` does not
  apply to the status-screen profile.
