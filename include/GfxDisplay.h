#ifndef GFX_DISPLAY_H
#define GFX_DISPLAY_H

#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Display abstraction
//
//  Two backends, selected at build time:
//
//   • BOARD_S3_TOUCH_LCD  → Arduino_GFX  (Waveshare ESP32-S3-Touch-LCD-1.9)
//     TFT_eSPI 2.5.43 panics on Arduino-ESP32 v3.x (the pioarduino stack),
//     so the S3 board uses Arduino_GFX instead. This class wraps it to mimic
//     the subset of TFT_eSPI methods that DisplayUI relies on
//     (datum-based drawString, textWidth, fillRoundRect, …).
//
//   • everything else     → plain TFT_eSPI  (esp32dev, ttgo-t-display, …)
//     Display is just a typedef so existing code paths are unchanged.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef BOARD_S3_TOUCH_LCD

#include <Arduino_GFX_Library.h>

// ── Colours (RGB565) ────────────────────────────────────────────────────────
#ifndef TFT_BLACK
#define TFT_BLACK       0x0000
#define TFT_WHITE       0xFFFF
#define TFT_RED         0xF800
#define TFT_GREEN       0x07E0
#define TFT_BLUE        0x001F
#define TFT_YELLOW      0xFFE0
#define TFT_ORANGE      0xFD20
#define TFT_CYAN        0x07FF
#define TFT_MAGENTA     0xF81F
#define TFT_LIGHTGREY   0xC618
#define TFT_DARKGREY    0x7BEF
#endif

// ── Text datum (matches TFT_eSPI numbering: H = col%3, V = col/3) ──────────
#ifndef TL_DATUM
#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define MC_DATUM 4
#define MR_DATUM 5
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8
#endif

class Display {
public:
    Arduino_GFX* gfx;
    uint8_t      _datum = TL_DATUM;

    explicit Display(Arduino_GFX* g) : gfx(g) {}

    bool init()                                       { return gfx->begin(); }
    void setRotation(uint8_t r)                       { gfx->setRotation(r); }
    void fillScreen(uint16_t c)                       { gfx->fillScreen(c); }
    int width()                                        { return gfx->width(); }
    int height()                                       { return gfx->height(); }

    void fillRect(int x, int y, int w, int h, uint16_t c)               { gfx->fillRect(x, y, w, h, c); }
    void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c)   { gfx->fillRoundRect(x, y, w, h, r, c); }
    void fillCircle(int x, int y, int r, uint16_t c)                    { gfx->fillCircle(x, y, r, c); }
    void drawFastHLine(int x, int y, int w, uint16_t c)                 { gfx->drawFastHLine(x, y, w, c); }
    void drawFastVLine(int x, int y, int h, uint16_t c)                 { gfx->drawFastVLine(x, y, h, c); }

    void setTextSize(uint8_t s)                       { gfx->setTextSize(s); }
    void setTextColor(uint16_t f)                     { gfx->setTextColor(f); }
    void setTextColor(uint16_t f, uint16_t b)         { gfx->setTextColor(f, b); }
    void setTextDatum(uint8_t d)                      { _datum = d; }

    int textWidth(const char* s) {
        int16_t  x1, y1;
        uint16_t w, h;
        gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
        return (int)w;
    }
    int textWidth(const String& s)                    { return textWidth(s.c_str()); }

    void drawString(const char* s, int x, int y) {
        int16_t  x1, y1;
        uint16_t w, h;
        gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);

        int cx = x, cy = y;
        switch (_datum % 3) {                // horizontal: 0=L 1=C 2=R
            case 1: cx -= (int)w / 2; break;
            case 2: cx -= (int)w;     break;
        }
        switch (_datum / 3) {                // vertical:   0=T 1=M 2=B
            case 1: cy -= (int)h / 2; break;
            case 2: cy -= (int)h;     break;
        }
        gfx->setCursor(cx, cy);
        gfx->print(s);
    }
    void drawString(const String& s, int x, int y)    { drawString(s.c_str(), x, y); }
};

#else  // ── TFT_eSPI path (default) ───────────────────────────────────────────

#include <TFT_eSPI.h>
using Display = TFT_eSPI;

#endif // BOARD_S3_TOUCH_LCD

#endif // GFX_DISPLAY_H
