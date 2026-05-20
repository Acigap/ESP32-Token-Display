# Quick Start

## 1. ติดตั้ง Extension

ใน VS Code: **Extensions** → `···` → **Install from VSIX**  
เลือก `vscode-extension/esp32-token-display-1.0.0.vsix`

---

## 2. เปิด Config Panel

`Ctrl+Shift+P` → **ESP32 Token Display: Open Config**

---

## 3. ตั้งค่า

### Tab: Device
- WiFi SSID / Password
- COM Port (กด ↺ ให้ detect อัตโนมัติ)

### Tab: API Keys
- เพิ่ม OpenRouter keys (`sk-or-v1-...`) — Type: OpenRouter
- เพิ่ม Anthropic key (`sk-ant-...`) — Type: Anthropic

### Tab: Claude.ai _(ถ้าต้องการ)_
- **Session Key**: เปิด claude.ai → F12 → Application → Cookies → `.claude.ai` → คัดลอก `sessionKey`
- **Org ID**: คัดลอก `lastActiveOrg` จากที่เดียวกัน
- **PC IP**: extension ตรวจหาให้อัตโนมัติ (กด 🔍 ถ้าต้องการ force detect)

### กด 💾 Save Config

---

## 4. Flash

**Tab: Actions** → กด **⚡ Build & Flash**

Build output จะแสดงในหน้า extension โดยตรง

---

## 5. Claude.ai Relay _(optional)_

ติดตั้ง dependencies ครั้งเดียว:
```bash
pip install curl_cffi flask python-dotenv
```

**Tab: Actions** → กด **▶ Start Relay**

ESP32 จะ switch หน้าจอไปแสดง Claude.ai usage อัตโนมัติเมื่อ relay เริ่มทำงาน

---

## BOOT Button

กดปุ่ม **BOOT** บนบอร์ดเพื่อวนหน้าจอ:

```
Key 1 → Key 2 → ... → Claude.ai → Key 1 → ...
```

---

## Checklist

- [ ] Extension ติดตั้งแล้ว
- [ ] WiFi SSID / Password ถูกต้อง
- [ ] API Keys ใส่แล้ว
- [ ] Save Config แล้ว
- [ ] Flash สำเร็จ
- [ ] หน้าจอแสดงผล
