# ESP32 Token Display — VS Code Extension

Configure, build, and flash your **ESP32 Token Display** firmware directly from VS Code.

Monitor API usage from **OpenRouter**, **Anthropic**, and **Claude.ai** on an ESP32 with a built-in ST7789 1.9" TFT display.

---

## Features

- **Config Panel UI** — set WiFi, API keys, and Claude.ai session without editing any source files
- **Auto COM Port detection** — finds your ESP32 port automatically
- **Build & Flash** — runs PlatformIO `upload` in one click
- **Relay Server control** — start/stop the Python Claude.ai relay server from the sidebar
- **Multi-key support** — add multiple OpenRouter / Anthropic keys, cycle with BOOT button

---

## Requirements

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html) installed and in PATH
- Python 3.x with `flask`, `curl_cffi`, `python-dotenv` (for Claude.ai relay)
- ESP32 board with ST7789 1.9" TFT display (320×170)

---

## Getting Started

### 1. Open the Sidebar

Click the **ESP32 Token Display** icon in the Activity Bar (left sidebar).

### 2. Open Config Panel

Click **Open Config Panel** or run `ESP32 Token Display: Open Config` from the Command Palette (`Ctrl+Shift+P`).

### 3. Fill in Config Tabs

| Tab | What to set |
|-----|-------------|
| **Device** | WiFi SSID, Password, COM Port |
| **API Keys** | OpenRouter keys (`sk-or-v1-...`), Anthropic key (`sk-ant-api03-...`) |
| **Claude.ai** | Session key, Org ID, Relay host IP |

Click **💾 Save Config** — this writes `include/config.h` and `server/.env` automatically.

### 4. Build & Flash

Go to the **Actions** tab → click **⚡ Build & Flash**.

### 5. Start Relay (Claude.ai only)

In the Actions tab → click **▶ Start Relay** to run the local Python relay server.

---

## Display Pages

Press the **BOOT button (GPIO 0)** to cycle between pages:

| Page | Shows |
|------|-------|
| OpenRouter | Credits remaining, used, limit + progress bar |
| Anthropic | Token/request rate limits + reset timer |
| Claude.ai | Session (5h) + Weekly (7d) usage + reset countdown |

---

## Hardware

Any **ESP32 board with built-in ST7789 1.9" TFT** (170×320 pixels).

| Signal | GPIO |
|--------|------|
| MOSI | 23 |
| SCLK | 18 |
| CS | 15 |
| DC | 2 |
| RST | 4 |
| BL | 32 |

---

## Source & Issues

- GitHub: [Acigap/ESP32-Token-Display](https://github.com/Acigap/ESP32-Token-Display)
- Issues: [GitHub Issues](https://github.com/Acigap/ESP32-Token-Display/issues)

---

## Disclaimer

This extension stores credentials (API keys, session tokens) **locally on your machine only** and never transmits them to any server other than the official API endpoints you configure.

The Claude.ai relay feature uses a browser session key provided by the user to read usage data from Anthropic's web interface. Users are solely responsible for ensuring their use complies with [Anthropic's Terms of Service](https://www.anthropic.com/legal/consumer-terms), [OpenRouter's Terms](https://openrouter.ai/terms), and any other applicable third-party terms.

This project is not affiliated with, endorsed by, or sponsored by Anthropic, OpenRouter, or any third-party service.

---

## License

MIT
