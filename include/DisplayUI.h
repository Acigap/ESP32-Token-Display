#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <TFT_eSPI.h>
#include <SPI.h>
#include "config.h"
#include "OpenRouterAPI.h"

// ── Layout constants (landscape 320 × 170) ───────────────
#define HEADER_H   28
#define FOOTER_H   22
#define BAR_H      14
#define ACCENT_W    4   // left-edge coloured strip in header

// ── Extended palette (RGB565) ─────────────────────────────
#define COL_HEADER_BG  0x4A49     // dark grey header
#define COL_LABEL      TFT_DARKGREY  // section sub-labels
#define COL_MUTED      0x39E7     // dimmer grey — footer text
#define COL_DIVIDER    0x2104     // near-black thin rules
#define COL_BAR_TRACK  0x2965     // dark grey bar rail

// ── Health colour: green when plenty, orange mid, red low ─
static inline uint16_t healthColor(float pct) {
    if (pct < 0.5f) return TFT_GREEN;
    if (pct < 0.8f) return TFT_ORANGE;
    return TFT_RED;
}

// ── Lighten a RGB565 colour ~25 % (per-channel, clamped) ─
static inline uint16_t lighten565(uint16_t c) {
    uint8_t r = min(31, ((c >> 11) & 0x1F) + 7);
    uint8_t g = min(63, ((c >>  5) & 0x3F) + 15);
    uint8_t b = min(31, ( c        & 0x1F) + 7);
    return (uint16_t)((r << 11) | (g << 5) | b);
}



class DisplayUI {
private:
    TFT_eSPI*           tft;
    int                 selectedKeyIndex;
    int                 numKeys;
    bool                needsRedraw;
    TokenData           currentData;
    const APIKeyConfig* keyConfigs;

    // Fraction of budget consumed (0–1, clamped)
    float pctUsed() const {
        if (!currentData.success || currentData.limit <= 0) return 0.0f;
        float p = currentData.usage / currentData.limit;
        return (p < 0.0f) ? 0.0f : (p > 1.0f) ? 1.0f : p;
    }

    // ── Header ─────────────────────────────────────────────
    void drawHeader() {
        tft->fillRect(0, 0, SCREEN_WIDTH, HEADER_H, COL_HEADER_BG);

        // Left accent strip — colour = credit health
        uint16_t accent = healthColor(pctUsed());
        tft->fillRect(0, 0, ACCENT_W, HEADER_H, accent);
        tft->fillRect(ACCENT_W, 0, 1, HEADER_H, COL_DIVIDER);

        String keyStr = (keyConfigs != nullptr)
            ? String(keyConfigs[selectedKeyIndex].name)
            : ("Key " + String(selectedKeyIndex + 1));

        const int cy = HEADER_H / 2;
        int x = ACCENT_W + 8;

        tft->setTextSize(2);
        tft->setTextDatum(ML_DATUM);

        // "Token" — accent (health) colour
        tft->setTextColor(accent, COL_HEADER_BG);
        tft->drawString("Token", x, cy);
        x += tft->textWidth("Token");

        // " · keyname" — white
        tft->setTextColor(TFT_WHITE, COL_HEADER_BG);
        tft->drawString(" \xB7 " + keyStr, x, cy);

        // Counter — right, size 1
        tft->setTextDatum(MR_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(TFT_LIGHTGREY, COL_HEADER_BG);
        tft->drawString(String(selectedKeyIndex + 1) + "/" + String(numKeys),
                        SCREEN_WIDTH - 4, cy);

        tft->drawFastHLine(0, HEADER_H - 1, SCREEN_WIDTH, COL_DIVIDER);
    }

    // ── Credits + stats area ───────────────────────────────
    void drawCredits() {
        const int y0    = HEADER_H;
        const int areaH = SCREEN_HEIGHT - HEADER_H - FOOTER_H - BAR_H - 8;
        tft->fillRect(0, y0, SCREEN_WIDTH, areaH, BACKGROUND_COLOR);

        // ── Error state ───────────────────────────────────
        if (!currentData.success) {
            tft->setTextDatum(MC_DATUM);
            tft->setTextColor(TFT_RED, BACKGROUND_COLOR);
            tft->setTextSize(2);
            tft->drawString("Fetch Error", SCREEN_WIDTH / 2, y0 + 28);
            tft->setTextSize(1);
            tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
            tft->drawString(currentData.error.substring(0, 38),
                            SCREEN_WIDTH / 2, y0 + 52);
            return;
        }

        float    pct  = pctUsed();
        uint16_t col  = healthColor(pct);

        // Remaining = limit − usage (or raw credits when no limit)
        float remaining = (currentData.limit > 0)
                        ? (currentData.limit - currentData.usage)
                        : currentData.credits;
        if (remaining < 0.0f) remaining = 0.0f;

        // ── "CREDITS REMAINING" label ─────────────────────
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
        tft->setTextSize(1);
        tft->drawString("CREDITS REMAINING", ACCENT_W + 6, y0 + 8);

        // ── Large value (health colour) ───────────────────
        tft->setTextColor(col, BACKGROUND_COLOR);
        tft->setTextSize(3);
        tft->drawString("$" + String(remaining, 4), ACCENT_W + 6, y0 + 30);

        // ── Two-column stats row ──────────────────────────
        const int statsY = y0 + 64;
        const int midX   = SCREEN_WIDTH / 2;

        // Thin vertical divider between columns
        tft->drawFastVLine(midX, statsY - 6, 30, COL_DIVIDER);

        // USED — left
        tft->setTextSize(1);
        tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
        tft->setTextDatum(ML_DATUM);
        tft->drawString("USED", ACCENT_W + 8, statsY);
        tft->setTextSize(2);
        tft->setTextColor(TFT_WHITE, BACKGROUND_COLOR);
        tft->drawString("$" + String(currentData.usage, 4), ACCENT_W + 8, statsY + 13);

        // LIMIT — right
        tft->setTextSize(1);
        tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
        tft->drawString("LIMIT", midX + 8, statsY);
        tft->setTextSize(2);
        tft->setTextColor(TFT_WHITE, BACKGROUND_COLOR);
        if (currentData.limit > 0)
            tft->drawString("$" + String(currentData.limit, 2), midX + 8, statsY + 13);
        else
            tft->drawString("No limit", midX + 8, statsY + 13);
    }

    // ── Progress bar ───────────────────────────────────────
    void drawProgressBar() {
        const int barY = SCREEN_HEIGHT - FOOTER_H - BAR_H - 6;
        tft->fillRect(0, barY - 4, SCREEN_WIDTH, BAR_H + 8, BACKGROUND_COLOR);

        if (!currentData.success || currentData.limit <= 0) return;

        const float    pct   = pctUsed();
        const uint16_t col   = healthColor(pct);
        const int      barX  = ACCENT_W + 8;
        const int      barW  = SCREEN_WIDTH - barX - 8;
        const int      fillW = (int)(pct * barW);

        // Rail
        tft->fillRoundRect(barX, barY, barW, BAR_H, 4, COL_BAR_TRACK);
        // Inner top shadow on rail (sunken look)
        tft->drawFastHLine(barX + 4, barY + 1, barW - 8, COL_DIVIDER);

        // Coloured fill
        if (fillW > 0) {
            tft->fillRoundRect(barX, barY, fillW, BAR_H, 4, col);
            // Shimmer: 1 px lighter line at top of fill
            if (fillW > 8)
                tft->drawFastHLine(barX + 4, barY + 1, fillW - 8, lighten565(col));
        }

        // Percentage — centred on bar, white
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(1);
        bool textOnFill = (barW / 2 < fillW);
        tft->setTextColor(TFT_WHITE, textOnFill ? col : COL_BAR_TRACK);
        tft->drawString(String((int)(pct * 100)) + "%",
                        barX + barW / 2, barY + BAR_H / 2);
    }

    // ── Footer ─────────────────────────────────────────────
    void drawFooter() {
        const int y = SCREEN_HEIGHT - FOOTER_H;
        tft->fillRect(0, y, SCREEN_WIDTH, FOOTER_H, TFT_BLACK);
        tft->drawFastHLine(0, y, SCREEN_WIDTH, COL_DIVIDER);

        tft->setTextSize(1);
        tft->setTextColor(COL_MUTED, TFT_BLACK);

        // Left: bullet dot + hint
        tft->fillCircle(ACCENT_W + 4, y + FOOTER_H / 2, 2, COL_MUTED);
        tft->setTextDatum(ML_DATUM);
        tft->drawString("BOOT = next key", ACCENT_W + 10, y + FOOTER_H / 2);

        // Right: status dot (green=OK, red=error) + uptime
        tft->fillCircle(SCREEN_WIDTH - 7, y + FOOTER_H / 2, 3,
                        currentData.success ? TFT_GREEN : TFT_RED);

        unsigned long sec = millis() / 1000;
        String upStr = (sec < 60)   ? String(sec) + "s"
                     : (sec < 3600) ? String(sec / 60) + "m"
                                    : String(sec / 3600) + "h";
        tft->setTextDatum(MR_DATUM);
        tft->drawString("up " + upStr, SCREEN_WIDTH - 14, y + FOOTER_H / 2);
    }

public:
    DisplayUI(TFT_eSPI* display, int numApiKeys)
        : tft(display), numKeys(numApiKeys), selectedKeyIndex(0),
          needsRedraw(true), keyConfigs(nullptr) {
        currentData = {};
    }

    void setAPIKeyLabels(const APIKeyConfig* keys) { keyConfigs = keys; }

    void init() {
        tft->setRotation(1);
        tft->fillScreen(BACKGROUND_COLOR);
        drawUI();
    }

    bool needsUpdate()   { return needsRedraw; }
    void requestRedraw() { needsRedraw = true; }

    void drawUI() {
        drawHeader();
        drawCredits();
        drawProgressBar();
        drawFooter();
        needsRedraw = false;
    }

    void updateTokenDisplay(const TokenData& data) {
        currentData = data;
        drawCredits();
        drawProgressBar();
        drawFooter();
    }

    int getSelectedKeyIndex() { return selectedKeyIndex; }

    void nextKey() {
        selectedKeyIndex = (selectedKeyIndex + 1) % numKeys;
        needsRedraw = true;
    }
};

#endif // DISPLAY_UI_H
