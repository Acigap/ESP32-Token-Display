# Claude.ai Usage POC — Notes & Plan

## สรุปผล POC

### ข้อมูลที่ต้องการ
```
Session (5hr) : 100%   resets in 3h
Weekly (7day) :   7%   resets in 1d
```

### Endpoint ที่ใช้ได้
```
GET https://claude.ai/api/organizations/{org_id}/usage
```
Response:
```json
{
  "five_hour": { "utilization": 100.0, "resets_at": "2026-05-18T22:50:00Z" },
  "seven_day":  { "utilization": 7.0,   "resets_at": "2026-05-20T01:00:00Z" }
}
```
org_id ดูได้จาก cookie `lastActiveOrg` หรือ `/api/organizations`

### ทำไม ESP32 call claude.ai โดยตรงไม่ได้
Cloudflare ตรวจ 3 ชั้น:
1. **TLS JA3 fingerprint** — ESP32 ใช้ mbedTLS ≠ Chrome BoringSSL → connection dropped
2. **JavaScript challenge** — ESP32 ไม่มี JS engine
3. **cf_clearance ผูกกับ fingerprint** — เอา cookie จาก browser มาใส่ ESP32 ก็ไม่ผ่าน

### วิธีที่ทำงานได้ (Relay)
```
ESP32  →  http://PC_IP:8765/usage  →  claude.ai
```
Python + `curl_cffi` (BoringSSL) impersonate Chrome → Cloudflare ผ่าน

### ไฟล์ใน POC
| ไฟล์ | ความหมาย |
|------|----------|
| `poc_claudeai_usage.py` | ทดสอบ endpoints ทั้งหมด |
| `poc_claudeai_relay.py` | Relay server สำหรับ ESP32 |
| `poc_claudeai_usage.ps1` | เวอร์ชัน PowerShell (ใช้ไม่ได้เพราะ Cloudflare) |
| `.env` | credentials (ไม่ commit) |

### Cookies ที่ต้องการ
| Cookie | อายุ | วิธีได้ |
|--------|------|---------|
| `sessionKey` | ~1 เดือน | F12 → Application → Cookies |
| `cf_clearance` | ~1 ชั่วโมง | F12 → Application → Cookies |
| `__cf_bm` | ~30 นาที | F12 → Application → Cookies |
| `lastActiveOrg` | ~1 ปี | F12 หรือ hardcode org UUID |

> `cf_clearance` และ `__cf_bm` หมดไวมาก — ต้องอัปเดต `.env` บ่อยจนกว่าจะมี auto-refresh

---

## Plan — งานที่ต้องทำต่อ

### Phase 1 — Relay Server (ทำได้เลย)
- [ ] เพิ่ม `RELAY_PORT` และ PC IP ใน `.env`
- [ ] ทดสอบ ESP32 call `http://192.168.0.63:8765/usage` ได้จริง
- [ ] เพิ่ม `ClaudeAIAPI.h` ใน ESP32 สำหรับ call relay (HTTP ไม่ใช่ HTTPS)
- [ ] เพิ่ม screen ใหม่ใน `DisplayUI.h` สำหรับแสดง Session/Weekly usage
- [ ] ใส่ relay URL ใน `config.h` (`#define CLAUDEAI_RELAY_URL "http://192.168.0.63:8765"`)

### Phase 2 — Auto Cookie Refresh (ยากขึ้น)
- [ ] ทำ script login อัตโนมัติด้วย Playwright เพื่อ refresh `cf_clearance` ทุกชั่วโมง
- [ ] Relay server อ่าน cookie จาก Playwright session แทนการใช้ .env

### Phase 3 — Standalone (optional)
- [ ] ย้าย relay ไปรันบน Raspberry Pi / NAS แทน PC
- [ ] เพิ่ม endpoint `/health` สำหรับตรวจว่า relay ยัง alive

---

## ทดสอบ Relay ตอนนี้
```powershell
# terminal 1 — รัน relay
python poc_claudeai_relay.py

# terminal 2 — ทดสอบ
Invoke-WebRequest http://localhost:8765/usage | Select -Expand Content
```
