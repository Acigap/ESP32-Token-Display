# � Hardware Reference - ESP32 + ST7789 (All-in-One Board)

> **บอร์ดนี้มีจอ ST7789 1.9" ฝังในตัวอยู่แล้ว — ไม่ต้องต่อสายหน้าจอเพิ่ม**
> เสียบแค่สาย USB Type-C แล้ว upload โค้ดได้เลย

## สเปคบอร์ด

| รายการ | รายละเอียด |
|--------|------------|
| MCU | ESP32 (Xtensa LX6 dual-core 240 MHz) |
| Flash | 16 MB |
| RAM | 520 KB SRAM |
| จอแสดงผล | ST7789 TFT 1.9" 170×320 (built-in) |
| Resolution | 320×170 pixels (landscape mode) |
| ไร้สาย | WiFi 802.11 b/g/n (2.4 GHz) + Bluetooth 4.2 |
| USB Driver | CH340 |
| พอร์ต | USB Type-C |
| ปุ่มบนบอร์ด | EN (Reset) + BOOT (GPIO 0) |
| แรงดันไฟ | 5V via USB / 3.3V logic |

## GPIO ภายในที่จอใช้

จอ ST7789 เชื่อมต่อกับ ESP32 ภายในบอร์ดผ่าน SPI ดังนี้:

| Signal | GPIO | หมายเหตุ |
|--------|------|----------|
| MOSI   | 23   | SPI Data |
| SCLK   | 18   | SPI Clock |
| CS     | 15   | Chip Select |
| DC     | 2    | Data/Command |
| RST    | 4    | Reset |
| BL     | 32   | Backlight (PWM) |

**ปุ่มบนบอร์ด:**
| ปุ่ม | GPIO | การใช้งาน |
|------|------|-----------|
| EN   | —    | Reset บอร์ด |
| BOOT | 0    | กดเพื่อสลับ API Key (active LOW) |

## ✅ สิ่งที่ต้องทำ (Checklist)

เนื่องจากเป็น all-in-one board ไม่ต้องต่อสายจอ ทำแค่:

- [ ] เสียบสาย **USB Type-C** เข้ากับคอมพิวเตอร์
- [ ] ตรวจสอบว่า Windows เห็น COM Port (ต้องติดตั้ง CH340 driver ก่อนถ้ายังไม่มี)
- [ ] ตั้งค่า `config.h` (WiFi + API Keys)
- [ ] Upload โค้ดผ่าน PlatformIO

## 💡 ข้อควรรู้

1. **ไม่มี Touch Screen** — ใช้ปุ่ม BOOT (GPIO 0) บนบอร์ดเพื่อสลับ API Key
2. **CH340 Driver** — ถ้า Windows ไม่เห็น COM Port ให้ดาวน์โหลด CH340 driver ติดตั้งก่อน
3. **USB Type-C** — ใช้สายที่รองรับ data (ไม่ใช่แค่ชาร์จ)
4. **Flash 16MB** — พื้นที่โค้ดเยอะมาก ไม่ต้องกังวลเรื่องขนาด

## 🎯 ผลลัพธ์ที่คาดหวัง

เมื่อ upload โค้ดสำเร็จ:

1. ✅ หน้าจอแสดง "Connecting WiFi..."
2. ✅ เชื่อมต่อ WiFi สำเร็จ แสดง IP address
3. ✅ แสดง Credits/Usage บนหน้าจอ landscape
4. ✅ กดปุ่ม BOOT เพื่อสลับ API Key

---

**ขอให้สนุกกับการสร้าง! 🎉**

