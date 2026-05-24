---
name: platformio-esp32-debug
description: >
  Guide for building, flashing, and debugging PlatformIO firmware for
  TokenDisplay across supported boards/environments. Use this when asked to
  fix build errors, flash issues, serial monitor problems, board-specific
  display/touch behavior, or dependency conflicts.
---

## Project Context

- **Framework**: Arduino via PlatformIO
- **Primary build file**: `platformio.ini`
- **Firmware entry**: `src/main.cpp`
- **UI layer**: `include/DisplayUI.h`
- **Display abstraction**: `include/GfxDisplay.h`
- **Secrets file**: `include/config.h` (never committed; copy from `include/config.h.example`)

### Supported PlatformIO environments

| Env | Board | Display backend | Input |
|-----|-------|-----------------|-------|
| `esp32dev` | ESP32-WROOM-32 class board | TFT_eSPI | BOOT button (GPIO0) |
| `esp32s3-touch-lcd-1_9` | Waveshare ESP32-S3-Touch-LCD-1.9 | Arduino_GFX | CST816 touch tap |
| `ttgo-t-display` | TTGO T-Display 1.14 | TFT_eSPI | BOOT button (GPIO0) |

---

## Build & Flash

```bash
# List available serial ports
pio device list --serial

# Compile only (choose env)
pio run -e esp32dev
pio run -e esp32s3-touch-lcd-1_9
pio run -e ttgo-t-display

# Compile + flash
pio run -e esp32dev --target upload
pio run -e esp32s3-touch-lcd-1_9 --target upload

# Open serial monitor
pio device monitor -p COM6 -b 115200 --filter esp32_exception_decoder
```

Use the correct upload procedure per board:
1. esp32dev / ttgo-t-display: if upload fails, hold BOOT, tap EN/RESET, release BOOT.
2. esp32s3-touch-lcd-1_9: usually no BOOT dance needed, but verify the selected COM and USB mode.
3. Lower `upload_speed` temporarily if the link is unstable.
4. Confirm `upload_port`/`monitor_port` belong to the selected env section.

---

## Common Build Errors

### `config.h: No such file or directory`
```
include/config.h not found
```
**Fix**: `cp include/config.h.example include/config.h` then fill in real values.

### Wrong environment selected
Symptoms:
- Compiles but hardware behavior is wrong (no touch, wrong display driver, wrong resolution)
- Upload works but display stays blank

Fix:
1. Confirm the env passed to `pio run -e ...` matches the connected board.
2. Confirm extension Device tab board selector matches the same env.

### TFT_eSPI pin/driver mismatch (esp32dev, ttgo)
If the screen is blank/garbled on TFT_eSPI boards, verify corresponding `build_flags` in the active env:
```
-DST7789_DRIVER=1
-DTFT_WIDTH=170   ; physical width (portrait)
-DTFT_HEIGHT=320  ; physical height (portrait)
-DTFT_MOSI=23  -DTFT_SCLK=18  -DTFT_CS=15
-DTFT_DC=2     -DTFT_RST=4    -DTFT_BL=32
```
The display runs in **landscape** via `tft.setRotation(1)`, giving a logical 320×170 canvas.

### ESP32-S3 touch board: display crash / black screen
Check these S3-specific requirements in `platformio.ini`:
- Env uses pioarduino platform URL (not default espressif32 release train)
- `board_build.arduino.memory_type = qio_opi`
- `-DBOARD_S3_TOUCH_LCD=1`
- Library is `moononournation/GFX Library for Arduino`

If missing, S3 can fail with PSRAM init panic or runtime display issues.

### Library version conflict
Keep per-env dependencies aligned in `platformio.ini`:
```ini
lib_deps =
    bodmer/TFT_eSPI@^2.5.43
    bblanchon/ArduinoJson@^7.0.4
```
Run `pio pkg update` to refresh, or `pio pkg install` after editing `lib_deps`.

---

## Debugging with Serial Monitor

`CORE_DEBUG_LEVEL=3` is already set, so ESP-IDF verbose logs appear.
- Exception decoder is active — stack traces are human-readable.
- Add `Serial.printf()` calls freely; they cost nothing in a dev build.

On `esp32s3-touch-lcd-1_9`, touch probe diagnostics are printed at boot:
- `CST816 probe ... ACK` means touch IC is visible on I2C.
- `no ACK` means wrong wiring/pins/build target or board mismatch.

---

## Stability Rules

1. **Never block in `loop()`** — use `millis()` timing, not `delay()` > 100 ms.
2. **WiFi reconnect** is already handled; do not add a second reconnect path.
3. **HTTP calls are synchronous** — always call inside `updateTokenData()`, never from an ISR or timer callback.
4. **Heap check** — add `Serial.printf("Free heap: %d\n", ESP.getFreeHeap())` if the device reboots unexpectedly.
5. **Input model differs by board** — S3 uses touch edge-detect; other boards use BOOT debounce.
