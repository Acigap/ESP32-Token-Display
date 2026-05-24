#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include "GfxDisplay.h"
#include "config.h"
#include "OpenRouterAPI.h"
#include "RelayAPI.h"

// ── Layout — scales for 320×170 (1.9") and 240×135 (TTGO 1.14") ──
#if defined(BOARD_TTGO_T_DISPLAY)
    #define UI_COMPACT_LAYOUT 1
#elif SCREEN_HEIGHT < 160
    #define UI_COMPACT_LAYOUT 1
#else
    #define UI_COMPACT_LAYOUT 0
#endif

#if UI_COMPACT_LAYOUT
  #define HEADER_H 22
  #define FOOTER_H 14
  #define BAR_H    11
#else
  #define HEADER_H 28
  #define FOOTER_H 22
  #define BAR_H    14
#endif
#define ACCENT_W    4

// ── Theme constants ───────────────────────────────────────
// Switched at build time via the DISPLAY_THEME #define in include/config.h
// (written by the VS Code extension's 📡 Device tab).
#ifndef THEME_DARK
#define THEME_DARK   0   // pitch black bg, high-contrast white/light-grey text
#define THEME_LIGHT  1   // white bg, dark text — good in bright rooms
#define THEME_VIVID  2   // dark bg with status-tinted header strip
#endif

#ifndef DISPLAY_THEME
#define DISPLAY_THEME THEME_DARK
#endif

// ── Palette per theme (RGB565) ─────────────────────────────
#if DISPLAY_THEME == THEME_LIGHT
  #define COL_SCREEN_BG      TFT_WHITE
  #define COL_HEADER_BG      0xE73C    // very light gray
  #define COL_TEXT_PRIMARY   TFT_BLACK
  #define COL_LABEL          0x6B4D    // mid grey
  #define COL_MUTED          0x9CD3
  #define COL_DIVIDER        0xC618
  #define COL_BAR_TRACK      0xD69A
  #define THEME_VIVID_HEADER 0
#elif DISPLAY_THEME == THEME_VIVID
  #define COL_SCREEN_BG      TFT_BLACK
  #define COL_HEADER_BG      TFT_BLACK   // overridden per-status by headerBgColor()
  #define COL_TEXT_PRIMARY   TFT_WHITE
  #define COL_LABEL          TFT_DARKGREY
  #define COL_MUTED          0x39E7
  #define COL_DIVIDER        0x2104
  #define COL_BAR_TRACK      0x2965
  #define THEME_VIVID_HEADER 1
#else  // THEME_DARK (default)
  #define COL_SCREEN_BG      TFT_BLACK
  #define COL_HEADER_BG      TFT_BLACK
  #define COL_TEXT_PRIMARY   TFT_WHITE
  #define COL_LABEL          TFT_LIGHTGREY
  #define COL_MUTED          0x8410
  #define COL_DIVIDER        0x2104
  #define COL_BAR_TRACK      0x2104
  #define THEME_VIVID_HEADER 0
#endif

// Legacy aliases — main.cpp / older code may reference these from config.h
#undef  BACKGROUND_COLOR
#define BACKGROUND_COLOR COL_SCREEN_BG
#undef  TEXT_COLOR
#define TEXT_COLOR       COL_TEXT_PRIMARY
#undef  HIGHLIGHT_COLOR
#if DISPLAY_THEME == THEME_LIGHT
  #define HIGHLIGHT_COLOR 0xC2C0   // dark amber — readable on white
#else
  #define HIGHLIGHT_COLOR TFT_YELLOW
#endif

// Status colours — slightly darker in LIGHT theme for contrast on white bg
#if DISPLAY_THEME == THEME_LIGHT
  #define STATUS_GREEN  0x0420   // darker green
  #define STATUS_AMBER  0xCA20   // darker orange
  #define STATUS_RED    0xB000   // dark red
#else
  #define STATUS_GREEN  TFT_GREEN
  #define STATUS_AMBER  TFT_ORANGE
  #define STATUS_RED    TFT_RED
#endif

// ── Colour helpers ────────────────────────────────────────
static inline uint16_t healthColor(float pctUsed) {
    if (pctUsed < 0.5f) return STATUS_GREEN;
    if (pctUsed < 0.8f) return STATUS_AMBER;
    return STATUS_RED;
}
static inline uint16_t remainColor(float pctLeft) {
    if (pctLeft > 0.5f) return STATUS_GREEN;
    if (pctLeft > 0.2f) return STATUS_AMBER;
    return STATUS_RED;
}
// Header background — usually flat COL_HEADER_BG; only THEME_VIVID tints it by status
static inline uint16_t headerBgColor(float pctUsed) {
#if THEME_VIVID_HEADER
    if (pctUsed < 0.5f) return 0x0246;  // dark green tint
    if (pctUsed < 0.8f) return 0x4980;  // dark amber tint
    return 0x6082;                       // dark red tint
#else
    (void)pctUsed;
    return COL_HEADER_BG;
#endif
}

// ── Provider-specific header tint ─────────────────────────
// Subtle background tone hint per page type — makes it easy to see at a glance
// which provider is showing. In THEME_VIVID, status-tint takes precedence.
enum HeaderKind { HDR_OPENROUTER = 0, HDR_ANTHROPIC = 1, HDR_RELAY = 2 };

static inline uint16_t headerBgFor(HeaderKind k) {
#if THEME_VIVID_HEADER
    (void)k;
    return COL_HEADER_BG;  // VIVID overrides — use status-tinted bg
#elif DISPLAY_THEME == THEME_LIGHT
    switch (k) {
        case HDR_ANTHROPIC:  return 0xFEB3;  // pale peach (Anthropic)
        case HDR_OPENROUTER: return 0xD6FF;  // pale blue   (OpenRouter)
        case HDR_RELAY:      return 0xC7FF;  // pale cyan   (Claude.ai)
    }
    return COL_HEADER_BG;
#else  // THEME_DARK
    switch (k) {
        case HDR_ANTHROPIC:  return 0x3082;  // dark warm-brown (Anthropic)
        case HDR_OPENROUTER: return 0x10D3;  // dark indigo     (OpenRouter)
        case HDR_RELAY:      return 0x0228;  // dark teal       (Claude.ai)
    }
    return COL_HEADER_BG;
#endif
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
    Display*            tft;
    int                 selectedKeyIndex;
    int                 numKeys;
    int                 _page;       // 0 = API key page, 1 = Relay page
    bool                needsRedraw;
    TokenData           currentData;
    RelayData           _relayData;
    const APIKeyConfig* keyConfigs;

    int sw() const { return tft->width(); }
    int sh() const { return tft->height(); }

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
        tft->setTextColor(onFill ? TFT_WHITE : COL_TEXT_PRIMARY, onFill ? col : COL_BAR_TRACK);
        tft->drawString(String((int)(pct * 100)) + "%", x + w / 2, y + h / 2);
    }

    // ── Header ─────────────────────────────────────────────
    void drawHeader() {
        uint16_t accent = healthColor(pctUsed());
        HeaderKind kind = currentData.isAnthropicMode ? HDR_ANTHROPIC : HDR_OPENROUTER;
        uint16_t bgCol  = headerBgFor(kind);
#if THEME_VIVID_HEADER
        bgCol = headerBgColor(pctUsed());   // VIVID: status-tinted instead
#endif
        tft->fillRect(0, 0, sw(), HEADER_H, bgCol);

        tft->fillRect(0, 0, ACCENT_W, HEADER_H, accent);
        tft->fillRect(ACCENT_W, 0, 1, HEADER_H, COL_DIVIDER);

        String keyStr = (keyConfigs != nullptr)
            ? String(keyConfigs[selectedKeyIndex].name)
            : ("Key " + String(selectedKeyIndex + 1));

        const int cy = HEADER_H / 2;
        int x = ACCENT_W + 8;

#if UI_COMPACT_LAYOUT
        tft->setTextSize(1);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(accent, bgCol);
        tft->drawString("TOKEN", x, cy);
        x += tft->textWidth("TOKEN") + 4;
        tft->setTextColor(COL_TEXT_PRIMARY, bgCol);
        tft->drawString(keyStr, x, cy);
#else
        tft->setTextSize(2);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(accent, bgCol);
        tft->drawString("Token", x, cy);
        x += tft->textWidth("Token");
        tft->setTextColor(COL_TEXT_PRIMARY, bgCol);
        tft->drawString(" \xB7 " + keyStr, x, cy);
#endif

        tft->setTextDatum(MR_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(TFT_LIGHTGREY, bgCol);
        tft->drawString(String(selectedKeyIndex + 1) + "/" + String(numKeys),
                        sw() - 4, cy);

        tft->drawFastHLine(0, HEADER_H - 1, sw(), COL_DIVIDER);
    }

    // ══════════════════════════════════════════════════════
    //  ANTHROPIC: Rate-limit display
    // ══════════════════════════════════════════════════════
    void drawRateLimitUI() {
        const int CY = HEADER_H;
        const int CW = sw();
        const int CH = sh() - HEADER_H - FOOTER_H;
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

        const int L_X = ACCENT_W + 8;

#if UI_COMPACT_LAYOUT
        // ── Compact layout: Tokens (top) + Requests (bottom) ─
        // Fits 240×135 (TTGO T-Display) with 85px content area
        const int ROW_H = (CH - 1) / 2;

        float tokPct = (currentData.tok_limit > 0)
                     ? (float)currentData.tok_remaining / currentData.tok_limit : 1.0f;
        uint16_t tokCol = remainColor(tokPct);
        tft->setTextDatum(ML_DATUM); tft->setTextSize(1);
        tft->setTextColor(tokCol, BACKGROUND_COLOR);
        tft->drawString("TOKENS/MIN", L_X, CY + 4);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_MUTED, BACKGROUND_COLOR);
        tft->drawString("/" + fmtK(currentData.tok_limit), CW - 4, CY + 4);
        tft->setTextDatum(ML_DATUM); tft->setTextSize(2);
        tft->setTextColor(tokCol, BACKGROUND_COLOR);
        tft->drawString(fmtK(currentData.tok_remaining), L_X, CY + 15);
        drawBar(L_X, CY + 27, CW - L_X - 6, BAR_H, tokPct, tokCol);

        tft->drawFastHLine(L_X, CY + ROW_H, CW - L_X * 2, COL_DIVIDER);

        float reqPct = (currentData.req_limit > 0)
                     ? (float)currentData.req_remaining / currentData.req_limit : 1.0f;
        uint16_t reqCol = remainColor(reqPct);
        const int RS = CY + ROW_H + 2;
        tft->setTextDatum(ML_DATUM); tft->setTextSize(1);
        tft->setTextColor(reqCol, BACKGROUND_COLOR);
        tft->drawString("REQS/MIN", L_X, RS + 4);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_MUTED, BACKGROUND_COLOR);
        tft->drawString("/" + String(currentData.req_limit), CW - 4, RS + 4);
        tft->setTextDatum(ML_DATUM); tft->setTextSize(2);
        tft->setTextColor(reqCol, BACKGROUND_COLOR);
        tft->drawString(String(currentData.req_remaining), L_X, RS + 15);
        drawBar(L_X, RS + 27, CW - L_X - 6, BAR_H, reqPct, reqCol);

#else
        // ── Full 4-metric layout: Tokens|Reqs / Input|Output ────────────
        // All four panels share the same sub-layout so sizes stay balanced:
        //   y+ 6 : label (size 1, ML) + "/limit" (size 1, MR)
        //   y+22 : value (size 2, ML)
        //   y+34 : bar   (BAR_H px)
        //   y+54 : reset (size 1, ML) — Tokens panel only
        const int DIV_X   = CW / 2;
        const int R_X     = DIV_X + 8;
        const int L_BAR_W = DIV_X - L_X - 4;
        const int R_BAR_W = CW - R_X - 6;
        const int R1      = CY;                       // row 1 top
        const int DIV_Y   = CY + CH / 2;              // 88
        const int R2      = DIV_Y + 2;                // row 2 top

        auto drawPanel = [&](int x, int y, int barW,
                             const char* label, const String& valTxt,
                             int limit, const String& limTxt,
                             float pct, uint16_t col,
                             const String& extra) {
            // label + "/limit"
            tft->setTextSize(1);
            tft->setTextDatum(ML_DATUM);
            tft->setTextColor(col, BACKGROUND_COLOR);
            tft->drawString(label, x, y + 6);
            if (limit > 0) {
                tft->setTextDatum(MR_DATUM);
                tft->setTextColor(COL_MUTED, BACKGROUND_COLOR);
                tft->drawString("/" + limTxt, x + barW, y + 6);
            }
            // value
            tft->setTextDatum(ML_DATUM);
            tft->setTextSize(2);
            tft->setTextColor(col, BACKGROUND_COLOR);
            tft->drawString(valTxt, x, y + 22);
            // bar
            drawBar(x, y + 34, barW, BAR_H, pct, col);
            // optional extra line (e.g. "Reset 2m")
            if (extra.length()) {
                tft->setTextSize(1);
                tft->setTextDatum(ML_DATUM);
                tft->setTextColor(COL_MUTED, BACKGROUND_COLOR);
                tft->drawString(extra, x, y + 54);
            }
        };

        // ── Row 1: Tokens (L) + Requests (R) ────────────────────────────
        float tokPct = (currentData.tok_limit > 0)
                     ? (float)currentData.tok_remaining / currentData.tok_limit : 1.0f;
        float reqPct = (currentData.req_limit > 0)
                     ? (float)currentData.req_remaining / currentData.req_limit : 1.0f;
        drawPanel(L_X, R1, L_BAR_W,
                  "TOKENS / MIN", fmtK(currentData.tok_remaining),
                  currentData.tok_limit, fmtK(currentData.tok_limit),
                  tokPct, remainColor(tokPct),
                  currentData.reset_in.length() ? ("Reset " + currentData.reset_in) : String(""));
        drawPanel(R_X, R1, R_BAR_W,
                  "REQUESTS / MIN", String(currentData.req_remaining),
                  currentData.req_limit, String(currentData.req_limit),
                  reqPct, remainColor(reqPct),
                  String(""));

        // ── Dividers ────────────────────────────────────────────────────
        tft->drawFastHLine(L_X, DIV_Y, CW - L_X * 2, COL_DIVIDER);
        tft->drawFastVLine(DIV_X, R1 + 4, CH - 8, COL_DIVIDER);

        // ── Row 2: Input (L) + Output (R) ───────────────────────────────
        float inPct  = (currentData.in_limit > 0)
                     ? (float)currentData.in_remaining / currentData.in_limit : 1.0f;
        float outPct = (currentData.out_limit > 0)
                     ? (float)currentData.out_remaining / currentData.out_limit : 1.0f;
        drawPanel(L_X, R2, L_BAR_W,
                  "INPUT TOKENS", fmtK(currentData.in_remaining),
                  currentData.in_limit, fmtK(currentData.in_limit),
                  inPct, remainColor(inPct),
                  String(""));
        drawPanel(R_X, R2, R_BAR_W,
                  "OUTPUT TOKENS", fmtK(currentData.out_remaining),
                  currentData.out_limit, fmtK(currentData.out_limit),
                  outPct, remainColor(outPct),
                  String(""));
#endif
    }

    // ══════════════════════════════════════════════════════
    //  OPENROUTER: Credits display (unchanged)
    // ══════════════════════════════════════════════════════
    void drawCredits() {
        const int y0    = HEADER_H;
        const int areaH = sh() - HEADER_H - FOOTER_H - BAR_H - 8;
        tft->fillRect(0, y0, sw(), areaH, BACKGROUND_COLOR);

        if (!currentData.success) {
            tft->setTextDatum(MC_DATUM);
            tft->setTextColor(TFT_RED, BACKGROUND_COLOR);
            tft->setTextSize(2);
            tft->drawString("Fetch Error", sw() / 2, y0 + 28);
            tft->setTextSize(1);
            tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
            tft->drawString(currentData.error.substring(0, 38),
                            sw() / 2, y0 + 52);
            return;
        }

        float    pct  = pctUsed();
        uint16_t col  = healthColor(pct);
        float remaining = (currentData.limit > 0)
                        ? (currentData.limit - currentData.usage)
                        : currentData.credits;
        if (remaining < 0.0f) remaining = 0.0f;

#if UI_COMPACT_LAYOUT
        // ── Compact 240×135: stacked rows, no divider, single line stats ──
        const int LX = ACCENT_W + 6;

        tft->setTextDatum(ML_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
        tft->drawString("CREDITS LEFT", LX, y0 + 4);

        tft->setTextSize(2);
        tft->setTextColor(col, BACKGROUND_COLOR);
        tft->drawString("$" + String(remaining, 4), LX, y0 + 20);

        // USED / LIMIT on one line, size 1
        const int statsY = y0 + 40;
        tft->setTextSize(1);
        tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
        tft->drawString("USED", LX, statsY);
        tft->setTextColor(COL_TEXT_PRIMARY, BACKGROUND_COLOR);
        tft->drawString("$" + String(currentData.usage, 4), LX + 30, statsY);

        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
        String limitTxt = (currentData.limit > 0)
                        ? ("LIMIT $" + String(currentData.limit, 2))
                        : String("NO LIMIT");
        tft->drawString(limitTxt, sw() - 6, statsY);
#else
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
        tft->setTextSize(1);
        tft->drawString("CREDITS REMAINING", ACCENT_W + 6, y0 + 8);

        tft->setTextColor(col, BACKGROUND_COLOR);
        tft->setTextSize(3);
        tft->drawString("$" + String(remaining, 4), ACCENT_W + 6, y0 + 30);

        const int statsY = y0 + 64;
        const int midX   = sw() / 2;
        tft->drawFastVLine(midX, statsY - 6, 30, COL_DIVIDER);

        tft->setTextSize(1);
        tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
        tft->setTextDatum(ML_DATUM);
        tft->drawString("USED", ACCENT_W + 8, statsY);
        tft->setTextSize(2);
        tft->setTextColor(COL_TEXT_PRIMARY, BACKGROUND_COLOR);
        tft->drawString("$" + String(currentData.usage, 4), ACCENT_W + 8, statsY + 13);

        tft->setTextSize(1);
        tft->setTextColor(COL_LABEL, BACKGROUND_COLOR);
        tft->drawString("LIMIT", midX + 8, statsY);
        tft->setTextSize(2);
        tft->setTextColor(COL_TEXT_PRIMARY, BACKGROUND_COLOR);
        if (currentData.limit > 0)
            tft->drawString("$" + String(currentData.limit, 2), midX + 8, statsY + 13);
        else
            tft->drawString("No limit", midX + 8, statsY + 13);
#endif
    }

    void drawProgressBar() {
        const int barY = sh() - FOOTER_H - BAR_H - 6;
        tft->fillRect(0, barY - 4, sw(), BAR_H + 8, BACKGROUND_COLOR);
        if (!currentData.success || currentData.limit <= 0) return;

        const float    pct  = pctUsed();
        const uint16_t col  = healthColor(pct);
        const int barX  = ACCENT_W + 8;
        const int barW  = sw() - barX - 8;
        drawBar(barX, barY, barW, BAR_H, pct, col);
    }

    // ══════════════════════════════════════════════════════
    //  RELAY: Claude.ai session usage
    // ══════════════════════════════════════════════════════
    void drawRelayHeader() {
        const uint16_t accent = 0x07FF;     // cyan
        const uint16_t bgCol  = headerBgFor(HDR_RELAY);
        tft->fillRect(0, 0, sw(), HEADER_H, bgCol);
        tft->fillRect(0, 0, ACCENT_W, HEADER_H, accent);
        tft->fillRect(ACCENT_W, 0, 1, HEADER_H, COL_DIVIDER);

        const int cy = HEADER_H / 2;
        int x = ACCENT_W + 8;

#if UI_COMPACT_LAYOUT
        tft->setTextSize(1);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(accent, bgCol);
        tft->drawString("CLAUDE.AI", x, cy);
#else
        tft->setTextSize(2);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(accent, bgCol);
        tft->drawString("Token", x, cy);
        x += tft->textWidth("Token") * 2; // size-2 width
        tft->setTextColor(COL_TEXT_PRIMARY, bgCol);
        tft->drawString(" \xB7 Claude.ai", x, cy);
#endif

        tft->setTextDatum(MR_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(COL_MUTED, bgCol);
        tft->drawString("relay", sw() - 4, cy);

        tft->drawFastHLine(0, HEADER_H - 1, sw(), COL_DIVIDER);
    }

    void drawRelayPage() {
        const int CY = HEADER_H;
        const int CW = sw();
        const int CH = sh() - HEADER_H - FOOTER_H;
        const int BX = ACCENT_W + 8;
        const int BW = CW - BX - 8;

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

#if UI_COMPACT_LAYOUT
        // ── Compact 240×135: short labels, tighter bars ──
        const int ROW_H = CH / 2;

        float sPct = constrain(_relayData.sessionPct / 100.0f, 0.0f, 1.0f);
        uint16_t sCol = healthColor(sPct);
        const int S1 = CY + 2;
        tft->setTextSize(1);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(sCol, BACKGROUND_COLOR);
        tft->drawString("SESSION 5h", BX, S1 + 4);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_MUTED, BACKGROUND_COLOR);
        tft->drawString(_relayData.sessionReset, CW - 6, S1 + 4);
        drawBar(BX, S1 + 16, BW, BAR_H, sPct, sCol);

        float wPct = constrain(_relayData.weeklyPct / 100.0f, 0.0f, 1.0f);
        uint16_t wCol = healthColor(wPct);
        const int S2 = CY + ROW_H + 2;
        tft->setTextSize(1);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(wCol, BACKGROUND_COLOR);
        tft->drawString("WEEK 7d", BX, S2 + 4);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_MUTED, BACKGROUND_COLOR);
        tft->drawString(_relayData.weeklyReset, CW - 6, S2 + 4);
        drawBar(BX, S2 + 16, BW, BAR_H, wPct, wCol);
#else
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
#endif
    }

    void drawRelayFooter() {
        const int y = sh() - FOOTER_H;
        tft->fillRect(0, y, sw(), FOOTER_H, COL_SCREEN_BG);
        tft->drawFastHLine(0, y, sw(), COL_DIVIDER);

        tft->setTextSize(1);
        tft->setTextColor(COL_MUTED, COL_SCREEN_BG);
        tft->fillCircle(ACCENT_W + 4, y + FOOTER_H / 2, 2, COL_MUTED);
        tft->setTextDatum(ML_DATUM);
#if UI_COMPACT_LAYOUT
        tft->drawString("BOOT", ACCENT_W + 10, y + FOOTER_H / 2);
#else
        tft->drawString("BOOT = next", ACCENT_W + 10, y + FOOTER_H / 2);
#endif

        tft->fillCircle(sw() - 7, y + FOOTER_H / 2, 3,
                        _relayData.ok ? TFT_GREEN : TFT_RED);

        unsigned long sec = millis() / 1000;
        String upStr = (sec < 60)   ? String(sec) + "s"
                     : (sec < 3600) ? String(sec / 60) + "m"
                                    : String(sec / 3600) + "h";
        tft->setTextDatum(MR_DATUM);
        tft->drawString("up " + upStr, sw() - 14, y + FOOTER_H / 2);
    }

    // ── Footer ─────────────────────────────────────────────
    void drawFooter() {
        const int y = sh() - FOOTER_H;
        tft->fillRect(0, y, sw(), FOOTER_H, COL_SCREEN_BG);
        tft->drawFastHLine(0, y, sw(), COL_DIVIDER);

        tft->setTextSize(1);
        tft->setTextColor(COL_MUTED, COL_SCREEN_BG);
        tft->fillCircle(ACCENT_W + 4, y + FOOTER_H / 2, 2, COL_MUTED);
        tft->setTextDatum(ML_DATUM);
#if UI_COMPACT_LAYOUT
        tft->drawString("BOOT", ACCENT_W + 10, y + FOOTER_H / 2);
#else
        tft->drawString("BOOT = next key", ACCENT_W + 10, y + FOOTER_H / 2);
#endif

        tft->fillCircle(sw() - 7, y + FOOTER_H / 2, 3,
                        currentData.success ? TFT_GREEN : TFT_RED);

        unsigned long sec = millis() / 1000;
        String upStr = (sec < 60)   ? String(sec) + "s"
                     : (sec < 3600) ? String(sec / 60) + "m"
                                    : String(sec / 3600) + "h";
        tft->setTextDatum(MR_DATUM);
        tft->drawString("up " + upStr, sw() - 14, y + FOOTER_H / 2);
    }

public:
    DisplayUI(Display* display, int numApiKeys)
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
        tft->fillRect(0, HEADER_H, sw(),
                  sh() - HEADER_H - FOOTER_H, BACKGROUND_COLOR);
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
