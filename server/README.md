# Claude Relay Server

🚀 Relay server สำหรับดึงข้อมูล Claude.ai usage และแสดงผลบน ESP32

## ✨ Features

- 📊 **Real-time Usage Monitoring**: ดูข้อมูล Session และ Weekly usage จาก Claude.ai
- 🌐 **Web Dashboard**: หน้า web สำหรับดูสถานะและจัดการ service
- ⚙️ **Service Management**: เปิด/ปิด/รีสตาร์ท service ผ่าน web interface
- 📝 **Live Logs**: ดู logs แบบ real-time
- 🔄 **Auto-refresh**: อัพเดทข้อมูลอัตโนมัติทุก 30 วินาที
- 🚀 **ESP32 Compatible**: API endpoint สำหรับ ESP32 เรียกใช้งาน
- 💾 **Systemd Service**: รันเป็น background service บน Raspberry Pi
- 🔒 **Caching**: ลด API calls ไปยัง Claude.ai

## 🎯 Quick Start

ดูคู่มือฉบับย่อ → [QUICKSTART.md](./QUICKSTART.md)

## 📋 Files

```
server/
├── relay.py                    # Main relay server (Python)
├── claude-relay.service        # Systemd service configuration
├── web-interface.html          # Web dashboard UI
├── requirements.txt            # Python dependencies
├── .env.example               # Configuration template
├── deploy-to-rpi.ps1          # PowerShell deployment script
├── deploy-to-rpi.sh           # Bash deployment script
├── deploy.sh                  # On-RPI deployment script
├── DEPLOY.md                  # Full deployment documentation
├── QUICKSTART.md              # Quick start guide (5 min)
└── README.md                  # This file
```

## 🚀 Installation

### Prerequisites

- Raspberry Pi (Debian/Ubuntu based)
- Python 3.7+
- Claude.ai account with session key

### 1. Get Credentials

1. Go to https://claude.ai and login
2. Open DevTools (F12) → Application → Cookies
3. Copy `sessionKey` and `lastActiveOrg`

### 2. Configure

```bash
cd server/
cp .env.example .env
nano .env
```

Add your credentials:
```env
CLAUDEAI_SESSION=sk-ant-sid01-xxxxx...
LASTACTIVE_ORG=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
RELAY_PORT=8765
RPI_HOST=pi@YOUR_RPI_IP
RPI_DIR=/home/pi/claude-relay
```

### 3. Deploy

**From Windows (PowerShell):**
```powershell
.\deploy-to-rpi.ps1
```

**From Linux/Mac:**
```bash
bash deploy-to-rpi.sh
```

## 🌐 Usage

### Web Interface

Open browser: `http://YOUR_RPI_IP:8765`

Features:
- View Claude.ai usage statistics
- Control service (start/stop/restart)
- View logs in real-time
- Auto-refresh every 30 seconds

### API Endpoints

#### `GET /usage`
Get Claude.ai usage data
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

#### `GET /service/status`
Get service status

#### `POST /service/start`
Start the service

#### `POST /service/stop`
Stop the service

#### `POST /service/restart`
Restart the service

#### `GET /service/logs`
Get last 50 lines of logs

## 🔧 Service Management

```bash
# Start service
sudo systemctl start claude-relay.service

# Stop service
sudo systemctl stop claude-relay.service

# Restart service
sudo systemctl restart claude-relay.service

# View status
sudo systemctl status claude-relay.service

# View logs
sudo journalctl -u claude-relay.service -f

# Enable auto-start on boot
sudo systemctl enable claude-relay.service
```

## 📱 ESP32 Integration

In your ESP32 code:

```cpp
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* RELAY_SERVER = "http://YOUR_RPI_IP:8765";

void fetchClaudeUsage() {
  HTTPClient http;
  http.begin(String(RELAY_SERVER) + "/usage");
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    if (doc["ok"]) {
      float sessionPct = doc["session_pct"];
      float weeklyPct = doc["weekly_pct"];
      const char* sessionReset = doc["session_resets_in"];
      const char* weeklyReset = doc["weekly_resets_in"];
      
      // Update your display
      Serial.printf("Session: %.1f%% (resets in %s)\n", sessionPct, sessionReset);
      Serial.printf("Weekly: %.1f%% (resets in %s)\n", weeklyPct, weeklyReset);
    }
  }
  http.end();
}
```

## 🔍 Troubleshooting

### Service not starting?

```bash
# Check logs
sudo journalctl -u claude-relay.service -n 50

# Test manually
cd /home/pi/claude-relay
python3 relay.py
```

### Can't control service from web?

Configure sudo permissions:
```bash
sudo visudo
```

Add:
```
pi ALL=(ALL) NOPASSWD: /bin/systemctl start claude-relay.service
pi ALL=(ALL) NOPASSWD: /bin/systemctl stop claude-relay.service
pi ALL=(ALL) NOPASSWD: /bin/systemctl restart claude-relay.service
pi ALL=(ALL) NOPASSWD: /bin/systemctl status claude-relay.service
```

### Update code?

Just run the deployment script again:
```powershell
.\deploy-to-rpi.ps1
```

## 📚 Documentation

- **Quick Start**: [QUICKSTART.md](./QUICKSTART.md) - Get started in 5 minutes
- **Full Guide**: [DEPLOY.md](./DEPLOY.md) - Complete deployment and usage documentation

## 🔒 Security

- Change default port in `.env`
- Restrict access to LAN only
- Consider adding authentication for production use
- Use HTTPS with reverse proxy (nginx)

## 📊 Architecture

```
ESP32/Browser → Relay Server → Claude.ai API
                     ↓
                Web Interface
                Service Control
                Log Viewer
```

## 🛠️ Technologies

- **Python 3**: Main server
- **curl_cffi**: HTTP requests with browser impersonation
- **systemd**: Service management
- **HTML/CSS/JavaScript**: Web interface

## 📝 License

See main project LICENSE

## 🤝 Contributing

Issues and pull requests welcome!

---

Made with ❤️ for ESP32-Token-Display project
