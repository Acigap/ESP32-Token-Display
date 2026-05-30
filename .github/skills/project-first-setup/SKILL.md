---
name: project-first-setup
description: >
  Complete guide for setting up the TokenDisplay project from scratch on a
  new machine. Use this when a new developer clones the repo, when setting
  up a fresh Windows PC, or when any step in the initial setup (config.h,
  extension install, PlatformIO, relay server) is missing or broken.
---

## Prerequisites

| Tool | Minimum version | Install |
|------|----------------|---------|
| VS Code | 1.85+ | https://code.visualstudio.com |
| PlatformIO IDE extension | latest | VS Code Extensions → search "PlatformIO IDE" |
| Python | 3.9+ | https://python.org (check "Add to PATH") |
| Node.js | 18+ | https://nodejs.org |

---

## Step 1 — Clone and open the workspace

```powershell
git clone https://github.com/Acigap/ESP32-Token-Display.git
cd ESP32-Token-Display
code TokenDisplay.code-workspace
```

---

## Step 2 — Create `include/config.h`

`config.h` is **never committed** (listed in `.gitignore`). Copy from the example:

```powershell
cp include/config.h.example include/config.h
```

Fill in real values (WiFi, API keys). You can also use the extension (Step 4) to do this with a GUI.

---

## Step 3 — Install the pre-commit hook _(optional but recommended)_

Prevents accidentally committing secrets:

```bash
bash scripts/install-hooks.sh
```

---

## Step 4 — Install the VS Code Extension

### Option A — Install pre-built .vsix (fastest)

```powershell
code --install-extension vscode-extension\esp32-token-display-1.0.0.vsix --force
```

Then reload VS Code (`Ctrl+Shift+P` → **Developer: Reload Window**).

### Option B — Build from source

```powershell
cd vscode-extension
npm install
npm run package
code --install-extension esp32-token-display-1.0.0.vsix --force
cd ..
```

### Verify installation

```powershell
code --list-extensions --show-versions | Select-String esp32
# Expected: gapcode.esp32-token-display@1.0.0
```

---

## Step 5 — Configure via the Extension Panel

Open the Config Panel: `Ctrl+Shift+P` → **ESP32 Token Display: Open Config**

| Tab | What to fill in |
|-----|----------------|
| **📡 Device** | Board env, WiFi SSID, Password, COM Port (press ↺), Upload Speed, Display Theme |
| **🔑 API Keys** | Add OpenRouter keys (`sk-or-v1-...`) and/or Anthropic keys (`sk-ant-...`) |
| **🤖 AI Providers** | Anthropic model, API version, base URL; OpenRouter API URL |
| **☁️ Claude.ai** | Session key + Org ID from browser cookies (see below) |

Press **💾 Save Config** — this writes `include/config.h`, `server/.env`, and patches `platformio.ini`.

### Choose the correct board env first

In **📡 Device** tab, set Board to match hardware:
- `esp32dev` for classic ESP32 1.9 inch board
- `esp32s3-touch-lcd-1_9` for Waveshare ESP32-S3 Touch LCD 1.9
- `lilygo-t-display-s3` for LilyGO T-Display-S3
- `ttgo-t-display` for TTGO T-Display

This affects build flags, display backend, input model, and upload behavior.

### Getting Claude.ai Session Key & Org ID

1. Open Edge/Chrome → go to `claude.ai` (logged in)
2. Press **F12** → **Application** tab → **Cookies** → `.claude.ai`
3. Copy `sessionKey` → paste into **Session Key**
4. Copy `lastActiveOrg` → paste into **Org ID**

---

## Step 6 — Build & Flash Firmware

### Via the Extension (recommended)

**Tab: Actions** → **⚡ Build & Flash**

Build output appears inline in the panel.

### Via terminal

```powershell
# Compile only (replace env as needed)
pio run -e esp32dev

# Compile + flash
pio run -e esp32dev --target upload

# Serial monitor
pio device monitor -p COM6 -b 115200
```

If flash fails:
- `esp32dev` / `ttgo-t-display` / `lilygo-t-display-s3`: hold **BOOT**, press **EN/RESET**, release **BOOT**, retry.
- `esp32s3-touch-lcd-1_9`: verify board env and COM first; BOOT sequence is usually not required.

For S3 touch board, runtime navigation uses touch tap (CST816), not BOOT button.

---

## Step 7 — Claude.ai Relay Server _(optional)_

The relay fetches Claude.ai usage stats for the ESP32 over your LAN.

### Install Python dependencies (once)

```powershell
pip install curl_cffi flask python-dotenv
```

### Start relay

**Tab: Actions** → **▶ Start Relay**

Or manually:

```powershell
cd server
python relay.py
```

The relay binds to the PC IP shown in the **☁️ Claude.ai** tab, port `8765` by default.
The ESP32 connects to `http://<PC_IP>:8765/usage`.

---

## File Map After Setup

```
include/
  config.h            ← your secrets (NOT in git)
  config.h.example    ← template (in git)
server/
  .env                ← CLAUDEAI_SESSION, LASTACTIVE_ORG (NOT in git)
  relay.py            ← relay server source
platformio.ini        ← patched upload_port / upload_speed by extension
vscode-extension/
  esp32-token-display-1.0.0.vsix  ← install this into VS Code
```

---

## Troubleshooting

### `config.h: No such file or directory` (build error)
```powershell
cp include/config.h.example include/config.h
```

### COM port not detected
- Open Device Manager → Ports (COM & LPT) → find the ESP32 port
- In Config Panel → **📡 Device** tab → press ↺, or type port manually → Save

### Extension panel does not open
```powershell
# Reinstall
code --install-extension vscode-extension\esp32-token-display-1.0.0.vsix --force
# Then: Ctrl+Shift+P → Developer: Reload Window
```

### Relay server fails to start
- Ensure Python 3.9+ is on PATH: `python --version`
- Install deps: `pip install curl_cffi flask python-dotenv`
- Check `server/.env` has `CLAUDEAI_SESSION` and `LASTACTIVE_ORG`

### Upload speed issues
Lower the speed in Config Panel → **📡 Device** → Upload Speed → `115200` → Save, then retry flash.
