# ESP32 Token Display

แสดงผล API usage จาก **OpenRouter**, **Anthropic**, และ **Claude.ai** บน ESP32 ที่มีจอ ST7789 1.9" ในตัว  
กด BOOT button เพื่อสลับหน้าแสดงผล

---

## Features

- แสดง Credits / Usage / Limit จาก **OpenRouter API**
- แสดง Rate Limits (Tokens, Requests, Input, Output) จาก **Anthropic API**
- แสดง Session (5h) และ Weekly (7d) usage จาก **Claude.ai** ผ่าน relay server
- สลับระหว่าง API Keys และหน้า relay ด้วย **BOOT Button (GPIO 0)**
- อัปเดตอัตโนมัติทุก 30 วินาที
- Auto-switch ไปหน้า Claude.ai ทันทีที่ relay server เริ่มทำงาน
- ตั้งค่าทุกอย่างผ่าน **VSCode Extension** — ไม่ต้องแก้ไขไฟล์เอง

---

## Hardware

**ESP32 Development Board (All-in-One)** — จอ ST7789 1.9" 170×320 ฝังในตัว ไม่ต้องต่อสายเพิ่ม

| รายการ | รายละเอียด |
|--------|------------|
| MCU | ESP32 (Xtensa LX6 dual-core 240 MHz) |
| Flash | 4 MB |
| จอ | ST7789 TFT 1.9" 170×320 (landscape = 320×170) |
| WiFi | 2.4 GHz |
| USB | Type-C + CH340 |
| ปุ่ม | BOOT (GPIO 0) |

| Signal | GPIO |
|--------|------|
| MOSI | 23 |
| SCLK | 18 |
| CS | 15 |
| DC | 2 |
| RST | 4 |
| BL | 32 |

---

## File Structure

```
ESP32-Token-Display/
├── include/
│   ├── config.h            ← WiFi, API Keys, Relay Host (แก้ผ่าน extension)
│   ├── config.h.example    ← template สำหรับ config.h
│   ├── AnthropicAPI.h      ← fetch rate limits จาก Anthropic
│   ├── OpenRouterAPI.h     ← fetch credits จาก OpenRouter
│   ├── RelayAPI.h          ← fetch usage จาก relay server
│   └── DisplayUI.h         ← render หน้าจอทั้งหมด
├── src/
│   └── main.cpp
├── server/
│   ├── relay.py            ← relay server (รันบน PC)
│   └── .env                ← Claude.ai credentials (ไม่ commit)
├── poc/                    ← scripts ทดสอบ API
│   ├── poc_claude_api.py
│   ├── poc_claudeai_usage.py
│   └── ...
└── vscode-extension/       ← ESP32 Token Display Config extension
```

---

## Display Pages

BOOT button วนหน้าจอตามลำดับ: **Key 1 → Key 2 → ... → Claude.ai → Key 1**

### OpenRouter (credits)
แสดง Credits remaining, Used, Limit พร้อม progress bar

### Anthropic (rate limits)
แสดง Tokens/min, Requests/min, Input tokens, Output tokens พร้อม reset timer

### Claude.ai Relay
แสดง Session usage (5 ชั่วโมง) และ Weekly usage (7 วัน) พร้อมเวลา reset  
หน้าจะ switch อัตโนมัติเมื่อ relay server เพิ่งเริ่มทำงาน

---

## Setup

### 1. ติดตั้ง Extension

ติดตั้ง **ESP32 Token Display Config** extension ใน VS Code (ไฟล์ `.vsix` ใน `vscode-extension/`)

```
Extensions → ··· → Install from VSIX → เลือก esp32-token-display-1.0.0.vsix
```

### 2. ตั้งค่าผ่าน Extension

เปิด Command Palette → `ESP32 Token Display: Open Config`

**Tab: Device**
- ใส่ WiFi SSID และ Password
- เลือก COM Port ของ ESP32

**Tab: API Keys**
- เพิ่ม OpenRouter keys (`sk-or-v1-...`)
- เพิ่ม Anthropic key (`sk-ant-api03-...`) — เลือก Type: Anthropic

**Tab: Claude.ai**
- ใส่ Session Key และ Org ID (ดูวิธีได้จากหน้า info ใน extension)
- ใส่ PC IP (Host) — extension จะตรวจหาให้อัตโนมัติ
- กด **💾 Save Config** เพื่อบันทึก (เขียนลง `include/config.h` และ `server/.env`)

### 3. Flash ESP32

**Tab: Actions** → กด **⚡ Build & Flash**

หรือใช้ PlatformIO CLI:
```bash
pio run -t upload --upload-port COM8
```

### 4. เริ่ม Relay Server (สำหรับ Claude.ai)

ต้องการ Python + `curl_cffi`:
```bash
pip install curl_cffi flask python-dotenv
```

**Tab: Actions** → กด **▶ Start Relay**

หรือรันเอง:
```bash
cd server
python relay.py
```

Relay server จะรันบน `http://localhost:8765/usage`

---

## Configuration Reference

### include/config.h

```cpp
// WiFi
#define WIFI_SSID     "ชื่อ_WiFi"
#define WIFI_PASSWORD "รหัสผ่าน"

// API Keys
const APIKeyConfig API_KEYS[] = {
    {"HomeAI",    "sk-or-v1-...", false},   // OpenRouter
    {"Anthropic", "sk-ant-...",   true},    // Anthropic
};

// Relay server (PC running server/relay.py)
#define RELAY_HOST "192.168.0.58"
#define RELAY_PORT 8765

// Update interval
#define UPDATE_INTERVAL 30000  // ms
```

### server/.env

```env
CLAUDEAI_SESSION="sk-ant-sid02-..."
LASTACTIVE_ORG="xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
RELAY_PORT=8765
```

ได้ค่าจาก: เปิด claude.ai → F12 → Application → Cookies → `.claude.ai`  
คัดลอก `sessionKey` และ `lastActiveOrg`

---

## Troubleshooting

**Upload ไม่ได้**  
ตรวจสอบ COM Port ใน Device Manager, ติดตั้ง CH340 driver, ลองกดปุ่ม BOOT ค้างไว้ตอน upload

**WiFi เชื่อมไม่ได้**  
ตรวจสอบ SSID/Password, ESP32 รองรับเฉพาะ 2.4 GHz

**Relay offline ขึ้นบนจอ**  
ตรวจสอบว่า `server/relay.py` กำลังรัน และ `RELAY_HOST` ตรงกับ IP ของ PC

**Anthropic แสดง API Error**  
ตรวจสอบ API key และว่า key มี rate limit tier อยู่

**Serial Monitor**  
```bash
pio device monitor --port COM8
```
หรือกดปุ่ม 📺 Serial Monitor ใน extension (Tab: Actions)

---

## Libraries

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) by Bodmer
- [ArduinoJson](https://arduinojson.org/) by Benoit Blanchon

## License

MIT
