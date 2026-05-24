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
#include <time.h>
#include "config.h"
#include "GfxDisplay.h"
#include "OpenRouterAPI.h"
#include "AnthropicAPI.h"
#include "RelayAPI.h"
#include "DisplayUI.h"

// ── Board-specific display + touch ──────────────────────────────────────────
#ifdef BOARD_S3_TOUCH_LCD
// Waveshare ESP32-S3-Touch-LCD-1.9: ST7789 + CST816 capacitive touch
//   LCD: RST=9 CLK=10 DC=11 CS=12 MOSI=13 BL=14 (active LOW)
//   TP : SDA=47 SCL=48 INT=21 addr=0x15
#include <Wire.h>
#define LCD_RST   9
#define LCD_SCK  10
#define LCD_DC   11
#define LCD_CS   12
#define LCD_MOSI 13
#define LCD_BL   14
#define TOUCH_SDA  47
#define TOUCH_SCL  48
#define TOUCH_INT  21
#define TOUCH_ADDR 0x15

static Arduino_DataBus *gfxBus = new Arduino_ESP32SPI(
    LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED, FSPI);

// ST7789V2 170×320 portrait, rotation 1 → 320×170 landscape, with ±35 col offset
// 4th arg "isIPS" = true sends INVON so 0x0000 renders as black (not white)
static Arduino_GFX *gfxPanel = new Arduino_ST7789(
    gfxBus, LCD_RST, 1, true, 170, 320, 35, 0, 35, 0);

Display tft(gfxPanel);

static void touchInit() {
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(400000);
    pinMode(TOUCH_INT, INPUT);
    Wire.beginTransmission(TOUCH_ADDR);
    uint8_t err = Wire.endTransmission();
    Serial.printf("[Touch] CST816 probe @0x%02X SDA=%d SCL=%d -> %s\n",
                  TOUCH_ADDR, TOUCH_SDA, TOUCH_SCL,
                  err == 0 ? "ACK" : "no ACK");
}

// Read CST816 touch: reg 0x01 starts at Gesture, then NumPoints, X-hi/lo, Y-hi/lo
static bool touchRead(uint16_t *x, uint16_t *y) {
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write(0x01);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)6) < 6) return false;
    uint8_t b[6];
    for (int i = 0; i < 6; i++) b[i] = Wire.read();
    if (b[1] == 0) return false;   // no touch points
    *x = ((uint16_t)(b[2] & 0x0F) << 8) | b[3];
    *y = ((uint16_t)(b[4] & 0x0F) << 8) | b[5];
    return true;
}
#else
// Original ESP32 / TTGO path — TFT_eSPI with BOOT button
Display tft = TFT_eSPI();
#endif

OpenRouterAPI  api;
AnthropicAPI   anthApi;
RelayAPI       relayApi;
DisplayUI* ui;

// State
unsigned long lastUpdateTime = 0;
unsigned long lastRelayTime  = 0;
bool wifiConnected   = false;
bool relayWasOnline  = false;
int currentKeyIndex  = 0;

// Input debounce (button or touch)
unsigned long lastButtonTime = 0;
const unsigned long DEBOUNCE_MS = 300;
#ifdef BOARD_S3_TOUCH_LCD
bool touchPrevState = false; // edge-detect: fire once per touch, not on hold
#endif

// Forward declarations
void setupWiFi();
void updateTokenData();
void updateRelayData();
void handleButton();

// ── setup ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

#ifdef BOARD_S3_TOUCH_LCD
    // Backlight ON (active LOW on Waveshare ESP32-S3-Touch-LCD-1.9)
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, LOW);
    touchInit();
#else
    pinMode(BUTTON_PIN, INPUT_PULLUP);
#endif

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(BACKGROUND_COLOR);

    const int sw = tft.width();
    const int sh = tft.height();

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(HIGHLIGHT_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(2);
    tft.drawString("AI TOKEN MONITOR", sw / 2, sh / 2 - 14);
    tft.setTextSize(1);
    tft.setTextColor(COL_TEXT_PRIMARY, BACKGROUND_COLOR);
    tft.drawString("Connecting WiFi...", sw / 2, sh / 2 + 14);

    ui = new DisplayUI(&tft, NUM_API_KEYS);
    ui->setAPIKeyLabels(API_KEYS);

    relayApi.setUrl(RELAY_HOST, RELAY_PORT);

    setupWiFi();

    if (NUM_API_KEYS > 0) {
        currentKeyIndex = 0;
        if (API_KEYS[0].isAnthropic)
            anthApi.setAPIKey(API_KEYS[0].key);
        else
            api.setAPIKey(API_KEYS[0].key);
    }

    ui->init();

    if (wifiConnected) {
        updateRelayData();
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
            if (ui->isRelayPage()) updateRelayData();
            else                   updateTokenData();
        }
    }

    if (wifiConnected && (millis() - lastUpdateTime > UPDATE_INTERVAL)) {
        updateTokenData();
    }

    if (wifiConnected && (millis() - lastRelayTime > UPDATE_INTERVAL)) {
        updateRelayData();
    }

    delay(50);
}

// ── handleButton ──────────────────────────────────────────
// On BOARD_S3_TOUCH_LCD: tap anywhere on screen to cycle
// On other boards:       press BOOT button (GPIO 0, active LOW)
static void cycleKey() {
    ui->nextKey();
    if (!ui->isRelayPage()) {
        currentKeyIndex = ui->getSelectedKeyIndex();
        if (API_KEYS[currentKeyIndex].isAnthropic)
            anthApi.setAPIKey(API_KEYS[currentKeyIndex].key);
        else
            api.setAPIKey(API_KEYS[currentKeyIndex].key);
        Serial.printf("Switched to key: %s\n", API_KEYS[currentKeyIndex].name);
    } else {
        Serial.println("Switched to relay page");
    }
}

void handleButton() {
#ifdef BOARD_S3_TOUCH_LCD
    uint16_t tx, ty;
    bool isTouched = touchRead(&tx, &ty);
    // Trigger only on the leading edge (finger-down), not while holding
    if (isTouched && !touchPrevState) {
        unsigned long now = millis();
        if (now - lastButtonTime > DEBOUNCE_MS) {
            lastButtonTime = now;
            Serial.printf("[Touch] tap x=%d y=%d\n", tx, ty);
            cycleKey();
        }
    }
    touchPrevState = isTouched;
#else
    if (digitalRead(BUTTON_PIN) == LOW) {
        unsigned long now = millis();
        if (now - lastButtonTime > DEBOUNCE_MS) {
            lastButtonTime = now;
            cycleKey();
        }
    }
#endif
}

// ── setupWiFi ─────────────────────────────────────────────
void setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    tft.fillScreen(BACKGROUND_COLOR);
    const int sw = tft.width();
    const int sh = tft.height();
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT_PRIMARY, BACKGROUND_COLOR);
    tft.setTextSize(2);
    tft.drawString("Connecting WiFi", sw / 2, sh / 2 - 14);
    tft.setTextSize(1);
    tft.drawString(WIFI_SSID, sw / 2, sh / 2 + 10);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
        String dots(attempts % 4, '.');
        tft.fillRect(sw / 2 - 30, sh / 2 + 26, 60, 14, BACKGROUND_COLOR);
        tft.drawString(dots, sw / 2, sh / 2 + 32);
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        // Sync time via NTP (needed for OpenAI billing usage query)
        configTime(0, 0, "pool.ntp.org", "time.google.com");
        tft.fillScreen(BACKGROUND_COLOR);
        tft.setTextColor(STATUS_GREEN, BACKGROUND_COLOR);
        tft.setTextSize(2);
        tft.drawString("WiFi Connected!", sw / 2, sh / 2 - 14);
        tft.setTextSize(1);
        tft.setTextColor(COL_TEXT_PRIMARY, BACKGROUND_COLOR);
        tft.drawString(WiFi.localIP().toString(), sw / 2, sh / 2 + 10);
        delay(1500);
    } else {
        wifiConnected = false;
        tft.fillScreen(BACKGROUND_COLOR);
        tft.setTextColor(STATUS_RED, BACKGROUND_COLOR);
        tft.setTextSize(2);
        tft.drawString("WiFi Failed!", sw / 2, sh / 2 - 14);
        tft.setTextSize(1);
        tft.setTextColor(COL_TEXT_PRIMARY, BACKGROUND_COLOR);
        tft.drawString("Check credentials", sw / 2, sh / 2 + 10);
        delay(2000);
    }
}

// ── updateRelayData ───────────────────────────────────────
void updateRelayData() {
    if (!wifiConnected) return;

    RelayData rd = relayApi.fetch();
    lastRelayTime = millis();

    // Auto-switch to relay page when server first comes online
    if (rd.ok && !relayWasOnline) {
        Serial.println("Relay server detected — switching to relay page");
        ui->goToRelayPage();
    }
    relayWasOnline = rd.ok;

    ui->updateRelayDisplay(rd);

    if (rd.ok)
        Serial.printf("Relay OK  session=%.0f%%  weekly=%.0f%%\n",
                      rd.sessionPct, rd.weeklyPct);
    else
        Serial.printf("Relay: %s\n", rd.error.c_str());
}

// ── updateTokenData ───────────────────────────────────────
void updateTokenData() {
    if (!wifiConnected) return;

    Serial.printf("Fetching: %s\n", API_KEYS[currentKeyIndex].name);

    // Show "Updating..." in footer right side (away from data area)
    int _fy = tft.height() - FOOTER_H;
    const int sw = tft.width();
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(HIGHLIGHT_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.fillRect(sw / 2, _fy + 1, sw / 2 - 1, FOOTER_H - 2, BACKGROUND_COLOR);
    tft.drawString("Updating...", sw - 8, _fy + FOOTER_H / 2);

    TokenData data = API_KEYS[currentKeyIndex].isAnthropic
                   ? anthApi.getRateLimits()
                   : api.getCredits();
    ui->updateTokenDisplay(data);
    lastUpdateTime = millis();

    if (data.success) {
        if (data.isAnthropicMode)
            Serial.printf("Tokens: %d/%d  Req: %d/%d  Reset: %s\n",
                          data.tok_remaining, data.tok_limit,
                          data.req_remaining, data.req_limit,
                          data.reset_in.c_str());
        else
            Serial.printf("Credits: %.4f  Used: %.4f  Limit: %.2f\n",
                          data.credits, data.usage, data.limit);
    } else {
        Serial.printf("Error: %s\n", data.error.c_str());
    }
}
