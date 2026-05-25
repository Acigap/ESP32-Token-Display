# 🚀 Quick Start - Deploy Claude Relay บน Raspberry Pi

คู่มือฉบับย่อสำหรับการติดตั้งและใช้งานอย่างรวดเร็ว

## ⚡ เริ่มต้นอย่างรวดเร็ว (5 นาที)

### 1️⃣ เตรียม Credentials

เข้า https://claude.ai → เปิด DevTools (F12) → Application → Cookies → คัดลอก:
- `sessionKey`
- `lastActiveOrg`

### 2️⃣ แก้ไข .env

```bash
cd D:\git\ESP32-Token-Display\server
copy .env.example .env
notepad .env
```

วางค่า credentials ที่คัดลอกมา:
```env
CLAUDEAI_SESSION=sk-ant-sid01-xxxxx...
LASTACTIVE_ORG=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
RELAY_PORT=8765
RPI_HOST=pi@YOUR_RPI_IP
RPI_DIR=/home/pi/claude-relay
```

### 3️⃣ Deploy!

**จาก PowerShell:**
```powershell
cd D:\git\ESP32-Token-Display\server
.\deploy-to-rpi.ps1
```

**จาก Git Bash/WSL:**
```bash
cd /d/git/ESP32-Token-Display/server
bash deploy-to-rpi.sh
```

### 4️⃣ ตั้งค่า sudo (ครั้งเดียว)

SSH เข้า RPI:
```bash
ssh pi@YOUR_RPI_IP
sudo visudo
```

เพิ่มท้ายไฟล์:
```
pi ALL=(ALL) NOPASSWD: /bin/systemctl start claude-relay.service
pi ALL=(ALL) NOPASSWD: /bin/systemctl stop claude-relay.service
pi ALL=(ALL) NOPASSWD: /bin/systemctl restart claude-relay.service
pi ALL=(ALL) NOPASSWD: /bin/systemctl status claude-relay.service
```

บันทึก: `Ctrl+X` → `Y` → `Enter`

### 5️⃣ เปิดใช้งาน!

เปิดเว็บเบราว์เซอร์:
```
http://YOUR_RPI_IP:8765
```

เสร็จแล้ว! 🎉

---

## 📱 ใช้งานกับ ESP32

เปลี่ยน URL ใน ESP32 code:
```cpp
const char* USAGE_URL = "http://YOUR_RPI_IP:8765/usage";
```

---

## 🔧 คำสั่งที่ใช้บ่อย

```bash
# รีสตาร์ท service (จาก Windows)
ssh pi@YOUR_RPI_IP "sudo systemctl restart claude-relay.service"

# ดู logs
ssh pi@YOUR_RPI_IP "sudo journalctl -u claude-relay.service -f"

# ตรวจสอบสถานะ
ssh pi@YOUR_RPI_IP "sudo systemctl status claude-relay.service"
```

หรือใช้ Web Interface ที่ `http://YOUR_RPI_IP:8765` ได้เลย! 😊

---

## 🆘 แก้ปัญหา

**Service ไม่ทำงาน?**
```bash
ssh pi@YOUR_RPI_IP
cd /home/pi/claude-relay
python3 relay.py
# ดู error message
```

**เปลี่ยน IP?**
แก้ไข `RPI_HOST` ในไฟล์ `.env` แล้วรัน deploy ใหม่

**อัพเดทโค้ด?**
รันคำสั่ง deploy ใหม่อีกครั้ง

---

## 📚 เอกสารเพิ่มเติม

ดูรายละเอียดเต็มใน [DEPLOY.md](./DEPLOY.md)

---

**Happy Coding! 🎯**
