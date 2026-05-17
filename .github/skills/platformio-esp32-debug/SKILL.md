---
name: platformio-esp32-debug
description: >
  Guide for building, flashing, and debugging PlatformIO ESP32 firmware for
  the TokenDisplay project. Use this when asked to fix build errors, flash
  issues, serial monitor problems, or library dependency conflicts.
---

## Project Context

- **Board**: ESP32-WROOM-32 (`esp32dev`)
- **Framework**: Arduino, via PlatformIO
- **Display**: ST7789 1.9" TFT (170×320) driven by TFT_eSPI
- **Key files**: `platformio.ini`, `src/main.cpp`, `include/config.h`, `include/DisplayUI.h`, `include/OpenRouterAPI.h`
- **Secrets file**: `include/config.h` — never committed, copied from `config.h.example`

---

## Build & Flash

```bash
# Compile only
pio run -e esp32dev

# Compile + flash
pio run -e esp32dev --target upload

# Open serial monitor
pio device monitor -p COM6 -b 115200 --filter esp32_exception_decoder
```

Upload speed is set to **460 800 bps** in `platformio.ini`. If upload fails:
1. Hold BOOT button on the ESP32, press EN, then release BOOT.
2. Lower `upload_speed` to `115200` temporarily.
3. Check `upload_port = COM6` matches the actual port (`pio device list`).

---

## Common Build Errors

### `config.h: No such file or directory`
```
include/config.h not found
```
**Fix**: `cp include/config.h.example include/config.h` then fill in real values.

### TFT_eSPI — wrong pin / driver macro
If the screen is blank or shows garbage, verify these `build_flags` in `platformio.ini`:
```
-DST7789_DRIVER=1
-DTFT_WIDTH=170   ; physical width (portrait)
-DTFT_HEIGHT=320  ; physical height (portrait)
-DTFT_MOSI=23  -DTFT_SCLK=18  -DTFT_CS=15
-DTFT_DC=2     -DTFT_RST=4    -DTFT_BL=32
```
The display runs in **landscape** via `tft.setRotation(1)`, giving a logical 320×170 canvas.

### Library version conflict
Lock versions in `platformio.ini`:
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

---

## Stability Rules

1. **Never block in `loop()`** — use `millis()` timing, not `delay()` > 100 ms.
2. **WiFi reconnect** is already handled; do not add a second reconnect path.
3. **HTTP calls are synchronous** — always call inside `updateTokenData()`, never from an ISR or timer callback.
4. **Heap check** — add `Serial.printf("Free heap: %d\n", ESP.getFreeHeap())` if the device reboots unexpectedly.
