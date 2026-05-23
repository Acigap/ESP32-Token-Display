# CLAUDE.md — TokenDisplay Project Context

## Project Overview

**ESP32 Token Display** — ESP32 all-in-one board (built-in ST7789 1.9" TFT) that shows AI API credit/token usage on screen. Supports OpenRouter, Anthropic, and Claude.ai session stats.

The user presses the **BOOT button (GPIO 0)** to cycle through API keys on-device.

---

## Architecture

```
┌─────────────────────┐      HTTP      ┌──────────────────────┐
│  ESP32 Firmware     │ ──────────── ▶ │  OpenRouter / Anthropic│
│  (Arduino/PlatformIO│               │  Claude.ai relay (PC)  │
│   TFT_eSPI display) │               └──────────────────────┘
└─────────────────────┘
         ▲
         │ USB (flash/monitor)
┌─────────────────────┐
│  VS Code Extension  │  writes config.h, server/.env, platformio.ini
│  (TypeScript)       │  runs PlatformIO build/flash
│  esp32-token-display│  manages relay.py lifecycle
└─────────────────────┘
```

---

## Directory Structure

```
TokenDisply/
├── src/main.cpp              # Firmware entry point (Arduino setup/loop)
├── include/
│   ├── config.h              # ⚠️ SECRETS — never commit (in .gitignore)
│   ├── config.h.example      # Template — commit this one
│   ├── DisplayUI.h           # TFT layout constants & drawing functions
│   ├── OpenRouterAPI.h       # HTTP call → openrouter.ai/api/v1/auth/key
│   ├── AnthropicAPI.h        # HTTP call → api.anthropic.com usage
│   └── RelayAPI.h            # HTTP call → PC relay server /usage
├── server/
│   ├── relay.py              # Flask-like HTTP server proxying claude.ai
│   └── .env                  # ⚠️ SECRETS — CLAUDEAI_SESSION, LASTACTIVE_ORG
├── platformio.ini            # Board config, build flags, lib deps
├── vscode-extension/
│   ├── src/
│   │   ├── extension.ts      # Registers VS Code commands, activates panel
│   │   ├── ConfigPanel.ts    # Webview with all config tabs (HTML inlined)
│   │   ├── ConfigManager.ts  # Read/write config.h + server/.env + platformio.ini
│   │   ├── RelayManager.ts   # Spawns/kills relay.py child process
│   │   └── PortDetector.ts   # Detects COM ports via pio / pyserial / PowerShell
│   ├── package.json
│   └── out/extension.js      # Compiled bundle (never edit directly)
└── .github/skills/           # Copilot skill docs (one SKILL.md per topic)
```

---

## Hardware

| Property | Value |
|----------|-------|
| MCU | ESP32-WROOM-32 (dual-core 240 MHz) |
| Display | ST7789 1.9" 170×320 (built-in, landscape → 320×170) |
| Flash / RAM | 16 MB / 520 KB |
| USB | Type-C, CH340 driver |
| Button | BOOT = GPIO 0, active LOW → cycles API key |

**SPI pins (internal, already set in `platformio.ini` build_flags):**
MOSI=23 SCLK=18 CS=15 DC=2 RST=4 BL=32

---

## Config Files

Three files are written by `ConfigManager.save()`:

| File | Fields |
|------|--------|
| `include/config.h` | WIFI_SSID, WIFI_PASSWORD, API_KEYS[], ANTHROPIC_*, OPENROUTER_API_URL, RELAY_HOST, RELAY_PORT, UPDATE_INTERVAL |
| `server/.env` | CLAUDEAI_SESSION, LASTACTIVE_ORG, RELAY_PORT |
| `platformio.ini` | upload_port, monitor_port, upload_speed (patched in-place) |

**Both `include/config.h` and `server/.env` are in `.gitignore` — never commit them.**

To bootstrap a fresh clone:
```powershell
cp include/config.h.example include/config.h
# fill in real values, or use the VS Code extension GUI
```

---

## Firmware Build Commands

```powershell
# Compile only
pio run -e esp32dev

# Compile + flash (ESP32 on configured COM port)
pio run -e esp32dev --target upload

# Serial monitor
pio device monitor -p COM6 -b 115200 --filter esp32_exception_decoder

# List detected serial ports
pio device list --serial
```

If flash fails: hold **BOOT** → press **EN** → release **BOOT** → retry.

---

## VS Code Extension Commands

```powershell
cd vscode-extension

# Dev build (produces out/extension.js with source map)
npm run build

# Watch mode (auto-rebuild on save)
npm run watch

# Package → .vsix (runs minified build first)
npm run package

# Install into VS Code
code --install-extension esp32-token-display-1.0.0.vsix --force
# Then: Ctrl+Shift+P → Developer: Reload Window
```

---

## Extension Tab Map

| Tab label | `data-tab` | What it configures |
|-----------|-----------|-------------------|
| 🏠 Home | `home` | Getting-started guide |
| 📡 Device | `device` | WiFi, COM port, upload speed, update interval |
| 🔑 API Keys | `apikeys` | Add/remove OpenRouter & Anthropic keys |
| 🤖 AI Providers | `anthropic` | Anthropic model/URL/version; OpenRouter URL |
| ☁️ Claude.ai | `claudeai` | Session key, Org ID, Relay PC IP/port |
| ⚡ Actions | `actions` | Build, Build & Flash, Serial Monitor, Relay start/stop |

---

## Claude.ai Relay Server

The relay proxies Claude.ai usage stats so the ESP32 can display them.

```powershell
# Install deps (once)
pip install curl_cffi flask python-dotenv

# Run manually
cd server
python relay.py
# → listens on http://<PC_LAN_IP>:8765
# → ESP32 calls GET /usage
```

`server/.env` must contain:
```
CLAUDEAI_SESSION=sk-ant-sid01-...
LASTACTIVE_ORG=<UUID from claude.ai cookies>
RELAY_PORT=8765
```

**Getting session cookies:**
1. Open claude.ai in Edge/Chrome (logged in)
2. F12 → Application → Cookies → `.claude.ai`
3. Copy `sessionKey` and `lastActiveOrg`

---

## Secrets Policy

| Secret | File | In git? |
|--------|------|---------|
| WiFi password | `include/config.h` | ❌ NO |
| API keys | `include/config.h` | ❌ NO |
| Claude.ai session | `server/.env` | ❌ NO |
| Template values | `include/config.h.example` | ✅ YES |

Install the pre-commit hook to prevent accidental leaks:
```bash
bash scripts/install-hooks.sh
```

---

## Display UI Layout (320×170 landscape)

```
┌──────────────────────────────┐  y=0
│  HEADER  28px  (navy bg)     │  provider name + key name (yellow)
├──────────────────────────────┤  y=28
│                              │
│  CREDITS / USAGE  ~84px      │  large number (font4/6), tokens below
│                              │
├──────────────────────────────┤  y≈122
│  PROGRESS BAR  18px          │  color: green→yellow→red by % used
├──────────────────────────────┤  y≈140
│  FOOTER  24px                │  timestamp, last-update, WiFi status
└──────────────────────────────┘  y=170
```

Constants: `HEADER_H=28`, `FOOTER_H=24`, `BAR_H=18` in `include/DisplayUI.h`.

---

## Common Tasks

### Add a new API key (GUI)
Extension panel → 🔑 API Keys → **+ Add Key** → fill name, key, type → 💾 Save Config

### Add a new API key (manual)
Edit `include/config.h`:
```cpp
const APIKeyConfig API_KEYS[] = {
    {"Personal", "sk-or-v1-...", false},   // OpenRouter
    {"Work",     "sk-ant-...",   true},    // Anthropic
};
```

### Change COM port
Extension panel → 📡 Device → press ↺ to auto-detect → select port → 💾 Save Config

### Rebuild extension after source changes
```powershell
cd vscode-extension
npm run package
code --install-extension esp32-token-display-1.0.0.vsix --force
# Ctrl+Shift+P → Developer: Reload Window
```

### Bump extension version
Edit `vscode-extension/package.json` → `"version"` field → rebuild + reinstall.

---

## GitHub Actions

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `.github/workflows/build.yml` | push main/develop, any PR | Compile firmware |
| `.github/workflows/release.yml` | push tag `v*.*.*` | Compile + GitHub Release |

Both workflows copy `config.h.example → config.h` before building.
