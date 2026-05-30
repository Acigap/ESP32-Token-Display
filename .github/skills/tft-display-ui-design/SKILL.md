---
name: tft-display-ui-design
description: >
  Guide for designing clean, stable, and visually appealing UI for
  TokenDisplay across supported TFT boards. Use this when asked to improve
  layout, fix flickering, change colors, add new screens, or redesign display
  behavior for esp32dev / esp32s3-touch-lcd-1_9 / lilygo-t-display-s3 / ttgo-t-display.
---

## Canvas

| Env | Backend | Logical canvas (landscape) |
|-----|---------|-----------------------------|
| `esp32dev` | TFT_eSPI | 320 × 170 |
| `esp32s3-touch-lcd-1_9` | Arduino_GFX via `Display` wrapper | 320 × 170 |
| `lilygo-t-display-s3` | TFT_eSPI | 320 × 170 |
| `ttgo-t-display` | TFT_eSPI | 240 × 135 |

Common rules:
- Rotation is `setRotation(1)` for all boards.
- Always code against the `Display` abstraction from `include/GfxDisplay.h`.
- Use `SCREEN_WIDTH` and `SCREEN_HEIGHT` macros, not hardcoded dimensions.

---

## Layout Zones (current design)

```
┌─────────────────────────────────────┐  y=0
│           HEADER  (28 px)           │  TFT_NAVY bg, yellow label
├─────────────────────────────────────┤  y=28
│                                     │
│        CREDITS AREA (~84 px)        │  TFT_BLACK bg
│   $XX.XXXX  (font size 4 = ~32 px)  │
│   Token used / Token limit          │
│                                     │
├─────────────────────────────────────┤  y≈122
│       PROGRESS BAR  (18 px)         │
├─────────────────────────────────────┤  y≈144
│           FOOTER  (24 px)           │  timestamp / last-update
└─────────────────────────────────────┘  y=170 or y=135
```

Constants live in `include/DisplayUI.h`:
```cpp
#if SCREEN_HEIGHT < 160
  #define HEADER_H 22
  #define FOOTER_H 14
  #define BAR_H    11
#else
  #define HEADER_H 28
  #define FOOTER_H 22
  #define BAR_H    14
#endif
```

UI is intentionally split into two layout classes:
- Compact layout for small height (TTGO 240x135).
- Full layout for 320x170 boards.

---

## Color Palette

Theme-driven palette constants now come from `DisplayUI.h`:
- `COL_SCREEN_BG`, `COL_HEADER_BG`, `COL_TEXT_PRIMARY`
- `COL_LABEL`, `COL_MUTED`, `COL_DIVIDER`, `COL_BAR_TRACK`
- status colors: `STATUS_GREEN`, `STATUS_AMBER`, `STATUS_RED`

Themes:
- `THEME_DARK`: high contrast dark default
- `THEME_LIGHT`: white background with darker status colors for readability
- `THEME_VIVID`: dark background with status-tinted header

To keep the UI clean: **use at most 3 colors per zone**.

---

## Anti-Flicker Rules

The project redraw strategy is backend-safe (works for TFT_eSPI and Arduino_GFX).

1. **Fill before draw** — always `tft->fillRect(x, y, w, h, BG_COLOR)` the exact region before writing new text or shapes. Do not `fillScreen()` unless changing the entire screen.
2. **Stable text footprints** — use compact formatting helpers (example: `fmtK`) where value widths can jump.
3. **`needsRedraw` flag** — only call `drawUI()` when data actually changes (already implemented via `needsUpdate()`).
4. **Avoid overlapping text** — when changing font size, clear old text region first.

---

## Font Sizes (TFT_eSPI built-in)

| Size | Approx height | Use for |
|------|---------------|---------|
| 1    | 8 px  | Fine labels, sub-text |
| 2    | 16 px | Secondary values, footer |
| 3    | 24 px | Medium emphasis |
| 4    | 32 px | Credit value |
| 6    | 48 px | Large single metric |

Do not rely on custom font APIs that exist in one backend only.
Use shared methods already exposed via `Display` wrapper (`setTextSize`, `drawString`, `textWidth`, datum alignment).

---

## Progress Bar Pattern

```cpp
// Track (background rail)
tft->fillRoundRect(barX, barY, barW, BAR_H, 4, TFT_DARKGREY);

// Fill (usage %)
uint16_t color = (pct < 0.5f) ? TFT_GREEN : (pct < 0.8f) ? TFT_YELLOW : TFT_RED;
tft->fillRoundRect(barX, barY, fillW, BAR_H, 4, color);
```

Always clamp `pct` to `[0.0, 1.0]` before computing `fillW`.

---

## Adding a New Screen

1. Add a private `void drawXxx()` method in `DisplayUI.h`.
2. Call it from `drawUI()` based on a state enum or flag.
3. Clear only the affected zone with `fillRect`; do not `fillScreen`.
4. Use `MC_DATUM` (middle-center) for centered text, `ML_DATUM` for left-aligned.
5. Validate on both canvas classes: 320x170 and 240x135.

---

## Checklist Before Committing UI Changes

- [ ] No `fillScreen` inside `loop()` or frequent calls
- [ ] All text regions pre-cleared with `fillRect` matching the font size
- [ ] Colors use current theme constants (not hardcoded legacy palette values)
- [ ] Tested with both success and error states (`currentData.success = false`)
- [ ] Tested at 3+ API key indices (header label cycles correctly)
- [ ] Tested at least one touch board and one button board for screen cycling behavior
