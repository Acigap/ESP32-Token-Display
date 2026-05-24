/**
 * POC: Waveshare ESP32-S3-Touch-LCD-1.9
 * ============================================================
 * Goal: verify the LCD + touch hardware works using Arduino_GFX
 *       before porting the main TokenDisplay firmware.
 *
 * Pin map (Waveshare official):
 *   LCD: RST=9 CLK=10 DC=11 CS=12 MOSI=13 BL=14
 *   TP : SDA=47 SCL=48 INT=21 (RST=17, NC)
 *
 * Touch IC: CST816 (I2C @ 0x15) — registers:
 *   0x01 Gesture     0x02 #points   0x03..0x06 X/Y (12-bit, big-endian)
 */

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

// ── Pin definitions ──────────────────────────────────────────────────────────
#define LCD_RST   9
#define LCD_SCK  10
#define LCD_DC   11
#define LCD_CS   12
#define LCD_MOSI 13
#define LCD_BL   14   // active LOW on this Waveshare board

#define TP_SDA   47
#define TP_SCL   48
#define TP_INT   21
#define TP_ADDR  0x15

// ── Arduino_GFX bus + display ────────────────────────────────────────────────
Arduino_DataBus *bus = new Arduino_ESP32SPI(
    LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED, FSPI);

// ST7789V2: portrait 170×320 with ±35 column offset; rotation 1 -> landscape 320×170
Arduino_GFX *gfx = new Arduino_ST7789(
    bus, LCD_RST,
    1,      // rotation 1 = landscape (320×170)
    false,  // NOT IPS panel
    170, 320,
    35, 0, 35, 0);

static const int SCREEN_W = 320;
static const int SCREEN_H = 170;

// ── Colours ──────────────────────────────────────────────────────────────────
#define C_BG        0x0000
#define C_HEADER    0x0821
#define C_TITLE     0xFFE0  // yellow
#define C_CYAN      0x07FF
#define C_WHITE     0xFFFF
#define C_GREEN     0x07E0
#define C_RED       0xF800
#define C_GRAY      0x7BEF

// ── CST816 touch ─────────────────────────────────────────────────────────────
static bool touchRead(uint16_t *x, uint16_t *y) {
    Wire.beginTransmission(TP_ADDR);
    Wire.write(0x01);                       // start at gesture reg
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)6) < 6) return false;

    uint8_t b[6];
    for (int i = 0; i < 6; i++) b[i] = Wire.read();

    if (b[1] == 0) return false;            // no touch points
    *x = ((uint16_t)(b[2] & 0x0F) << 8) | b[3];
    *y = ((uint16_t)(b[4] & 0x0F) << 8) | b[5];
    return true;
}

// ── Demo screen ──────────────────────────────────────────────────────────────
static uint32_t lastTouchMs = 0;
static int      tapCount    = 0;

static void drawScreen() {
    gfx->fillScreen(C_BG);

    // Header
    gfx->fillRect(0, 0, SCREEN_W, 28, C_HEADER);
    gfx->setTextSize(2);
    gfx->setTextColor(C_CYAN);
    gfx->setCursor(6, 7);
    gfx->print("ESP32-S3 LCD 1.9");

    // Big title
    gfx->setTextColor(C_TITLE);
    gfx->setTextSize(3);
    gfx->setCursor(40, 50);
    gfx->print("HELLO S3!");

    // Status lines
    gfx->setTextSize(1);
    gfx->setTextColor(C_GREEN);
    gfx->setCursor(10, 90);
    gfx->print("Display: OK");

    gfx->setTextColor(C_WHITE);
    gfx->setCursor(10, 105);
    gfx->printf("Resolution: %dx%d", SCREEN_W, SCREEN_H);

    gfx->setCursor(10, 120);
    gfx->printf("Taps: %d", tapCount);

    // Footer
    gfx->fillRect(0, SCREEN_H - 16, SCREEN_W, 16, 0x3186);
    gfx->setTextColor(C_GRAY);
    gfx->setCursor(4, SCREEN_H - 12);
    gfx->printf("Up:%lus  Tap screen to test touch", millis() / 1000UL);
}

// ── Setup / loop ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[POC-S3] starting...");
    Serial.printf("[POC-S3] LCD pins: RST=%d SCK=%d DC=%d CS=%d MOSI=%d BL=%d\n",
                  LCD_RST, LCD_SCK, LCD_DC, LCD_CS, LCD_MOSI, LCD_BL);
    Serial.printf("[POC-S3] TP  pins: SDA=%d SCL=%d INT=%d addr=0x%02X\n",
                  TP_SDA, TP_SCL, TP_INT, TP_ADDR);

    // Backlight: confirmed active LOW on Waveshare ESP32-S3-Touch-LCD-1.9
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, LOW);

    // Display
    if (!gfx->begin()) {
        Serial.println("[ERR] gfx->begin() failed!");
        while (1) delay(1000);
    }
    Serial.printf("[POC-S3] Display ready: %d x %d\n", gfx->width(), gfx->height());

    // Touch I2C
    Wire.begin(TP_SDA, TP_SCL);
    Wire.setClock(400000);
    pinMode(TP_INT, INPUT);

    // Quick I2C probe so we know if the touch chip is alive
    Wire.beginTransmission(TP_ADDR);
    uint8_t err = Wire.endTransmission();
    Serial.printf("[POC-S3] CST816 probe at 0x%02X -> %s (err=%d)\n",
                  TP_ADDR, err == 0 ? "ACK" : "no ACK", err);

    drawScreen();
}

void loop() {
    uint16_t tx, ty;
    if (touchRead(&tx, &ty)) {
        if (millis() - lastTouchMs > 250) {
            lastTouchMs = millis();
            tapCount++;
            Serial.printf("[POC-S3] tap x=%u y=%u  (count=%d)\n", tx, ty, tapCount);
            drawScreen();
        }
    }

    // Refresh uptime in footer every second
    static uint32_t lastDraw = 0;
    if (millis() - lastDraw >= 1000) {
        lastDraw = millis();
        gfx->fillRect(0, SCREEN_H - 16, SCREEN_W, 16, 0x3186);
        gfx->setTextColor(C_GRAY);
        gfx->setTextSize(1);
        gfx->setCursor(4, SCREEN_H - 12);
        gfx->printf("Up:%lus  Tap screen to test touch", millis() / 1000UL);
    }

    delay(20);
}
