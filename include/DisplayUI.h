#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <TFT_eSPI.h>
#include <SPI.h>
#include "config.h"
#include "OpenRouterAPI.h"
#include "RelayAPI.h"

// ── Layout (landscape 320 × 170) ─────────────────────────
#define HEADER_H   28
#define FOOTER_H   22
#define BAR_H      14
#define ACCENT_W    4

// ── Palette (RGB565) ──────────────────────────────────────
#define COL_HEADER_BG  0x4A49
#define COL_LABEL      TFT_DARKGREY
#define COL_MUTED      0x39E7
#define COL_DIVIDER    0x2104
#define COL_BAR_TRACK  0x2965

// ── Colour helpers ────────────────────────────────────────
static inline uint16_t healthColor(float pctUsed) {
    if (pctUsed < 0.5f) return TFT_GREEN;
    if (pctUsed < 0.8f) return TFT_ORANGE;
    return TFT_RED;
}
static inline uint16_t remainColor(float pctLeft) {
    if (pctLeft > 0.5f) return TFT_GREEN;
    if (pctLeft > 0.2f) return TFT_ORANGE;
    return TFT_RED;
}
// Returns a very dark tinted background matching the health color
static inline uint16_t headerBgColor(float pctUsed) {
    if (pctUsed < 0.5f) return 0x0246;  // dark green tint
    if (pctUsed < 0.8f) return 0x4980;  // dark amber tint
    return 0x6082;                        // dark red tint
}

static inline uint16_t lighten565(uint16_t c) {
    uint8_t r = min(31, ((c >> 11) & 0x1F) + 7);
    uint8_t g = min(63, ((c >>  5) & 0x3F) + 15);
    uint8_t b = min(31, ( c        & 0x1F) + 7);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

// Format integer → compact string: 60000→"60K", 9500→"9.5K", 99→"99"
static inline String fmtK(int n) {
    if (n < 0) n = 0;
    if (n >= 1000) {
        int k   = n / 1000;
        int dec = (n % 1000) / 100;
        return dec ? String(k) + "." + String(dec) + "K" : String(k) + "K";
    }
    return String(n);
}


class DisplayUI {
private:
    TFT_eSPI*           tft;
    int                 selectedKeyIndex;
    int                 numKeys;
    int                 _page;       // 0 = API key page, 1 = Relay page
    bool                needsRedraw;
    TokenData           currentData;
    RelayData           _relayData;
    const APIKeyConfig* keyConfigs;

    // ── pctUsed — works for both modes ────────────────────
    float pctUsed() const {
        if (currentData.isAnthropicMode) {
            // Worst-case remaining across all active rate-limit metrics
            float worst = 1.0f;
            if (currentData.tok_limit > 0)
                worst = min(worst, (float)currentData.tok_remaining / currentData.tok_limit);
            if (currentData.req_limit > 0)
                worst = min(worst, (float)currentData.req_remaining / currentData.req_limit);
            if (currentData.in_limit > 0)
                worst = min(worst, (float)currentData.in_remaining / currentData.in_limit);
            if (currentData.out_limit > 0)
                worst = min(worst, (float)currentData.out_remaining / currentData.out_limit);
            return constrain(1.0f - worst, 0.0f, 1.0f);
        }
        if (!currentData.success || currentData.limit <= 0) return 0.0f;
        float p = currentData.usage / currentData.limit;
        return constrain(p, 0.0f, 1.0f);
    }

    // ── Shared progress-bar helper ─────────────────────────
    void drawBar(int x, int y, int w, int h, float pct, uint16_t col) {
        pct = constrain(pct, 0.0f, 1.0f);
        int fillW = (int)(pct * w);
        tft->fillRoundRect(x, y, w, h, 3, COL_BAR_TRACK);
        tft->drawFastHLine(x + 3, y + 1, w - 6, COL_DIVIDER);
        if (fillW > 0) {
            tft->fillRoundRect(x, y, fillW, h, 3, col);
            if (fillW > 6)
                tft->drawFastHLine(x + 3, y + 1, fillW - 6, lighten565(col));
        }
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(1);
        bool onFill = (w / 2 < fillW);
        tft->setTextColor(TFT_WHITE, onFill ? col : COL_BAR_TRACK);
        tft->drawString(String((int)(pct * 100)) + "%", x + w / 2, y + h / 2);
    }

    // ── Header ─────────────────────────────────────────────
    void drawHeader() {
        uint16_t accent = healthColor(pctUsed());
        uint16_t bgCol  = headerBgColor(pctUsed());
        tft->fillRect(0, 0, SCREEN_WIDTH, HEADER_H, bgCol);

        tft->fillRect(0, 0, ACCENT_W, HEADER_H, accent);
        tft->fillRect(ACCENT_W, 0, 1, HEADER_H, COL_DIVIDER);

        String keyStr = (keyConfigs != nullptr)
            ? String(keyConfigs[selectedKeyIndex].name)
            : ("Key " + String(selectedKeyIndex + 1));

        const int cy = HEADER_H / 2;
        int x = ACCENT_W + 8;

        tft->setTextSize(2);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(accent, bgCol);
        tft->drawString("Token", x, cy);
        x += tft->textWidth("Token");
        tft->setTextColor(TFT_WHITE, bgCol);
        tft->drawString(" \xB7 " + keyStr, x, cy);

        tft->setTextDatum(MR_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(TFT_LIGHTGREY, bgCol);
        tft->drawString(String(selectedKeyIndex + 1) + "/" + String(numKeys),
                        SCREEN_WIDTH - 4, cy);

        tft->drawFastHLine(0, HEADER_H - 1, SCREEN_WIDTH, COL_DIVIDER);
    }

    // ══════════════════════════════════════════════════════
    //  ANTHROPIC: Rate-limit display
    // ══════════════════════════════════════════════════════
    void drawRateLimitUI() {
        const int CY = HEADER_H;
        const int CW = SCREEN_WIDTH;
        const int CH = SCREEN_HEIGHT - HEADER_H - FOOTER_H;   // 120px
        tft->fillRect(0, CY, CW, CH, BACKGROUND_COLOR);

        if (!currentData.success) {
            tft->setTextDatum(MC_DATUM);
            tft->setTextColor(TFT_RED, BACKGROUND_COLOR);
            tft->setTextSize(2);
            tft->drawString("API Error", CW / 2, CY + CH / 2 - 10);
            tft->setTextSize(1);
            tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
            tft->drawString(currentData.error.substring(0, 40),
                            CW / 2, CY + CH / 2 + 10);
            return;
        }

        // ── Layout constants ─────────────────────────────
        // Section 1  y=28..89  (62px) — Tokens | Requests
        // Divider    y=90               (1px)
        // Section 2  y=91..147 (57px) — Input  | Output
        const int DIV_X   = 162;              // vertical mid divider
        const int L_X     = ACCENT_W + 8;     // 12  — left col start
        const int R_X     = DIV_X + 8;        // 170 — right col start
        const int L_BAR_W = DIV_X - L_X - 4; // ~146
        const int R_BAR_W = CW - R_X - 6;    // ~144

        // ── Section 1 — Tokens (left) ────────────────────
        const int S1 = CY;  // y=28
        float tokPct = (currentData.tok_limit > 0)
                     ? (float)currentData.tok_remaining / currentData.tok_limit : 1.0f;
        uint16_t tokCol = remainColor(tokPct);

        tft->setTextDatum(ML_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(tokCol, BACKGROUND_COLOR);
        tft->drawString("TOKENS / MIN", L_X, S1 + 8);

        int      tokSz   = (tokPct < 0.2f) ? 3 : 2;
        int      tokValY = S1 + (tokSz == 3 ? 22 : 24);
        int      tokBarY = S1 + (tokSz == 3 ? 40 : 36);
        tft->setTextSize(tokSz);
        tft->setTextColor(tokCol, BACKGROUND_COLOR);
        tft->drawString(fmtK(currentData.tok_remaining), L_X, tokValY);

        drawBar(L_X, tokBarY, L_BAR_W, BAR_H, tokPct, tokCol);

        if (currentData.reset_in.length()) {
            tft->setTextDatum(ML_DATUM);
            tft->setTextSize(1);
            tft->setTextColor(tokCol, BACKGROUND_COLOR);
            tft->drawString("Reset " + currentData.reset_in, L_X, tokBarY + BAR_H + 4);
        }

        // ── Section 1 — Requests (right) ─────────────────
        float reqPct = (currentData.req_limit > 0)
                     ? (float)currentData.req_remaining / currentData.req_limit : 1.0f;
        uint16_t reqCol = remainColor(reqPct);

        tft->setTextDatum(ML_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(reqCol, BACKGROUND_COLOR);
        tft->drawString("REQUESTS / MIN", R_X, S1 + 8);

        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(reqCol, BACKGROUND_COLOR);
        tft->drawString("/ " + String(currentData.req_limit), CW - 6, S1 + 8);

        tft->setTextDatum(ML_DATUM);
        tft->setTextSize(3);
        tft->setTextColor(reqCol, BACKGROUND_COLOR);
        tft->drawString(String(currentData.req_remaining), R_X, S1 + 26);

        drawBar(R_X, S1 + 42, R_BAR_W, BAR_H, reqPct, reqCol);

        // ── Horizontal divider ────────────────────────────
        tft->drawFastHLine(L_X, S1 + 63, CW - L_X * 2, COL_DIVIDER);

        // ── Vertical divider (both sections) ─────────────
        tft->drawFastVLine(DIV_X, S1 + 4, 120 - 8, COL_DIVIDER);

        // ── Section 2 — Input (left) ──────────────────────
        const int S2 = CY + 65;  // y=93
        float inPct = (currentData.in_limit > 0)
                    ? (float)currentData.in_remaining / currentData.in_limit : 1.0f;
        uint16_t inCol = remainColor(inPct);

        tft->setTextDatum(ML_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(inCol, BACKGROUND_COLOR);
        tft->drawString("INPUT TOKENS", L_X, S2 + 6);

        tft->setTextSize(2);
        tft->setTextColor(inCol, BACKGROUND_COLOR);
        tft->drawString(fmtK(currentData.in_remaining), L_X, S2 + 19);

        drawBar(L_X, S2 + 29, L_BAR_W, 10, inPct, inCol);

        tft->setTextDatum(ML_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(inCol, BACKGROUND_COLOR);
        tft->drawString("/ " + fmtK(currentData.in_limit), L_X, S2 + 44);

        // ── Section 2 — Output (right) ────────────────────
        float outPct = (currentData.out_limit > 0)
                     ? (float)currentData.out_remaining / currentData.out_limit : 1.0f;
        uint16_t outCol = remainColor(outPct);

        tft->setTextDatum(ML_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(outCol, BACKGROUND_COLOR);
        tft->drawString("OUTPUT TOKENS", R_X, S2 + 6);

        tft->setTextSize(2);
        tft->setTextColor(outCol, BACKGROUND_COLOR);
        tft->drawString(fmtK(currentData.out_remaining), R_X, S2 + 19);

        drawBar(R_X, S2 + 29, R_BAR_W, 10, outPct, outCol);

        tft->setTextDatum(ML_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(outCol, BACKGROUND_COLOR);
        tft->drawString("/ " + fmtK(currentData.out_limit), R_X, S2 + 44);
    }

    // ══════════════════════════════════════════════════════
    //  OPENROUTER: Credits display (unchanged)
    // ══════════════════════════════════════════════════════
    void drawCredits() {
        const int y0    = HEADER_H;
        const int areaH = SCREEN_HEIGHT - HEADER_H - FOOTER_H - BAR_H - 8;
        tft->fillRect(0, y0, SCREEN_WIDTH, areaH, BACKGROUND_COLOR);

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
        float remaining = (currentData.limit > 0)
                        ? (currentData.limit - currentData.usage)
                        : currentData.credits;
        if (remaining < 0.0f) remaining = 0.0f;

        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
        tft->setTextSize(1);
        tft->drawString("CREDITS REMAINING", ACCENT_W + 6, y0 + 8);

        tft->setTextColor(col, BACKGROUND_COLOR);
        tft->setTextSize(3);
        tft->drawString("$" + String(remaining, 4), ACCENT_W + 6, y0 + 30);

        const int statsY = y0 + 64;
        const int midX   = SCREEN_WIDTH / 2;
        tft->drawFastVLine(midX, statsY - 6, 30, COL_DIVIDER);

        tft->setTextSize(1);
        tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
        tft->setTextDatum(ML_DATUM);
        tft->drawString("USED", ACCENT_W + 8, statsY);
        tft->setTextSize(2);
        tft->setTextColor(TFT_WHITE, BACKGROUND_COLOR);
        tft->drawString("$" + String(currentData.usage, 4), ACCENT_W + 8, statsY + 13);

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

    void drawProgressBar() {
        const int barY = SCREEN_HEIGHT - FOOTER_H - BAR_H - 6;
        tft->fillRect(0, barY - 4, SCREEN_WIDTH, BAR_H + 8, BACKGROUND_COLOR);
        if (!currentData.success || currentData.limit <= 0) return;

        const float    pct  = pctUsed();
        const uint16_t col  = healthColor(pct);
        const int barX  = ACCENT_W + 8;
        const int barW  = SCREEN_WIDTH - barX - 8;
        drawBar(barX, barY, barW, BAR_H, pct, col);
    }

    // ══════════════════════════════════════════════════════
    //  RELAY: Claude.ai session usage
    // ══════════════════════════════════════════════════════
    void drawRelayHeader() {
        const uint16_t accent = 0x07FF; // cyan
        const uint16_t bgCol  = 0x0230; // dark cyan tint
        tft->fillRect(0, 0, SCREEN_WIDTH, HEADER_H, bgCol);
        tft->fillRect(0, 0, ACCENT_W, HEADER_H, accent);
        tft->fillRect(ACCENT_W, 0, 1, HEADER_H, COL_DIVIDER);

        const int cy = HEADER_H / 2;
        int x = ACCENT_W + 8;

        tft->setTextSize(2);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(accent, bgCol);
        tft->drawString("Token", x, cy);
        x += tft->textWidth("Token") * 2; // size-2 width
        tft->setTextColor(TFT_WHITE, bgCol);
        tft->drawString(" \xB7 Claude.ai", x, cy);

        tft->setTextDatum(MR_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(COL_MUTED, bgCol);
        tft->drawString("relay", SCREEN_WIDTH - 4, cy);

        tft->drawFastHLine(0, HEADER_H - 1, SCREEN_WIDTH, COL_DIVIDER);
    }

    void drawRelayPage() {
        const int CY = HEADER_H;        // 28
        const int CW = SCREEN_WIDTH;    // 320
        const int CH = SCREEN_HEIGHT - HEADER_H - FOOTER_H; // 120
        const int BX = ACCENT_W + 8;   // bar/label left edge
        const int BW = CW - BX - 8;    // bar width

        tft->fillRect(0, CY, CW, CH, BACKGROUND_COLOR);

        if (!_relayData.ok) {
            tft->setTextDatum(MC_DATUM);
            tft->setTextColor(COL_MUTED, BACKGROUND_COLOR);
            tft->setTextSize(2);
            tft->drawString("Relay offline", CW / 2, CY + CH / 2 - 10);
            tft->setTextSize(1);
            tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
            String msg = _relayData.error.length()
                       ? _relayData.error
                       : "Start relay: python poc_claudeai_relay.py";
            tft->drawString(msg.substring(0, 42), CW / 2, CY + CH / 2 + 10);
            return;
        }

        // ── Session (top half) ────────────────────────────
        const int S1 = CY + 8;
        float sPct = constrain(_relayData.sessionPct / 100.0f, 0.0f, 1.0f);
        uint16_t sCol = healthColor(sPct);

        tft->setTextSize(1);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(sCol, BACKGROUND_COLOR);
        tft->drawString("SESSION  (5-hour)", BX, S1);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_MUTED, BACKGROUND_COLOR);
        tft->drawString("resets " + _relayData.sessionReset, CW - 6, S1);

        drawBar(BX, S1 + 12, BW, 18, sPct, sCol);

        // ── Divider ───────────────────────────────────────
        tft->drawFastHLine(BX, CY + 52, CW - BX - 6, COL_DIVIDER);

        // ── Weekly (bottom half) ──────────────────────────
        const int S2 = CY + 62;
        float wPct = constrain(_relayData.weeklyPct / 100.0f, 0.0f, 1.0f);
        uint16_t wCol = healthColor(wPct);

        tft->setTextSize(1);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(wCol, BACKGROUND_COLOR);
        tft->drawString("WEEKLY  (7-day)", BX, S2);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_MUTED, BACKGROUND_COLOR);
        tft->drawString("resets " + _relayData.weeklyReset, CW - 6, S2);

        drawBar(BX, S2 + 12, BW, 18, wPct, wCol);
    }

    void drawRelayFooter() {
        const int y = SCREEN_HEIGHT - FOOTER_H;
        tft->fillRect(0, y, SCREEN_WIDTH, FOOTER_H, TFT_BLACK);
        tft->drawFastHLine(0, y, SCREEN_WIDTH, COL_DIVIDER);

        tft->setTextSize(1);
        tft->setTextColor(COL_MUTED, TFT_BLACK);
        tft->fillCircle(ACCENT_W + 4, y + FOOTER_H / 2, 2, COL_MUTED);
        tft->setTextDatum(ML_DATUM);
        tft->drawString("BOOT = next", ACCENT_W + 10, y + FOOTER_H / 2);

        tft->fillCircle(SCREEN_WIDTH - 7, y + FOOTER_H / 2, 3,
                        _relayData.ok ? TFT_GREEN : TFT_RED);

        unsigned long sec = millis() / 1000;
        String upStr = (sec < 60)   ? String(sec) + "s"
                     : (sec < 3600) ? String(sec / 60) + "m"
                                    : String(sec / 3600) + "h";
        tft->setTextDatum(MR_DATUM);
        tft->drawString("up " + upStr, SCREEN_WIDTH - 14, y + FOOTER_H / 2);
    }

    // ── Footer ─────────────────────────────────────────────
    void drawFooter() {
        const int y = SCREEN_HEIGHT - FOOTER_H;
        tft->fillRect(0, y, SCREEN_WIDTH, FOOTER_H, TFT_BLACK);
        tft->drawFastHLine(0, y, SCREEN_WIDTH, COL_DIVIDER);

        tft->setTextSize(1);
        tft->setTextColor(COL_MUTED, TFT_BLACK);
        tft->fillCircle(ACCENT_W + 4, y + FOOTER_H / 2, 2, COL_MUTED);
        tft->setTextDatum(ML_DATUM);
        tft->drawString("BOOT = next key", ACCENT_W + 10, y + FOOTER_H / 2);

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
          _page(0), needsRedraw(true), keyConfigs(nullptr) {
        currentData = {};
        _relayData  = {};
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
        if (_page == 1) {
            drawRelayHeader();
            drawRelayPage();
            drawRelayFooter();
        } else {
            drawHeader();
            if (currentData.isAnthropicMode)
                drawRateLimitUI();
            else {
                drawCredits();
                drawProgressBar();
            }
            drawFooter();
        }
        needsRedraw = false;
    }

    void updateTokenDisplay(const TokenData& data) {
        if (_page == 1) return;  // ignore API updates while on relay page
        currentData = data;
        tft->fillRect(0, HEADER_H, SCREEN_WIDTH,
                      SCREEN_HEIGHT - HEADER_H - FOOTER_H, BACKGROUND_COLOR);
        drawHeader();
        if (currentData.isAnthropicMode)
            drawRateLimitUI();
        else {
            drawCredits();
            drawProgressBar();
        }
        drawFooter();
    }

    void updateRelayDisplay(const RelayData& d) {
        _relayData = d;
        if (_page == 1) {
            drawRelayPage();
            drawRelayFooter();
        }
    }

    void goToRelayPage() {
        _page = 1;
        needsRedraw = true;
    }

    bool isRelayPage() { return _page == 1; }

    int getSelectedKeyIndex() { return selectedKeyIndex; }

    // Cycle: key0 → key1 → ... → keyN-1 → relay → key0
    void nextKey() {
        if (_page == 1) {
            _page = 0;
            selectedKeyIndex = 0;
        } else {
            selectedKeyIndex++;
            if (selectedKeyIndex >= numKeys) {
                selectedKeyIndex = 0;
                _page = 1;
            }
        }
        needsRedraw = true;
    }
};

#endif // DISPLAY_UI_H
