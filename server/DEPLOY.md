# Claude Relay Server - Deployment Guide

คู่มือการติดตั้งและใช้งาน Claude Relay Server บน Raspberry Pi

## 📋 ข้อกำหนดเบื้องต้น

- Raspberry Pi (ทดสอบบน Raspberry Pi OS)
- Python 3.7+
- สิทธิ์ sudo
- เชื่อมต่ออินเทอร์เน็ต
- Claude.ai session key และ organization ID

## 🚀 วิธีการติดตั้ง

### ขั้นตอนที่ 1: เตรียมข้อมูล Credentials

1. เข้า https://claude.ai และ login
2. เปิด Developer Tools (F12) → Application/Storage → Cookies
3. คัดลอก `sessionKey` และ `lastActiveOrg`
4. แก้ไขไฟล์ `.env` ในโฟลเดอร์ `server/`:

```bash
CLAUDEAI_SESSION=sk-ant-sid01-xxxxx...
LASTACTIVE_ORG=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
RELAY_PORT=8765
RPI_HOST=pi@YOUR_RPI_IP_ADDRESS
RPI_DIR=/home/pi/claude-relay
```

### ขั้นตอนที่ 2: Deploy ไปยัง Raspberry Pi

จาก Windows PowerShell:

```powershell
cd D:\git\ESP32-Token-Display\server

# Deploy (ใช้ Git Bash หรือ WSL)
bash deploy-to-rpi.sh
```

หรือทำแบบ manual:

```bash
# 1. Copy ไฟล์ไปยัง RPI
scp relay.py pi@YOUR_RPI_IP:/home/pi/claude-relay/
scp claude-relay.service pi@YOUR_RPI_IP:/home/pi/claude-relay/
scp requirements.txt pi@YOUR_RPI_IP:/home/pi/claude-relay/
scp web-interface.html pi@YOUR_RPI_IP:/home/pi/claude-relay/index.html
scp .env pi@YOUR_RPI_IP:/home/pi/claude-relay/

# 2. SSH เข้า RPI
ssh pi@YOUR_RPI_IP

# 3. ติดตั้ง dependencies
cd /home/pi/claude-relay
pip3 install --user curl_cffi flask python-dotenv

# 4. ติดตั้ง systemd service
sudo cp claude-relay.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable claude-relay.service
sudo systemctl start claude-relay.service

# 5. ตรวจสอบสถานะ
sudo systemctl status claude-relay.service
```

### ขั้นตอนที่ 3: ตั้งค่า sudo ให้ไม่ต้องใส่ password (สำหรับ web control)

```bash
# SSH เข้า RPI
ssh YOUR_RPI_USER@YOUR_RPI_IP

# เพิ่มสิทธิ์ sudo สำหรับคำสั่ง systemctl
sudo visudo

# เพิ่มบรรทัดนี้ท้ายไฟล์:
pi ALL=(ALL) NOPASSWD: /bin/systemctl start claude-relay.service
pi ALL=(ALL) NOPASSWD: /bin/systemctl stop claude-relay.service
pi ALL=(ALL) NOPASSWD: /bin/systemctl restart claude-relay.service
pi ALL=(ALL) NOPASSWD: /bin/systemctl status claude-relay.service

# บันทึก: Ctrl+X, Y, Enter
```

## 🌐 การเข้าใช้งาน

### Web Interface

เปิดเว็บเบราว์เซอร์:
```
http://YOUR_RPI_IP:8765
```

ฟีเจอร์:
- 📊 แสดงสถิติ Claude.ai usage (Session & Weekly)
- ⚙️ จัดการ service (Start, Stop, Restart)
- 📝 ดู logs แบบ real-time
- 🔄 Auto-refresh ทุก 30 วินาที

### API Endpoints

#### GET /usage
ดึงข้อมูล usage จาก Claude.ai
```bash
curl http://YOUR_RPI_IP:8765/usage
```

Response:
```json
{
  "ok": true,
  "session_pct": 45.2,
  "session_resets_in": "2h 15m",
  "weekly_pct": 12.8,
  "weekly_resets_in": "3d 4h"
}
```

#### GET /service/status
ตรวจสอบสถานะ service
```bash
curl http://YOUR_RPI_IP:8765/service/status
```

#### POST /service/start
เริ่ม service
```bash
curl -X POST http://YOUR_RPI_IP:8765/service/start
```

#### POST /service/stop
หยุด service
```bash
curl -X POST http://YOUR_RPI_IP:8765/service/stop
```

#### POST /service/restart
รีสตาร์ท service
```bash
curl -X POST http://YOUR_RPI_IP:8765/service/restart
```

#### GET /service/logs
ดู logs ล่าสุด 50 บรรทัด
```bash
curl http://YOUR_RPI_IP:8765/service/logs
```

## 🔧 การจัดการ Service

### คำสั่งพื้นฐาน

```bash
# เริ่ม service
sudo systemctl start claude-relay.service

# หยุด service
sudo systemctl stop claude-relay.service

# รีสตาร์ท service
sudo systemctl restart claude-relay.service

# ดูสถานะ
sudo systemctl status claude-relay.service

# ดู logs
sudo journalctl -u claude-relay.service -f

# ดู logs ล่าสุด 100 บรรทัด
sudo journalctl -u claude-relay.service -n 100

# เปิดใช้งาน auto-start ตอน boot
sudo systemctl enable claude-relay.service

# ปิด auto-start
sudo systemctl disable claude-relay.service
```

### แก้ไขการตั้งค่า

```bash
# แก้ไข .env
nano /home/pi/claude-relay/.env

# รีสตาร์ท service
sudo systemctl restart claude-relay.service
```

### อัพเดทโค้ด

```bash
# จาก Windows
cd D:\git\ESP32-Token-Display\server
bash deploy-to-rpi.sh

# หรือ copy ไฟล์เดียว
scp relay.py YOUR_RPI_USER@YOUR_RPI_IP:/home/pi/claude-relay/

# SSH เข้า RPI และรีสตาร์ท
ssh YOUR_RPI_USER@YOUR_RPI_IP
sudo systemctl restart claude-relay.service
```

## 📱 การใช้งานกับ ESP32

แก้ไขโค้ด ESP32 ให้เรียก API:

```cpp
// ใน DisplayUI.h หรือ main.cpp
const char* RELAY_SERVER = "http://YOUR_RPI_IP:8765";

void fetchUsage() {
  HTTPClient http;
  http.begin(RELAY_SERVER "/usage");
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    // Parse JSON และแสดงผล
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    float session_pct = doc["session_pct"];
    float weekly_pct = doc["weekly_pct"];
    const char* session_reset = doc["session_resets_in"];
    const char* weekly_reset = doc["weekly_resets_in"];
    
    // อัพเดท UI
  }
  http.end();
}
```

## 🔍 Troubleshooting

### Service ไม่เริ่มทำงาน

```bash
# ตรวจสอบ logs
sudo journalctl -u claude-relay.service -n 50

# ตรวจสอบว่า Python dependencies ติดตั้งครบ
pip3 list | grep -E 'curl_cffi|flask|python-dotenv'

# ทดสอบรันแบบ manual
cd /home/pi/claude-relay
python3 relay.py
```

### Web interface ไม่เปิด

```bash
# ตรวจสอบว่า port 8765 ถูกใช้งานหรือไม่
sudo netstat -tulpn | grep 8765

# ตรวจสอบ firewall (ถ้ามี)
sudo ufw status
sudo ufw allow 8765/tcp
```

### ไม่สามารถควบคุม service จาก web

```bash
# ตรวจสอบ sudo permissions
sudo -l

# ตั้งค่า sudo ใหม่ (ดูขั้นตอนที่ 3)
sudo visudo
```

### Connection timeout

```bash
# ตรวจสอบว่า .env มี credentials ถูกต้อง
cat /home/pi/claude-relay/.env

# ทดสอบ curl ไปยัง Claude.ai
curl -v https://claude.ai/api/organizations/YOUR_ORG_ID/usage
```

## 📊 Monitoring

### ดูการใช้งาน CPU/Memory

```bash
# แบบ real-time
top

# กรอง process
ps aux | grep relay.py

# ดูการใช้งาน systemd service
sudo systemctl status claude-relay.service
```

### ตั้งค่า Log Rotation

```bash
# สร้างไฟล์ logrotate config
sudo nano /etc/logrotate.d/claude-relay

# เพิ่มเนื้อหา:
/home/pi/claude-relay/relay.log {
    daily
    rotate 7
    compress
    missingok
    notifempty
    create 0644 pi pi
}
```

## 🔒 Security Tips

1. **เปลี่ยน Port**: แก้ `RELAY_PORT` ใน `.env`
2. **Firewall**: จำกัดการเข้าถึง port 8765 เฉพาะ LAN
3. **HTTPS**: ติดตั้ง reverse proxy (nginx) พร้อม SSL
4. **Authentication**: เพิ่ม basic auth สำหรับ web interface

## 📝 ไฟล์ที่เกี่ยวข้อง

```
server/
├── relay.py                    # Main server code
├── claude-relay.service        # Systemd service file
├── web-interface.html          # Web dashboard
├── requirements.txt            # Python dependencies
├── .env                        # Configuration (ไม่ commit)
├── .env.example               # Example config
├── deploy.sh                  # Deployment script
├── deploy-to-rpi.sh           # Quick deploy from Windows
└── DEPLOY.md                  # This file
```

## 🎯 ตัวอย่างการใช้งาน

### Scenario 1: ตรวจสอบ usage จาก browser

1. เปิด `http://YOUR_RPI_IP:8765`
2. ดูสถิติ Session & Weekly usage
3. รีเฟรชอัตโนมัติทุก 30 วินาที

### Scenario 2: รีสตาร์ท service จาก web

1. เปิด web interface
2. คลิกปุ่ม "Restart"
3. รอ 1-2 วินาที
4. ดูสถานะอัพเดทเป็น "ACTIVE"

### Scenario 3: ดู logs แบบ real-time

```bash
# จาก SSH
sudo journalctl -u claude-relay.service -f

# หรือจาก web interface → Service Logs section
```

## 🆘 Support

หากพบปัญหา:
1. ตรวจสอบ logs: `sudo journalctl -u claude-relay.service -n 100`
2. ตรวจสอบ .env credentials
3. ทดสอบรันแบบ manual: `python3 relay.py`
4. ตรวจสอบ network connectivity

---

✅ **ตอนนี้ระบบพร้อมใช้งานแล้ว!**

ESP32 สามารถเรียก `http://YOUR_RPI_IP:8765/usage` เพื่อดึงข้อมูล Claude usage  
และคุณสามารถจัดการ service ผ่าน web interface ได้สะดวก!
