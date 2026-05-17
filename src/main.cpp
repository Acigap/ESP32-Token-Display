/**
 * ESP32 OpenRouter Token Display
 *
 * Hardware:
 * - ESP32-WROOM-32 (esp32dev)
 * - ST7789 1.9" 170x320 (landscape: 320x170)
 * - BOOT button (GPIO 0) to cycle API keys
 *
 * Author: Gap_code
 */

#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "config.h"
#include "OpenRouterAPI.h"
#include "DisplayUI.h"

// Global objects
TFT_eSPI tft = TFT_eSPI();
OpenRouterAPI api;
DisplayUI* ui;

// State
unsigned long lastUpdateTime = 0;
bool wifiConnected = false;
int currentKeyIndex = 0;

// Button debounce
unsigned long lastButtonTime = 0;
const unsigned long DEBOUNCE_MS = 300;

// Forward declarations
void setupWiFi();
void updateTokenData();
void handleButton();

// ── setup ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("AI TOKEN MONITOR", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 14);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Connecting WiFi...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 14);

    ui = new DisplayUI(&tft, NUM_API_KEYS);
    ui->setAPIKeyLabels(API_KEYS);

    setupWiFi();

    if (NUM_API_KEYS > 0) {
        api.setAPIKey(API_KEYS[0].key);
        currentKeyIndex = 0;
    }

    ui->init();

    if (wifiConnected) {
        updateTokenData();
    }
}

// ── loop ──────────────────────────────────────────────────
void loop() {
    if (WiFi.status() != WL_CONNECTED && wifiConnected) {
        wifiConnected = false;
        setupWiFi();
    }

    handleButton();

    if (ui->needsUpdate()) {
        ui->drawUI();
        if (wifiConnected) {
            updateTokenData();
        }
    }

    if (wifiConnected && (millis() - lastUpdateTime > UPDATE_INTERVAL)) {
        updateTokenData();
    }

    delay(50);
}

// ── handleButton ──────────────────────────────────────────
void handleButton() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        unsigned long now = millis();
        if (now - lastButtonTime > DEBOUNCE_MS) {
            lastButtonTime = now;
            ui->nextKey();
            currentKeyIndex = ui->getSelectedKeyIndex();
            api.setAPIKey(API_KEYS[currentKeyIndex].key);
            Serial.printf("Switched to key: %s\n", API_KEYS[currentKeyIndex].name);
        }
    }
}

// ── setupWiFi ─────────────────────────────────────────────
void setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Connecting WiFi", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 14);
    tft.setTextSize(1);
    tft.drawString(WIFI_SSID, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 10);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
        String dots(attempts % 4, '.');
        tft.fillRect(SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2 + 26, 60, 14, TFT_BLACK);
        tft.drawString(dots, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 32);
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        // Sync time via NTP (needed for OpenAI billing usage query)
        configTime(0, 0, "pool.ntp.org", "time.google.com");
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.drawString("WiFi Connected!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 14);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(WiFi.localIP().toString(), SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 10);
        delay(1500);
    } else {
        wifiConnected = false;
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(2);
        tft.drawString("WiFi Failed!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 14);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Check credentials", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 10);
        delay(2000);
    }
}

// ── updateTokenData ───────────────────────────────────────
void updateTokenData() {
    if (!wifiConnected) return;

    Serial.printf("Fetching: %s\n", API_KEYS[currentKeyIndex].name);

    // Show "Updating..." in footer right side (away from data area)
    int _fy = SCREEN_HEIGHT - FOOTER_H;
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.fillRect(SCREEN_WIDTH / 2, _fy + 1, SCREEN_WIDTH / 2 - 1, FOOTER_H - 2, TFT_BLACK);
    tft.drawString("Updating...", SCREEN_WIDTH - 8, _fy + FOOTER_H / 2);

    TokenData data = api.getCredits();
    ui->updateTokenDisplay(data);
    lastUpdateTime = millis();

    if (data.success) {
        Serial.printf("Credits: %.4f  Used: %.4f  Limit: %.2f\n",
                      data.credits, data.usage, data.limit);
    } else {
        Serial.printf("Error: %s\n", data.error.c_str());
    }
}
