# OpenRouter Token Display - ESP32 + ST7789 (All-in-One Board)

โปรเจกต์แสดงผล Token/Credits จาก OpenRouter API บน **ESP32 Development Board แบบมีจอในตัว** (ST7789 1.9" 170×320) กด BOOT button เพื่อสลับ API Key

## 🎯 Features

- ✨ แสดง Credits, Usage, และ Limit จาก OpenRouter API
- 🔄 สลับระหว่าง API Keys หลายตัวได้ด้วย **BOOT Button (GPIO 0)** บนบอร์ด
- 📊 อัพเดทข้อมูลอัตโนมัติทุก 30 วินาที
- 🎨 UI สวยงามแบบ landscape 320×170
- 📶 แสดงสถานะ WiFi และ progress bar

## 🛠️ Hardware

### บอร์ดที่ใช้

**ESP32 Development Board (All-in-One)** — มีจอ ST7789 ในตัว ไม่ต้องต่อสายเพิ่ม

| รายการ | รายละเอียด |
|--------|------------|
| MCU    | ESP32 (Xtensa LX6 dual-core 240 MHz) |
| Flash  | 16 MB |
| จอแสดงผล | ST7789 TFT 1.9" 170×320 (built-in, landscape = 320×170) |
| ไร้สาย | WiFi 2.4GHz + Bluetooth |
| USB Driver | CH340 |
| พอร์ต | USB Type-C |
| ปุ่ม | BOOT (GPIO 0) — ใช้สลับ API Key |
| รองรับ | Arduino / MicroPython |

### GPIO ที่จอใช้ภายใน (ตั้งค่าใน platformio.ini)

| Signal | GPIO |
|--------|------|
| MOSI   | 23   |
| SCLK   | 18   |
| CS     | 15   |
| DC     | 2    |
| RST    | 4    |
| BL     | 32   |

> **ไม่ต้องต่อสายหน้าจอเพิ่มเติม** — จอ ST7789 ฝังอยู่ในบอร์ดแล้ว
> กด **BOOT button** บนบอร์ดเพื่อสลับ API Key

## 📦 Software Requirements

- **PlatformIO** (ติดตั้งผ่าน VS Code Extension)
- **ESP32 Platform** (จะติดตั้งอัตโนมัติ)

### Libraries ที่ใช้

- TFT_eSPI - สำหรับควบคุมหน้าจอ LCD
- ArduinoJson - สำหรับ parse JSON จาก API
- WiFi - สำหรับเชื่อมต่ออินเทอร์เน็ต
- HTTPClient - สำหรับเรียก API

## 🚀 การติดตั้งและใช้งาน

### 1. ติดตั้ง PlatformIO

ติดตั้ง PlatformIO extension ใน VS Code:
1. เปิด VS Code
2. ไปที่ Extensions (Ctrl+Shift+X)
3. ค้นหา "PlatformIO IDE"
4. คลิก Install

### 2. เปิดโปรเจกต์

```bash
cd TokenDisplay
code .
```

### 3. ตั้งค่า WiFi และ API Keys

สร้างหรือแก้ไขไฟล์ `include/config.h` (copy จาก `config.h.example`):

```cpp
// WiFi Configuration
#define WIFI_SSID "ชื่อ_WiFi_ของคุณ"
#define WIFI_PASSWORD "รหัสผ่าน_WiFi"

// API Keys
const APIKeyConfig API_KEYS[] = {
    {"Personal", "sk-or-v1-xxxxxxxxxxxxx"},
    {"Work", "sk-or-v1-yyyyyyyyyyyyy"},
    // เพิ่ม API Keys เพิ่มเติมได้ตามต้องการ
};
```

### 4. Build และ Upload

1. เสียบ ESP32-C6 เข้ากับคอมพิวเตอร์ผ่าน USB
2. กด PlatformIO: Upload (หรือใช้ shortcut)
3. รอจนการ upload เสร็จสิ้น

หรือใช้คำสั่ง:
```bash
pio run --target upload
```

### 6. ดู Serial Monitor (สำหรับ Debug)

```bash
pio device monitor
```

หรือคลิกที่ไอคอน Serial Monitor ใน PlatformIO

## 📱 การใช้งาน

### หน้าจอหลัก (landscape 320×170)

```
┌──────────────────────────────────────┐
│ PersonalKey               1/3        │ ← Header (navy)
├──────────────────────────────────────┤
│ CREDITS REMAINING                    │
│ $123.4567                            │ ← Credits (ใหญ่, เขียว)
│ Token used: $23.4567                 │
│ Token limit: $150.00                 │
├──────────────────────────────────────┤
│ [████████░░░░░░░░░░░░░░░░░] 15%      │ ← Progress bar
├──────────────────────────────────────┤
│ 12:34:56              Updating...    │ ← Footer
└──────────────────────────────────────┘
```

### การใช้งาน

1. **สลับ API Key**: กดปุ่ม **BOOT** บนบอร์ด ESP32 เพื่อวนเปลี่ยน API Key
2. **Auto-Update**: ระบบอัพเดทข้อมูลอัตโนมัติทุก 30 วินาที
3. **Header**: แสดงชื่อ API Key และลำดับ (เช่น 2/3)
4. **Progress bar**: แสดงสัดส่วน usage เทียบกับ limit

### สถานะ WiFi

- **เชื่อมต่อสำเร็จ**: แสดง "WiFi Connected!" สีเขียว พร้อม IP address
- **เชื่อมต่อไม่ได้**: แสดง "WiFi Failed!" สีแดง

## ⚙️ การปรับแต่ง

### เปลี่ยนความถี่ในการอัพเดท

ใน `config.h`:
```cpp
#define UPDATE_INTERVAL 30000  // หน่วยเป็น milliseconds (30000 = 30 วินาที)
```

### เปลี่ยนสีของ UI

ใน `config.h`:
```cpp
#define BACKGROUND_COLOR TFT_BLACK
#define TEXT_COLOR TFT_WHITE
#define HIGHLIGHT_COLOR TFT_YELLOW
```

### ปรับขา GPIO

ใน `platformio.ini` ส่วน `build_flags`:
```ini
-DTFT_MOSI=23   ; เปลี่ยนขา MOSI
-DTFT_SCLK=18   ; เปลี่ยนขา SCLK
-DTFT_CS=15     ; เปลี่ยนขา CS
-DTFT_DC=2      ; เปลี่ยนขา DC
-DTFT_RST=4     ; เปลี่ยนขา RST
-DTFT_BL=32     ; เปลี่ยนขา Backlight
```

## 🔧 Troubleshooting

### หน้าจอไม่แสดงผล

1. ตรวจสอบการต่อสายให้ถูกต้อง
2. ตรวจสอบว่า VCC ได้ 3.3V (ไม่ใช่ 5V)
3. ลอง Reset บอร์ด
4. ตรวจสอบ Serial Monitor ว่ามี error อะไร

### ปุ่ม BOOT ไม่สลับ API Key

1. ตรวจสอบว่า `BUTTON_PIN 0` ตรงกับปุ่ม BOOT บนบอร์ด
2. เพิ่ม delay ของ debounce ใน `config.h` ถ้ากระตุก

### WiFi เชื่อมต่อไม่ได้

1. ตรวจสอบ SSID และ Password ใน config.h
2. ตรวจสอบว่า WiFi เป็น 2.4GHz (ESP32 ไม่รองรับ 5GHz)
3. ลองเอาบอร์ดไปใกล้ Router

### API ไม่ทำงาน

1. ตรวจสอบว่า API Key ถูกต้อง
2. ตรวจสอบว่ามีอินเทอร์เน็ต
3. ดู Serial Monitor เพื่อดู error message
4. ลองเรียก API ด้วย Postman หรือ curl ก่อน

### Serial Monitor แสดงอักขระแปลกๆ

ตรวจสอบ baud rate ให้เป็น 115200

## 📊 API Response Format

OpenRouter API จะส่งข้อมูลกลับมาในรูปแบบ JSON:

```json
{
  "data": {
    "usage": 23.45,
    "limit": 150.00,
    "is_free_tier": false,
    "rate_limit": {
      "requests": 200
    }
  }
}
```

## 🔐 Security Notes

- ⚠️ **อย่า commit API Keys** เข้า Git repository
- เก็บ API Keys ไว้ในที่ปลอดภัย
- ใช้ `.gitignore` เพื่อป้องกันไฟล์ config

## 📄 License

MIT License - ใช้งานได้อย่างอิสระ

## 🤝 Contributing

Pull requests are welcome! สำหรับการเปลี่ยนแปลงใหญ่ๆ กรุณาเปิด issue ก่อน

## 📞 Support

หากมีปัญหาหรือคำถาม:
- เปิด Issue บน GitHub
- ดู Serial Monitor output เพื่อ debug

## 🎉 Credits

- TFT_eSPI Library by Bodmer
- ArduinoJson by Benoit Blanchon
- OpenRouter API

---

**สนุกกับการใช้งาน! 🚀**
