---
name: tft-display-ui-design
description: >
  Guide for designing clean, stable, and visually appealing UI on the
  ST7789 TFT display (320×170 landscape) using TFT_eSPI. Use this when
  asked to improve layout, fix flickering, change colors, add new screens,
  or redesign any part of the display.
---

## Canvas

| Property | Value |
|----------|-------|
| Driver   | ST7789 |
| Physical | 170 × 320 (portrait) |
| **Logical (landscape)** | **320 × 170** |
| Rotation | `tft.setRotation(1)` |
| SPI freq | 80 MHz |

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
└─────────────────────────────────────┘  y=170
```

Constants live in `include/DisplayUI.h`:
```cpp
#define HEADER_H  28
#define FOOTER_H  24
#define BAR_H     18
```

---

## Color Palette

| Role | Constant | Hex |
|------|----------|-----|
| Background | `TFT_BLACK` | `#000000` |
| Header bg | `TFT_NAVY` | `#000080` |
| Primary text | `TFT_WHITE` | `#FFFFFF` |
| Accent / key name | `TFT_YELLOW` | `#FFFF00` |
| Credit value | `TFT_GREEN` | `#00FF00` |
| Muted labels | `TFT_DARKGREY` / `TFT_LIGHTGREY` | — |
| Dividers | `TFT_DARKGREY` | — |
| Error state | `TFT_RED` | `#FF0000` |

To keep the UI clean: **use at most 3 colors per zone**.

---

## Anti-Flicker Rules

TFT_eSPI has no double-buffer — overdrawing causes flicker.

1. **Fill before draw** — always `tft->fillRect(x, y, w, h, BG_COLOR)` the exact region before writing new text or shapes. Do not `fillScreen()` unless changing the entire screen.
2. **Fixed-width numbers** — use `String(value, 4)` (4 decimal places) so the string width stays constant and the background fill can be pre-sized.
3. **`needsRedraw` flag** — only call `drawUI()` when data actually changes (already implemented via `needsUpdate()`).
4. **Avoid overlapping text** — when changing font size, always clear the old area first; a size-4 digit is wider than a size-2 digit.

---

## Font Sizes (TFT_eSPI built-in)

| Size | Approx height | Use for |
|------|---------------|---------|
| 1    | 8 px  | Fine labels, sub-text |
| 2    | 16 px | Secondary values, footer |
| 3    | 24 px | Medium emphasis |
| 4    | 32 px | Credit value |
| 6    | 48 px | Large single metric |

Smooth fonts (GFXFF) are enabled — use `tft->loadFont()` for custom TTF glyphs if sharper text is needed.

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

---

## Checklist Before Committing UI Changes

- [ ] No `fillScreen` inside `loop()` or frequent calls
- [ ] All text regions pre-cleared with `fillRect` matching the font size
- [ ] Colors match the palette table above
- [ ] Tested with both success and error states (`currentData.success = false`)
- [ ] Tested at 3+ API key indices (header label cycles correctly)
