# 🚀 Quick Start Guide

## ขั้นตอนการใช้งานแบบรวดเร็ว

### 1. เปิดโปรเจกต์ใน VS Code
```bash
code .
```

### 2. แก้ไขไฟล์ config.h

**สำคัญ!** ต้องตั้งค่า WiFi และ API Keys ก่อนใช้งาน

เปิดไฟล์: `include/config.h` (copy จาก `config.h.example` ถ้ายังไม่มี)

```cpp
// เปลี่ยนเป็น WiFi ของคุณ
#define WIFI_SSID "ชื่อWiFi"
#define WIFI_PASSWORD "รหัสผ่าน"

// เปลี่ยนเป็น API Keys จาก OpenRouter
const APIKeyConfig API_KEYS[] = {
    {"Personal", "sk-or-v1-xxxxx..."},  // ← ใส่ API Key จริง
    {"Work", "sk-or-v1-yyyyy..."},
};
```

### 3. เชื่อมต่อ ESP32-WROOM-32

1. เสียบสาย USB เข้ากับคอมพิวเตอร์
2. ตรวจสอบว่า Windows เห็น COM Port (Device Manager) — โค้ดนี้ใช้ **COM6**

### 4. Upload โค้ด

กดปุ่ม **Upload** ใน PlatformIO toolbar (ลูกศรขวา ➡️)

หรือใช้คำสั่ง:
```bash
pio run --target upload
```

### 5. ดู Serial Monitor

กดปุ่ม **Serial Monitor** ใน PlatformIO (ปล๊ัก 🔌)

หรือ:
```bash
pio device monitor
```

### 6. เริ่มใช้งาน! 🎉

หน้าจอจะแสดง:
- ✅ ชื่อ API Key บน header
- 💰 Credits, Usage, Limit
- 📊 Progress bar
- 🔄 กดปุ่ม BOOT เพื่อสลับ API Key

---

## 🔧 Hardware (All-in-One Board)

บอร์ดนี้มีจอ ST7789 1.9" ฝังในตัวอยู่แล้ว **ไม่ต้องต่อสายหน้าจอเพิ่ม**

| รายการ | รายละเอียด |
|--------|------------|
| MCU | ESP32 (dual-core 240 MHz) |
| Flash | 16 MB |
| จอ | ST7789 1.9" 170×320 (built-in) |
| USB | Type-C + CH340 driver |
| ปุ่ม BOOT | GPIO 0 — กดเพื่อสลับ API Key |

> ✅ เสียบแค่ **USB Type-C** แล้ว upload โค้ดได้เลย

---

## ✅ Checklist ก่อนใช้งาน

- [ ] ติดตั้ง PlatformIO แล้ว
- [ ] แก้ไข config.h (WiFi + API Keys)
- [ ] เสียบสาย USB Type-C แล้ว
- [ ] Upload โค้ดสำเร็จ
- [ ] เห็นหน้าจอแสดงผล

---

## 🆘 แก้ปัญหาเบื้องต้น

### หน้าจอไม่แสดงผล
✅ ตรวจสอบสายไฟ VCC (3.3V), GND
✅ ตรวจสอบขา BL (GPIO 32), CS, RST, DC

### WiFi เชื่อมไม่ได้
✅ ตรวจสอบ SSID และ Password
✅ WiFi ต้องเป็น 2.4GHz

### Upload ไม่ได้
✅ ตรวจสอบ COM Port ใน Device Manager
✅ ติดตั้ง CH340 driver ถ้ายังไม่มี
✅ ลองกดปุ่ม BOOT ค้างไว้ตอน upload
✅ ลองเปลี่ยน USB cable (ต้องรองรับ data)

---

## 📞 ต้องการความช่วยเหลือ?

1. เปิด Serial Monitor ดู error messages
2. อ่าน [README.md](README.md) เพิ่มเติม

---

**Happy Coding! 🎉**

