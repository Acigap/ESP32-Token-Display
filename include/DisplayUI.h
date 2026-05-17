#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <TFT_eSPI.h>
#include <SPI.h>
#include "config.h"
#include "OpenRouterAPI.h"

// Layout constants (landscape 320x170)
#define HEADER_H  28
#define FOOTER_H  24
#define BAR_H     18

class DisplayUI {
private:
    TFT_eSPI*           tft;
    int                 selectedKeyIndex;
    int                 numKeys;
    bool                needsRedraw;
    TokenData           currentData;
    const APIKeyConfig* keyConfigs;

    // ── Header ─────────────────────────────────────────────
    void drawHeader() {
        tft->fillRect(0, 0, SCREEN_WIDTH, HEADER_H, TFT_NAVY);
        tft->drawFastHLine(0, HEADER_H, SCREEN_WIDTH, TFT_DARKGREY);

        String label = (keyConfigs != nullptr)
            ? String(keyConfigs[selectedKeyIndex].name)
            : "Key " + String(selectedKeyIndex + 1);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(TFT_YELLOW, TFT_NAVY);
        tft->setTextSize(2);
        tft->drawString(label, 8, HEADER_H / 2);

        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(TFT_LIGHTGREY, TFT_NAVY);
        tft->setTextSize(1);
        tft->drawString(String(selectedKeyIndex + 1) + "/" + String(numKeys),
                        SCREEN_WIDTH - 6, HEADER_H / 2);
    }

    // ── Credits area ───────────────────────────────────────
    void drawCredits() {
        int y    = HEADER_H + 4;
        int areaH = SCREEN_HEIGHT - HEADER_H - FOOTER_H - BAR_H - 14;
        tft->fillRect(0, y, SCREEN_WIDTH, areaH, BACKGROUND_COLOR);

        if (!currentData.success) {
            tft->setTextDatum(MC_DATUM);
            tft->setTextColor(TFT_RED, BACKGROUND_COLOR);
            tft->setTextSize(2);
            tft->drawString("Fetch Error", SCREEN_WIDTH / 2, y + 22);
            tft->setTextSize(1);
            tft->setTextColor(TFT_WHITE, BACKGROUND_COLOR);
            tft->drawString(currentData.error.substring(0, 42),
                            SCREEN_WIDTH / 2, y + 44);
            return;
        }

        // "CREDITS REMAINING" label
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(TFT_DARKGREY, BACKGROUND_COLOR);
        tft->setTextSize(1);
        tft->drawString("CREDITS REMAINING", 10, y + 4);

        // Big credit number
        tft->setTextColor(TFT_GREEN, BACKGROUND_COLOR);
        tft->setTextSize(4);
        tft->setTextDatum(ML_DATUM);
        tft->drawString("$" + String(currentData.credits, 4), 10, y + 24);

        // Usage  |  Limit
        tft->setTextSize(2);
        tft->setTextColor(TFT_LIGHTGREY, BACKGROUND_COLOR);
        tft->setTextDatum(ML_DATUM);
        tft->drawString("Token used: $" + String(currentData.usage, 4), 10, y + 60);
        if (currentData.limit > 0) {
            tft->setTextDatum(ML_DATUM);
            tft->drawString("Token limit: $" + String(currentData.limit, 2),
                            10, y + 76);
        }
    }

    // ── Progress bar ───────────────────────────────────────
    void drawProgressBar() {
        int barY = SCREEN_HEIGHT - FOOTER_H - BAR_H - 4;
        tft->fillRect(0, barY - 2, SCREEN_WIDTH, BAR_H + 6, BACKGROUND_COLOR);

        if (!currentData.success || currentData.limit <= 0) return;

        const int barX = 10;
        const int barW = SCREEN_WIDTH - 20;

        float pct = currentData.usage / currentData.limit;  // usage %
        if (pct < 0.0f) pct = 0.0f;
        if (pct > 1.0f) pct = 1.0f;
        int fillW = (int)(pct * barW);

        // Track
        tft->fillRoundRect(barX, barY, barW, BAR_H, 5, TFT_DARKGREY);

        // Fill — colour by usage level (high usage = red)
        uint16_t fillColor = (pct > 0.8f) ? TFT_RED
                           : (pct > 0.5f) ? TFT_ORANGE
                                          : TFT_GREEN;
        if (fillW > 0) {
            tft->fillRoundRect(barX, barY, fillW, BAR_H, 5, fillColor);
        }

        // Percentage label
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(1);
        tft->setTextColor(TFT_WHITE, fillW >= barW / 2 ? fillColor : TFT_DARKGREY);
        tft->drawString(String((int)(pct * 100)) + "%",
                        barX + barW / 2, barY + BAR_H / 2);
    }

    // ── Footer ─────────────────────────────────────────────
    void drawFooter() {
        int y = SCREEN_HEIGHT - FOOTER_H;
        tft->fillRect(0, y, SCREEN_WIDTH, FOOTER_H, TFT_BLACK);
        tft->drawFastHLine(0, y, SCREEN_WIDTH, TFT_DARKGREY);

        tft->setTextSize(1);
        tft->setTextColor(TFT_DARKGREY, TFT_BLACK);

        tft->setTextDatum(ML_DATUM);
        tft->drawString("BOOT = next key", 8, y + FOOTER_H / 2);

        tft->setTextDatum(MR_DATUM);
        unsigned long sec = millis() / 1000;
        tft->drawString("up " + String(sec) + "s", SCREEN_WIDTH - 8, y + FOOTER_H / 2);
    }

public:
    DisplayUI(TFT_eSPI* display, int numApiKeys)
        : tft(display), numKeys(numApiKeys), selectedKeyIndex(0),
          needsRedraw(true), keyConfigs(nullptr) {
        currentData = {};
    }

    void setAPIKeyLabels(const APIKeyConfig* keys) {
        keyConfigs = keys;
    }

    void init() {
        tft->setRotation(1);   // Landscape
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

    // Partial redraw after API fetch (header stays)
    void updateTokenDisplay(const TokenData& data) {
        currentData = data;
        drawCredits();
        drawProgressBar();
        drawFooter();
    }

    int getSelectedKeyIndex() { return selectedKeyIndex; }

    // Advance to next API key (wraps around)
    void nextKey() {
        selectedKeyIndex = (selectedKeyIndex + 1) % numKeys;
        needsRedraw = true;
    }
};

#endif // DISPLAY_UI_H
