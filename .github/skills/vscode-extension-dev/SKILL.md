---
name: vscode-extension-dev
description: >
  Guide for developing, building, packaging, and installing the VS Code
  extension for the TokenDisplay project. Use this when asked to modify
  the extension UI (ConfigPanel.ts), add commands, change config logic,
  rebuild after edits, or reinstall the .vsix into VS Code.
---

## Extension Architecture

| File | Role |
|------|------|
| `src/extension.ts` | Entry point — registers commands, activates ConfigPanel |
| `src/ConfigPanel.ts` | Webview panel with all tabs (Home, Device, API Keys, AI Providers, Claude.ai, Actions) |
| `src/ConfigManager.ts` | Read/write config: `include/config.h`, `server/.env`, `platformio.ini` |
| `src/RelayManager.ts` | Spawns/stops `server/relay.py` as a child process |
| `src/PortDetector.ts` | Detects serial ports via PlatformIO → pyserial → PowerShell fallback |
| `package.json` | Manifest: commands, views, activity bar, contributes |
| `out/extension.js` | Compiled bundle (generated — never edit directly) |

---

## Where Config Is Stored

`ConfigManager.save()` writes to **three files**:

| File | Fields written |
|------|---------------|
| `include/config.h` | WiFi, API keys, Anthropic/OpenRouter URLs, relay host/port, update interval |
| `server/.env` | `CLAUDEAI_SESSION`, `LASTACTIVE_ORG`, `RELAY_PORT` |
| `platformio.ini` | `upload_port`, `monitor_port`, `upload_speed` |

`ConfigManager.load()` reads all three and merges them.

---

## Tab ↔ Pane Map

| Tab button label | `data-tab` | `id="tab-*"` pane |
|-----------------|-----------|------------------|
| 🏠 Home | `home` | `tab-home` |
| 📡 Device | `device` | `tab-device` |
| 🔑 API Keys | `apikeys` | `tab-apikeys` |
| 🤖 AI Providers | `anthropic` | `tab-anthropic` |
| ☁️ Claude.ai | `claudeai` | `tab-claudeai` |
| ⚡ Actions | `actions` | `tab-actions` |

---

## Build Commands

All commands run inside `vscode-extension/`:

```powershell
cd vscode-extension

# Compile TypeScript (type-check only — does NOT produce out/)
npm run compile

# Fast dev build (produces out/extension.js with source map)
npm run build

# Watch mode (rebuilds on every file save)
npm run watch

# Package into .vsix (runs vscode:prepublish → minified build first)
npm run package

# Build + package + install in one go
npm run package ; code --install-extension esp32-token-display-1.0.0.vsix --force
```

---

## Edit → Rebuild → Reinstall Workflow

```powershell
cd e:\Gap_code\TokenDisply\vscode-extension

# 1. Edit source files (ConfigPanel.ts, ConfigManager.ts, etc.)

# 2. Package (compiles + bundles + creates .vsix)
npm run package

# 3. Install
code --install-extension esp32-token-display-1.0.0.vsix --force

# 4. In VS Code: Ctrl+Shift+P → "Developer: Reload Window"
```

> **Tip**: After reload, the Config Panel reopens automatically because `activate()` calls `ConfigPanel.createOrShow()`.

---

## Adding a New Tab

1. Add a tab button in `_html()` (around line 257–262):
   ```html
   <button class="tab-btn" data-tab="mytab">🆕 My Tab</button>
   ```
2. Add the matching pane with `id="tab-mytab"`:
   ```html
   <div class="tab-pane" id="tab-mytab">
     ...content...
   </div>
   ```
3. If new fields need saving, add them to `ESP32Config` interface in `ConfigManager.ts`, update `DEFAULTS`, `buildConfigH()` / `buildEnv()`, and `load()`.

---

## Adding a New VS Code Command

1. Register in `package.json` → `contributes.commands`:
   ```json
   { "command": "esp32td.myCommand", "title": "My Command", "category": "ESP32 Token Display" }
   ```
2. Wire up in `extension.ts`:
   ```ts
   vscode.commands.registerCommand('esp32td.myCommand', () => { /* … */ })
   ```

---

## Common Issues

### `out/extension.js` is stale after editing
Run `npm run build` (or `npm run package` to also get a fresh .vsix).

### Extension does not activate
- Check the Output panel → **ESP32 Token Display** channel.
- Make sure `"activationEvents": ["onStartupFinished"]` is in `package.json`.
- Verify the installed version with: `code --list-extensions --show-versions | Select-String esp32`.

### Webview shows blank / old content
The webview HTML is inlined as a template string in `ConfigPanel._html()`. After rebuilding the extension JS, reload VS Code (`Developer: Reload Window`).

### `vsce` not found
```powershell
npm install --save-dev @vscode/vsce
```

### TypeScript errors
```powershell
npm run compile   # shows all tsc errors without bundling
```
