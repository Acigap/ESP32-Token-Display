import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import * as os from 'os';
import { spawn } from 'child_process';
import { ConfigManager, ESP32Config, detectLocalIp, getAvailableEnvs } from './ConfigManager';
import { RelayManager } from './RelayManager';
import { detectPorts } from './PortDetector';

/** Returns the platformio executable path for use with spawn(). */
function pioExe(): string {
    const win  = path.join(os.homedir(), '.platformio', 'penv', 'Scripts', 'platformio.exe');
    const unix = path.join(os.homedir(), '.platformio', 'penv', 'bin', 'platformio');
    if (process.platform === 'win32' && fs.existsSync(win))  { return win; }
    if (process.platform !== 'win32' && fs.existsSync(unix)) { return unix; }
    return 'pio';
}

function getNonce() {
    let t = '';
    const c = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    for (let i = 0; i < 32; i++) { t += c.charAt(Math.floor(Math.random() * c.length)); }
    return t;
}

export class ConfigPanel {
    public static currentPanel: ConfigPanel | undefined;
    private readonly _panel: vscode.WebviewPanel;
    private readonly _context: vscode.ExtensionContext;
    private readonly _relay: RelayManager;
    private _disposables: vscode.Disposable[] = [];

    private constructor(panel: vscode.WebviewPanel, ctx: vscode.ExtensionContext, relay: RelayManager) {
        this._panel = panel;
        this._context = ctx;
        this._relay = relay;
        this._panel.webview.html = this._html(panel.webview);
        this._panel.onDidDispose(() => this.dispose(), null, this._disposables);

        this._panel.webview.onDidReceiveMessage(async (msg) => {
            switch (msg.type) {
                case 'ready':
                    await this._pushConfig();
                    await this._pushPorts();
                    await relay.refreshStatus();
                    this._panel.webview.postMessage({ type: 'relayStatus', status: relay.status });
                    break;
                case 'saveConfig':
                    this._save(msg.config as ESP32Config);
                    break;
                case 'refreshPorts':
                    await this._pushPorts();
                    break;
                case 'build':
                    this._runPio(false, msg.port as string | undefined, msg.boardEnv as string | undefined);
                    break;
                case 'buildAndFlash':
                    this._runPio(true, msg.port as string | undefined, msg.boardEnv as string | undefined);
                    break;
                case 'openMonitor':
                    this._openMonitor(msg.port as string | undefined);
                    break;
                case 'startRelay':
                    await relay.start();
                    break;
                case 'stopRelay':
                    await relay.stop();
                    break;
                case 'startRelayBackground':
                    await relay.startBackground();
                    break;
                case 'stopRelayBackground':
                    await relay.stopBackground();
                    break;

            }
        }, null, this._disposables);

        relay.onOutput(text => this._panel.webview.postMessage({ type: 'relayOutput', text }));
        relay.onStatusChange(status => this._panel.webview.postMessage({ type: 'relayStatus', status }));
    }

    static createOrShow(ctx: vscode.ExtensionContext, relay: RelayManager) {
        const col = vscode.window.activeTextEditor?.viewColumn ?? vscode.ViewColumn.One;
        if (ConfigPanel.currentPanel) {
            ConfigPanel.currentPanel._panel.reveal(col);
            return;
        }
        const panel = vscode.window.createWebviewPanel(
            'esp32Config', 'ESP32 Token Display Config', col,
            { enableScripts: true, retainContextWhenHidden: true }
        );
        ConfigPanel.currentPanel = new ConfigPanel(panel, ctx, relay);
    }

    static disposeAll() { ConfigPanel.currentPanel?.dispose(); }

    private async _pushConfig() {
        const root = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
        if (!root) { return; }
        const config = ConfigManager.load(root);
        const detectedIp = detectLocalIp();
        const availableEnvs = getAvailableEnvs(root);
        this._panel.webview.postMessage({ type: 'loadConfig', config, detectedIp, availableEnvs });
    }

    private async _pushPorts() {
        const ports = await detectPorts();
        this._panel.webview.postMessage({ type: 'portsUpdated', ports });
    }

    private _save(config: ESP32Config) {
        const root = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
        if (!root) { return; }
        try {
            ConfigManager.save(root, config);
            this._panel.webview.postMessage({ type: 'saveResult', success: true, message: 'Config saved — config.h and server/.env updated.' });
        } catch (e: unknown) {
            const msg = e instanceof Error ? e.message : String(e);
            this._panel.webview.postMessage({ type: 'saveResult', success: false, message: msg });
        }
    }

    private _runPio(flash: boolean, port?: string, boardEnv?: string) {
        const root = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
        if (!root) { return; }

        const exe  = pioExe();
        const args = ['--no-ansi', 'run'];
        if (boardEnv) { args.push('-e', boardEnv); }
        if (flash) {
            args.push('-t', 'upload');
            if (port) { args.push('--upload-port', port); }
        }

        this._panel.webview.postMessage({
            type: 'buildOutput',
            text: `▶ ${exe} ${args.slice(1).join(' ')}\n`,
        });

        const env = {
            ...process.env,
            PYTHONUTF8: '1',
            PYTHONIOENCODING: 'utf-8',
            NO_COLOR: '1',
        };
        const proc = spawn(exe, args, { cwd: root, env });

        const fwd = (data: Buffer) => {
            this._panel.webview.postMessage({ type: 'buildOutput', text: data.toString() });
        };
        proc.stdout.on('data', fwd);
        proc.stderr.on('data', fwd);

        proc.on('close', (code) => {
            const msg = code === 0 ? '✅ Done' : `❌ Failed (exit ${code})`;
            this._panel.webview.postMessage({ type: 'buildOutput', text: `\n${msg}\n` });
        });

        proc.on('error', (err) => {
            this._panel.webview.postMessage({ type: 'buildOutput', text: `\nError: ${err.message}\n` });
        });
    }

    private _pythonBin(root: string): string {
        const wins = [
            path.join(root, '.venv', 'Scripts', 'python.exe'),
            path.join(root, '.venv', 'bin', 'python'),
        ];
        for (const p of wins) { if (fs.existsSync(p)) { return `"${p}"`; } }
        return 'python';
    }

    private _openMonitor(port?: string) {
        const root = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
        if (!root) { return; }
        const exe = pioExe();
        const args = port ? `device monitor --port ${port}` : 'device monitor';
        const t = vscode.window.createTerminal({ name: 'ESP32 Monitor', cwd: root });
        t.show();
        t.sendText(`& "${exe}" ${args}`);
    }

    public dispose() {
        ConfigPanel.currentPanel = undefined;
        this._panel.dispose();
        while (this._disposables.length) { this._disposables.pop()?.dispose(); }
    }

    // ── HTML ────────────────────────────────────────────────────────────────

    private _html(wv: vscode.Webview): string {
        const nonce = getNonce();
        return /* html */`<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; script-src 'nonce-${nonce}';">
<title>ESP32 Token Display Config</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:var(--vscode-font-family,-apple-system,BlinkMacSystemFont,sans-serif);font-size:var(--vscode-font-size,13px);color:var(--vscode-foreground,#ccc);background:var(--vscode-editor-background,#1e1e1e);height:100vh;display:flex;flex-direction:column;overflow:hidden}
.app{display:flex;flex-direction:column;height:100%;overflow:hidden}
.app-header{display:flex;align-items:center;justify-content:space-between;padding:10px 16px;border-bottom:1px solid var(--vscode-editorGroup-border,#444);flex-shrink:0;gap:8px}
.app-title{font-size:14px;font-weight:600;display:flex;align-items:center;gap:6px;white-space:nowrap}
.header-right{display:flex;gap:6px;align-items:center;flex-wrap:wrap}
.save-state{font-size:11px;padding:3px 8px;border-radius:999px;border:1px solid var(--vscode-editorGroup-border,#444);color:var(--vscode-descriptionForeground,#888)}
.save-state.unsaved{border-color:var(--vscode-inputValidation-warningBorder,#cca700);background:var(--vscode-inputValidation-warningBackground,rgba(204,167,0,.12));color:var(--vscode-editorWarning-foreground,#f4d03f)}
.save-state.saved{border-color:var(--vscode-inputValidation-infoBorder,#3794ff);background:var(--vscode-inputValidation-infoBackground,rgba(55,148,255,.10));color:var(--vscode-foreground,#ccc)}
.tabs-nav{display:flex;border-bottom:1px solid var(--vscode-editorGroup-border,#444);flex-shrink:0;background:var(--vscode-editorGroupHeader-tabsBackground,#252526);overflow-x:auto}
.tab-btn{padding:8px 14px;background:none;border:none;border-bottom:2px solid transparent;color:var(--vscode-tab-inactiveForeground,#999);cursor:pointer;font-size:12px;font-family:inherit;white-space:nowrap}
.tab-btn:hover{color:var(--vscode-foreground,#ccc);background:rgba(255,255,255,.05)}
.tab-btn.active{color:var(--vscode-tab-activeForeground,#fff);border-bottom-color:var(--vscode-tab-activeBorderTop,#0ea5e9);background:var(--vscode-editor-background,#1e1e1e)}
.tabs-content{flex:1;overflow-y:auto;padding:16px}
.tab-pane{display:none}.tab-pane.active{display:block}
.section{margin-bottom:24px}
.section-title{font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.06em;color:var(--vscode-descriptionForeground,#888);margin-bottom:10px;display:flex;align-items:center;gap:8px}
.form-row{display:grid;grid-template-columns:150px 1fr;align-items:center;gap:6px;margin-bottom:7px}
.form-row label{font-size:12px;text-align:right;padding-right:4px;color:var(--vscode-foreground,#ccc)}
input[type=text],input[type=password],input[type=number],select,textarea{background:var(--vscode-input-background,#3c3c3c);color:var(--vscode-input-foreground,#ccc);border:1px solid var(--vscode-input-border,#555);padding:4px 8px;font-size:12px;font-family:inherit;outline:none;width:100%;border-radius:2px}
input:focus,select:focus,textarea:focus{border-color:var(--vscode-focusBorder,#0ea5e9)}
.ig{display:flex;gap:4px;width:100%}.ig input,.ig select{flex:1}
.unit{align-self:center;font-size:11px;color:var(--vscode-descriptionForeground,#888);white-space:nowrap;padding:0 4px}
.btn{padding:4px 12px;font-size:12px;font-family:inherit;border:none;border-radius:2px;cursor:pointer;white-space:nowrap}
.btn-p{background:var(--vscode-button-background,#0e639c);color:var(--vscode-button-foreground,#fff)}
.btn-p:hover:not(:disabled){background:var(--vscode-button-hoverBackground,#1177bb)}
.btn-s{background:var(--vscode-button-secondaryBackground,#3a3d41);color:var(--vscode-button-secondaryForeground,#ccc)}
.btn-s:hover:not(:disabled){background:var(--vscode-button-secondaryHoverBackground,#45494e)}
.btn:disabled{opacity:.45;cursor:not-allowed}
.btn-icon{background:none;border:1px solid var(--vscode-input-border,#555);color:var(--vscode-foreground,#ccc);padding:4px 8px;cursor:pointer;font-size:12px;border-radius:2px;line-height:1}
.btn-sm{padding:2px 8px;font-size:11px}
.btn-row{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:8px}
table{width:100%;border-collapse:collapse;font-size:12px}
th{text-align:left;padding:5px 6px;color:var(--vscode-descriptionForeground,#888);font-weight:600;border-bottom:1px solid var(--vscode-editorGroup-border,#444);font-size:11px}
td{padding:3px 4px;border-bottom:1px solid rgba(255,255,255,.04)}
td input,td select{padding:3px 6px}
.btn-del{background:none;border:none;color:var(--vscode-errorForeground,#f48771);cursor:pointer;padding:2px 6px;font-size:14px}
.btn-del:hover{opacity:.7}
.out{background:var(--vscode-terminal-background,#1a1a1a);border:1px solid var(--vscode-editorGroup-border,#333);padding:8px;font-family:var(--vscode-editor-font-family,'Courier New',monospace);font-size:11px;height:160px;overflow-y:auto;white-space:pre-wrap;word-break:break-all;border-radius:2px;color:var(--vscode-terminal-foreground,#ccc);margin-top:8px}
.relay-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:8px;flex-wrap:wrap;gap:8px}
.status-row{display:flex;align-items:center;gap:8px}
.dot{width:10px;height:10px;border-radius:50%;background:#666;display:inline-block;flex-shrink:0}
.dot.on{background:#4caf50;box-shadow:0 0 6px #4caf50}
.info{background:var(--vscode-inputValidation-infoBackground,rgba(0,122,204,.12));border:1px solid var(--vscode-inputValidation-infoBorder,#007acc);padding:10px 12px;border-radius:2px;font-size:12px;margin-bottom:14px;line-height:1.6}
.warn{background:var(--vscode-inputValidation-warningBackground,rgba(204,167,0,.12));border:1px solid var(--vscode-inputValidation-warningBorder,#cca700);padding:10px 12px;border-radius:2px;font-size:12px;margin-top:10px;line-height:1.6}
.info a{color:var(--vscode-textLink-foreground,#3794ff);cursor:pointer}
code{background:rgba(255,255,255,.1);padding:1px 4px;border-radius:2px;font-family:var(--vscode-editor-font-family,monospace);font-size:11px}
.toast{position:fixed;bottom:16px;right:16px;padding:8px 14px;border-radius:4px;font-size:12px;z-index:999;opacity:0;transition:opacity .3s;pointer-events:none}
.toast.show{opacity:1}
.toast-ok{background:#4caf50;color:#fff}
.toast-err{background:#f44336;color:#fff}
.home-hero{text-align:center;padding:10px 0 18px;border-bottom:1px solid var(--vscode-editorGroup-border,#444);margin-bottom:18px}
.step-card{background:var(--vscode-editorWidget-background,rgba(255,255,255,.04));border:1px solid var(--vscode-editorGroup-border,#444);border-radius:4px;padding:12px 14px;margin-bottom:10px}
.step-num{display:inline-flex;align-items:center;justify-content:center;width:22px;height:22px;border-radius:50%;background:var(--vscode-button-background,#0e639c);color:#fff;font-size:11px;font-weight:700;flex-shrink:0;margin-right:8px}
.step-title{font-size:13px;font-weight:600;display:flex;align-items:center;margin-bottom:7px}
.step-body{font-size:12px;color:var(--vscode-foreground,#ccc);line-height:1.8;padding-left:30px}
.req-row{display:flex;align-items:flex-start;gap:6px;font-size:12px;line-height:1.8;margin-bottom:1px}
.home-link{color:var(--vscode-textLink-foreground,#3794ff);cursor:pointer;text-decoration:none}
.home-link:hover{text-decoration:underline}
</style>
</head>
<body>
<div class="app">

  <!-- Header -->
  <div class="app-header">
    <div class="app-title">🔌 ESP32 Token Display</div>
    <div class="header-right">
      <span class="save-state saved" id="save-state">✅ Saved</span>
      <button class="btn btn-p" id="btn-save">💾 Save Config</button>
    </div>
  </div>

  <!-- Tabs -->
  <div class="tabs-nav">
    <button class="tab-btn active" data-tab="home">🏠 Home</button>
    <button class="tab-btn" data-tab="device">📡 Device</button>
    <button class="tab-btn" data-tab="apikeys">🔑 API Keys</button>
    <button class="tab-btn" data-tab="anthropic">🤖 AI Providers</button>
    <button class="tab-btn" data-tab="claudeai">☁️ Claude.ai</button>
    <button class="tab-btn" data-tab="actions">⚡ Actions</button>
  </div>

  <!-- Content -->
  <div class="tabs-content">

    <!-- ── Tab: Home ── -->
    <div class="tab-pane active" id="tab-home">
      <div class="home-hero">
        <div style="font-size:34px;margin-bottom:6px">🔌</div>
        <div style="font-size:15px;font-weight:700;margin-bottom:6px">ESP32 Token Display</div>
        <div style="font-size:12px;color:var(--vscode-descriptionForeground,#888);max-width:440px;margin:0 auto;line-height:1.7">
          แสดงผล API usage จาก <strong>OpenRouter</strong>, <strong>Anthropic</strong> และ <strong>Claude.ai</strong><br>
          บน ESP32 จอ ST7789 1.9&quot; — กด BOOT เพื่อสลับหน้าจอ
        </div>
      </div>

      <div class="section">
        <div class="section-title">📋 สิ่งที่ต้องมีก่อนเริ่ม</div>
        <div class="req-row"><span>✅</span><span><strong>ESP32</strong> พร้อมจอ ST7789 1.9&quot; (170×320) ในตัว + สาย USB-C</span></div>
        <div class="req-row"><span>✅</span><span><strong>PlatformIO</strong> ติดตั้งใน VS Code — ค้นหา &quot;PlatformIO IDE&quot; ใน Extensions</span></div>
        <div class="req-row"><span>✅</span><span><strong>Python 3.8+</strong> และ: <code>pip install curl_cffi flask python-dotenv</code></span></div>
        <div class="req-row"><span>✅</span><span><strong>CH340 Driver</strong> สำหรับ USB ของบอร์ด (ถ้า Windows ไม่เห็น COM Port)</span></div>
        <div class="req-row"><span>✅</span><span><strong>API Key</strong> จาก OpenRouter หรือ Anthropic อย่างน้อย 1 อัน</span></div>
      </div>

      <div class="section">
        <div class="section-title">🚀 ขั้นตอนการตั้งค่า</div>

        <div class="step-card">
          <div class="step-title">
            <span class="step-num">1</span>ตั้งค่า WiFi และ COM Port
            <button class="btn btn-s btn-sm" style="margin-left:auto" onclick="goToTab('device')">📡 ไป Device →</button>
          </div>
          <div class="step-body">
            ใส่ <strong>WiFi SSID / Password</strong> ของเครือข่ายที่ ESP32 จะเชื่อมต่อ (2.4 GHz เท่านั้น)<br>
            เลือก <strong>COM Port</strong> ของ ESP32 กด ↺ เพื่อ refresh ถ้ายังไม่เห็น port<br>
            ปล่อย Upload Speed ไว้ที่ <strong>460800</strong> (recommended)
          </div>
        </div>

        <div class="step-card">
          <div class="step-title">
            <span class="step-num">2</span>เพิ่ม API Keys
            <button class="btn btn-s btn-sm" style="margin-left:auto" onclick="goToTab('apikeys')">🔑 ไป API Keys →</button>
          </div>
          <div class="step-body">
            กด <strong>+ Add Key</strong> แล้วใส่ชื่อและ API key<br>
            • <strong>OpenRouter</strong> — key ขึ้นต้นด้วย <code>sk-or-v1-...</code> เลือก Type: OpenRouter<br>
            • <strong>Anthropic</strong> — key ขึ้นต้นด้วย <code>sk-ant-api03-...</code> เลือก Type: Anthropic<br>
            เพิ่มได้หลาย key — ESP32 จะวนแสดงผลทีละหน้า
          </div>
        </div>

        <div class="step-card">
          <div class="step-title">
            <span class="step-num">3</span>ตั้งค่า Claude.ai Relay
            <span style="font-size:11px;font-weight:400;color:var(--vscode-descriptionForeground,#888);margin-left:4px">(optional)</span>
            <button class="btn btn-s btn-sm" style="margin-left:auto" onclick="goToTab('claudeai')">☁️ ไป Claude.ai →</button>
          </div>
          <div class="step-body">
            ข้ามได้ถ้าไม่ต้องการดู Claude.ai usage<br>
            เปิด <code>claude.ai</code> (login แล้ว) → กด <strong>F12</strong> → Application → Cookies → <code>.claude.ai</code><br>
            คัดลอก <strong>sessionKey</strong> และ <strong>lastActiveOrg</strong> มาใส่ในช่องที่กำหนด<br>
            PC IP จะถูกตรวจจับอัตโนมัติ กด 🔍 เพื่อ re-detect
          </div>
        </div>

        <div class="step-card">
          <div class="step-title">
            <span class="step-num">4</span>บันทึก Config และ Flash ESP32
            <button class="btn btn-s btn-sm" style="margin-left:auto" onclick="goToTab('actions')">⚡ ไป Actions →</button>
          </div>
          <div class="step-body">
            กด <strong>💾 Save Config</strong> (มุมบนขวา) เพื่อเขียน <code>config.h</code> และ <code>server/.env</code><br>
            จากนั้นกด <strong>⚡ Build &amp; Flash</strong> ใน Tab: Actions<br>
            รอ compile เสร็จ (ประมาณ 1–2 นาทีครั้งแรก) — ESP32 จะแสดงผลทันทีหลัง reboot ✅
          </div>
        </div>

        <div class="step-card">
          <div class="step-title">
            <span class="step-num">5</span>เริ่ม Relay Server (ถ้าใช้ Claude.ai)
            <button class="btn btn-s btn-sm" style="margin-left:auto" onclick="goToTab('claudeai')">☁️ ไป Claude.ai →</button>
          </div>
          <div class="step-body">
            ใน Tab: Claude.ai เลื่อนลงที่ส่วน <strong>Start / Stop Relay</strong><br>
            กด <strong>▶ Start Relay</strong> — server จะรันบน <code>localhost:8765</code><br>
            ESP32 จะ auto-switch ไปหน้า Claude.ai ทันทีที่ relay พร้อม
          </div>
        </div>
      </div>

      <div class="section">
        <div class="section-title">💡 Tips</div>
        <div style="font-size:12px;line-height:2">
          <div>• กด <strong>BOOT button</strong> บน ESP32 เพื่อสลับหน้าแสดงผล</div>
          <div>• ESP32 อัปเดตข้อมูลอัตโนมัติทุก <strong>30 วินาที</strong></div>
          <div>• ใช้ <strong>📺 Serial Monitor</strong> ใน Tab: Actions เพื่อดู debug log จาก ESP32</div>
          <div>• ถ้า upload ไม่ได้ ลองกด <strong>BOOT</strong> ค้างไว้แล้วกด RESET แล้วค่อยปล่อย BOOT</div>
          <div>• ESP32 รองรับ WiFi <strong>2.4 GHz เท่านั้น</strong> (ไม่รองรับ 5 GHz)</div>
        </div>
      </div>
    </div>

    <!-- ── Tab: Device ── -->
    <div class="tab-pane" id="tab-device">
      <div class="section">
        <div class="section-title">WiFi</div>
        <div class="form-row">
          <label>SSID</label>
          <input type="text" id="wifi-ssid" placeholder="Network name">
        </div>
        <div class="form-row">
          <label>Password</label>
          <div class="ig">
            <input type="password" id="wifi-password" placeholder="WiFi password">
            <button class="btn-icon" data-toggle="wifi-password" title="Show/hide">👁</button>
          </div>
        </div>
      </div>

      <div class="section">
        <div class="section-title">ESP32 Serial</div>
        <div class="form-row">
          <label>Board</label>
          <select id="board-env"><option value="esp32dev">esp32dev</option></select>
        </div>
        <div class="form-row">
          <label>COM Port</label>
          <div class="ig">
            <select id="com-port"><option value="">-- Detecting... --</option></select>
            <button class="btn btn-s" id="btn-refresh-ports">↺</button>
          </div>
        </div>
        <div class="form-row">
          <label>Upload Speed</label>
          <select id="upload-speed">
            <option value="921600">921600 (fastest)</option>
            <option value="460800" selected>460800 (recommended)</option>
            <option value="115200">115200 (safe)</option>
          </select>
        </div>
      </div>

      <div class="section">
        <div class="section-title">Display</div>
        <div class="form-row">
          <label>Theme</label>
          <select id="display-theme">
            <option value="dark"  selected>🌑 Dark — black bg, high-contrast (default)</option>
            <option value="light">☀️ Light — white bg, dark text</option>
            <option value="vivid">🎨 Vivid — dark bg, status-tinted header</option>
          </select>
        </div>
        <div class="form-row">
          <label>Update Interval</label>
          <div class="ig">
            <input type="number" id="update-interval" min="5000" step="1000" value="30000">
            <span class="unit">ms</span>
          </div>
        </div>
      </div>
    </div>

    <!-- ── Tab: API Keys ── -->
    <div class="tab-pane" id="tab-apikeys">
      <div class="section">
        <div class="section-title">
          API Keys
          <button class="btn btn-s btn-sm" id="btn-add-key">+ Add Key</button>
        </div>
        <table>
          <thead>
            <tr>
              <th style="width:160px">Name</th>
              <th>API Key</th>
              <th style="width:110px">Type</th>
              <th style="width:32px"></th>
            </tr>
          </thead>
          <tbody id="api-keys-body"></tbody>
        </table>
      </div>
    </div>

    <!-- ── Tab: Anthropic ── -->
    <div class="tab-pane" id="tab-anthropic">
      <div class="section">
        <div class="section-title">Anthropic</div>
        <div class="form-row">
          <label>Model</label>
          <select id="anthropic-model">
            <option value="claude-haiku-4-5">claude-haiku-4-5</option>
            <option value="claude-3-5-haiku-20241022">claude-3-5-haiku-20241022</option>
            <option value="claude-3-5-sonnet-20241022">claude-3-5-sonnet-20241022</option>
            <option value="claude-opus-4-5">claude-opus-4-5</option>
            <option value="claude-3-opus-20240229">claude-3-opus-20240229</option>
          </select>
        </div>
        <div class="form-row">
          <label>API Version</label>
          <input type="text" id="anthropic-api-version" value="2023-06-01">
        </div>
        <div class="form-row">
          <label>Base URL</label>
          <input type="text" id="anthropic-base-url" value="https://api.anthropic.com">
        </div>
      </div>
      <div class="section">
        <div class="section-title">OpenRouter</div>
        <div class="form-row">
          <label>API URL</label>
          <input type="text" id="openrouter-api-url" value="https://openrouter.ai/api/v1/auth/key">
        </div>
      </div>
    </div>

    <!-- ── Tab: Claude.ai Session ── -->
    <div class="tab-pane" id="tab-claudeai">
      <div class="section">
        <div class="info">
          <strong>ℹ️ Relay Server</strong><br>
          A small server running on this PC fetches your Claude.ai usage stats for the ESP32.<br>
          Values are saved to <code>server/.env</code> only — never uploaded anywhere.<br><br>
          <strong>How to get Session Key and Org ID:</strong><br>
          1. Open Edge or Chrome and go to <code>claude.ai</code> (make sure you are logged in)<br>
          2. Press <kbd>F12</kbd> → <em>Application</em> tab → <em>Cookies</em> → <code>.claude.ai</code><br>
          3. Copy the value of <code>sessionKey</code> → paste into <em>Session Key</em> below<br>
          4. Copy the value of <code>lastActiveOrg</code> → paste into <em>Org ID</em> below
        </div>

        <div class="section-title">Credentials</div>
        <div class="form-row">
          <label>Session Key</label>
          <div class="ig">
            <input type="password" id="claude-session" placeholder="sk-ant-sid01-...">
            <button class="btn-icon" data-toggle="claude-session" title="Show/hide">👁</button>
          </div>
        </div>
        <div class="form-row">
          <label>Org ID</label>
          <input type="text" id="org-id" placeholder="lastActiveOrg UUID (e.g. 2ef4399b-...)">
        </div>

        <div class="section-title" style="margin-top:16px">Relay Server</div>
        <div class="form-row">
          <label>PC IP (Host)</label>
          <div class="ig">
            <input type="text" id="relay-host" placeholder="detecting..." style="max-width:160px">
            <button class="btn-icon" id="btn-detect-ip" title="Auto-detect PC LAN IP">🔍</button>
          </div>
        </div>
        <div style="display:grid;grid-template-columns:150px 1fr;margin-top:-2px;margin-bottom:4px">
          <span></span>
          <span id="relay-host-hint" style="font-size:11px;color:var(--vscode-descriptionForeground,#888);padding-left:2px;display:none"></span>
        </div>
        <div class="form-row">
          <label>Port</label>
          <input type="number" id="relay-port" value="8765" min="1024" max="65535" style="max-width:100px">
        </div>
      </div>

      <div class="section">
        <div class="section-title">Start / Stop Relay</div>
        <div class="relay-header">
          <div class="status-row">
            <span class="dot" id="relay-dot"></span>
            <span id="relay-status-text">Stopped</span>
          </div>
          <div class="btn-row" style="margin:0">
            <button class="btn btn-p" id="btn-start-relay">▶ Start Relay</button>
            <button class="btn btn-s" id="btn-start-relay-bg">▶ Start Relay (Background)</button>
            <button class="btn btn-s" id="btn-stop-relay" disabled>■ Stop</button>
            <button class="btn btn-s" id="btn-stop-relay-bg" disabled>■ Stop Background</button>
          </div>
        </div>
        <div class="warn">
          <strong>⚠️ Background mode notice</strong><br>
          เมื่อกด <strong>Start Relay (Background)</strong> แล้ว relay จะยังทำงานต่อแม้ปิด workspace หรือปิด VS Code<br>
          ต้องกด <strong>Stop Background</strong> (หรือ kill process) เพื่อหยุดการทำงาน
        </div>
        <div class="out" id="relay-output">Relay not running</div>
      </div>
    </div>

    <!-- ── Tab: Actions ── -->
    <div class="tab-pane" id="tab-actions">
      <div class="section">
        <div class="section-title">Build &amp; Flash</div>
        <div class="btn-row">
          <button class="btn btn-s" id="btn-build">🔨 Build Only</button>
          <button class="btn btn-p" id="btn-flash">⚡ Build &amp; Flash</button>
          <button class="btn btn-s" id="btn-monitor">📺 Serial Monitor</button>
        </div>
        <div class="out" id="build-output" style="height:220px">Press Build or Build &amp; Flash to start...</div>
      </div>

    </div>

  </div><!-- /tabs-content -->
</div><!-- /app -->

<div class="toast" id="toast"></div>

<script nonce="${nonce}">
const vscode = acquireVsCodeApi();
let isDirty = false;
let saveInProgress = false;
let pendingBuildAction = null;

function setDirty(nextDirty) {
  isDirty = !!nextDirty;
  const badge = document.getElementById('save-state');
  if (!badge) return;
  badge.classList.remove('saved', 'unsaved');
  if (isDirty) {
    badge.classList.add('unsaved');
    badge.textContent = '⚠ Unsaved changes';
  } else {
    badge.classList.add('saved');
    badge.textContent = '✅ Saved';
  }
}

function markDirty() {
  if (!saveInProgress) {
    setDirty(true);
  }
}

// ── Tab helper ─────────────────────────────────────────────────────────────
function goToTab(name) {
  const btn = document.querySelector('[data-tab="' + name + '"]');
  if (btn) btn.click();
}

// ── Tab switching ──────────────────────────────────────────────────────────
document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('tab-' + btn.dataset.tab).classList.add('active');
  });
});

// ── Password toggles ───────────────────────────────────────────────────────
document.querySelectorAll('[data-toggle]').forEach(btn => {
  btn.addEventListener('click', () => {
    const inp = document.getElementById(btn.dataset.toggle);
    if (!inp) return;
    inp.type = inp.type === 'password' ? 'text' : 'password';
    btn.textContent = inp.type === 'password' ? '👁' : '🙈';
  });
});

// ── Relay IP detection ─────────────────────────────────────────────────────
let _detectedIp = '';
const RELAY_HOST_DEFAULTS = ['192.168.1.100', ''];

function applyDetectedIp(ip) {
  if (!ip) return;
  _detectedIp = ip;
  const inp  = document.getElementById('relay-host');
  const hint = document.getElementById('relay-host-hint');
  // Auto-fill only if still holding a placeholder value
  if (RELAY_HOST_DEFAULTS.includes(inp.value.trim())) {
    inp.value = ip;
  }
  hint.textContent = 'Detected PC IP: ' + ip + (inp.value === ip ? ' ✓' : '');
  hint.style.display = 'inline';
}

document.getElementById('btn-detect-ip').addEventListener('click', () => {
  const inp  = document.getElementById('relay-host');
  const hint = document.getElementById('relay-host-hint');
  if (_detectedIp) {
    inp.value = _detectedIp;
    hint.textContent = 'Detected PC IP: ' + _detectedIp + ' ✓';
    hint.style.display = 'inline';
  }
});

// ── API Keys table ─────────────────────────────────────────────────────────
let apiKeys = [];

function esc(s) {
  return String(s ?? '').replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function renderKeys() {
  const tbody = document.getElementById('api-keys-body');
  tbody.innerHTML = '';
  apiKeys.forEach((k, i) => {
    const tr = document.createElement('tr');
    tr.innerHTML =
      '<td><input type="text" data-i="' + i + '" data-f="name" value="' + esc(k.name) + '"></td>' +
      '<td><div class="ig">' +
        '<input type="password" class="kv" data-i="' + i + '" data-f="key" value="' + esc(k.key) + '">' +
        '<button class="btn-icon toggle-key" data-i="' + i + '" title="Show/hide">👁</button>' +
      '</div></td>' +
      '<td><select data-i="' + i + '" data-f="isAnthropic">' +
        '<option value="false"' + (!k.isAnthropic ? ' selected' : '') + '>OpenRouter</option>' +
        '<option value="true"' + (k.isAnthropic ? ' selected' : '') + '>Anthropic</option>' +
      '</select></td>' +
      '<td><button class="btn-del" data-i="' + i + '" title="Delete">✕</button></td>';
    tbody.appendChild(tr);
  });

  tbody.querySelectorAll('input[data-f],select[data-f]').forEach(el => {
    ['input','change'].forEach(ev => el.addEventListener(ev, e => {
      const t = e.target, idx = +t.dataset.i, f = t.dataset.f;
      apiKeys[idx][f] = f === 'isAnthropic' ? t.value === 'true' : t.value;
      markDirty();
    }));
  });
  tbody.querySelectorAll('.toggle-key').forEach(btn => {
    btn.addEventListener('click', () => {
      const inp = tbody.querySelector('.kv[data-i="' + btn.dataset.i + '"]');
      if (!inp) return;
      inp.type = inp.type === 'password' ? 'text' : 'password';
      btn.textContent = inp.type === 'password' ? '👁' : '🙈';
    });
  });
  tbody.querySelectorAll('.btn-del').forEach(btn => {
    btn.addEventListener('click', () => { apiKeys.splice(+btn.dataset.i, 1); renderKeys(); markDirty(); });
  });
}

document.getElementById('btn-add-key').addEventListener('click', () => {
  apiKeys.push({ name: '', key: '', isAnthropic: false });
  renderKeys();
  markDirty();
});

// ── Collect / load config ──────────────────────────────────────────────────
function collect() {
  return {
    wifiSsid:           document.getElementById('wifi-ssid').value,
    wifiPassword:       document.getElementById('wifi-password').value,
    boardEnv:           document.getElementById('board-env').value,
    comPort:            document.getElementById('com-port').value,
    uploadSpeed:        parseInt(document.getElementById('upload-speed').value) || 460800,
    updateInterval:     parseInt(document.getElementById('update-interval').value) || 30000,
    displayTheme:       document.getElementById('display-theme').value || 'dark',
    apiKeys:            apiKeys.map(k => ({...k})),
    anthropicModel:     document.getElementById('anthropic-model').value,
    anthropicApiVersion:document.getElementById('anthropic-api-version').value,
    anthropicBaseUrl:   document.getElementById('anthropic-base-url').value,
    openrouterApiUrl:   document.getElementById('openrouter-api-url').value,
    claudeSession:      document.getElementById('claude-session').value,
    orgId:              document.getElementById('org-id').value,
    relayHost:          document.getElementById('relay-host').value,
    relayPort:          parseInt(document.getElementById('relay-port').value) || 8765,
  };
}

function load(cfg) {
  const s = (id, v) => { const el = document.getElementById(id); if (el) el.value = v ?? ''; };
  s('wifi-ssid',            cfg.wifiSsid);
  s('wifi-password',        cfg.wifiPassword);
  if (cfg.boardEnv) { s('board-env', cfg.boardEnv); }
  s('update-interval',      cfg.updateInterval ?? 30000);
  s('display-theme',        cfg.displayTheme ?? 'dark');
  s('anthropic-model',      cfg.anthropicModel ?? 'claude-haiku-4-5');
  s('anthropic-api-version',cfg.anthropicApiVersion ?? '2023-06-01');
  s('anthropic-base-url',   cfg.anthropicBaseUrl ?? 'https://api.anthropic.com');
  s('openrouter-api-url',   cfg.openrouterApiUrl ?? 'https://openrouter.ai/api/v1/auth/key');
  s('claude-session',       cfg.claudeSession);
  s('org-id',               cfg.orgId);
  s('relay-host',           cfg.relayHost ?? '192.168.1.100');
  s('relay-port',           cfg.relayPort ?? 8765);
  s('upload-speed',         String(cfg.uploadSpeed ?? 460800));
  apiKeys = (cfg.apiKeys ?? []).map(k => ({...k}));
  renderKeys();
  setDirty(false);
}

// ── Ports ──────────────────────────────────────────────────────────────────
function updateEnvs(envs) {
  const sel = document.getElementById('board-env');
  const prev = sel.value;
  sel.innerHTML = '';
  envs.forEach(e => {
    const o = document.createElement('option');
    o.value = e; o.textContent = e;
    if (e === prev) o.selected = true;
    sel.appendChild(o);
  });
  if (!prev && envs.length) sel.value = envs[0];
}

function updatePorts(ports) {
  const sel = document.getElementById('com-port');
  const prev = sel.value;
  sel.innerHTML = '<option value="">-- Select port --</option>';
  ports.forEach(p => {
    const o = document.createElement('option');
    o.value = p; o.textContent = p;
    if (p === prev) o.selected = true;
    sel.appendChild(o);
  });
  if (!prev && ports.length === 1) sel.value = ports[0];
}

// ── Relay status ───────────────────────────────────────────────────────────
function setRelayStatus(status) {
  const mode = status?.mode || 'stopped';
  const running = !!status?.running;

  document.getElementById('relay-dot').className = 'dot' + (running ? ' on' : '');

  if (!running) {
    document.getElementById('relay-status-text').textContent = '⚪ Stopped';
  } else if (mode === 'background') {
    document.getElementById('relay-status-text').textContent = '🟢 Running (background)';
  } else {
    document.getElementById('relay-status-text').textContent = '🟢 Running (workspace)';
  }

  document.getElementById('btn-start-relay').disabled = running;
  document.getElementById('btn-start-relay-bg').disabled = running;
  document.getElementById('btn-stop-relay').disabled = mode !== 'foreground';
  document.getElementById('btn-stop-relay-bg').disabled = mode !== 'background';
}

// ── Output helpers ─────────────────────────────────────────────────────────
function appendOut(id, text) {
  const box = document.getElementById(id);
  box.textContent += text;
  box.scrollTop = box.scrollHeight;
}

function runBuild(kind) {
  document.querySelector('[data-tab="actions"]').click();
  document.getElementById('build-output').textContent = '';
  const payload = {
    port: document.getElementById('com-port').value,
    boardEnv: document.getElementById('board-env').value,
  };
  vscode.postMessage(kind === 'flash' ? { type: 'buildAndFlash', ...payload } : { type: 'build', ...payload });
}

function queueSaveAndBuild(kind) {
  pendingBuildAction = kind;
  if (saveInProgress) return;
  saveInProgress = true;
  vscode.postMessage({ type: 'saveConfig', config: collect() });
}

function requestBuild(kind) {
  if (!isDirty) {
    runBuild(kind);
    return;
  }
  const wantsSave = confirm('You have unsaved changes. Save config before building?');
  if (wantsSave) {
    queueSaveAndBuild(kind);
    return;
  }
  toast('Build canceled: please save config first.', false);
}

document.querySelector('.tabs-content').addEventListener('input', (e) => {
  const t = e.target;
  if (t && (t.matches('input') || t.matches('textarea') || t.matches('select'))) {
    markDirty();
  }
}, true);

document.querySelector('.tabs-content').addEventListener('change', (e) => {
  const t = e.target;
  if (t && (t.matches('input') || t.matches('textarea') || t.matches('select'))) {
    markDirty();
  }
}, true);

// ── Toast ──────────────────────────────────────────────────────────────────
function toast(msg, ok) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className = 'toast show ' + (ok ? 'toast-ok' : 'toast-err');
  setTimeout(() => { el.className = 'toast'; }, 3500);
}

// ── Button handlers ────────────────────────────────────────────────────────
document.getElementById('btn-save').addEventListener('click', () => {
  if (saveInProgress) return;
  saveInProgress = true;
  vscode.postMessage({ type: 'saveConfig', config: collect() });
});
document.getElementById('btn-refresh-ports').addEventListener('click', () => {
  vscode.postMessage({ type: 'refreshPorts' });
});
document.getElementById('btn-build').addEventListener('click', () => {
  requestBuild('build');
});
document.getElementById('btn-flash').addEventListener('click', () => {
  requestBuild('flash');
});
document.getElementById('btn-monitor').addEventListener('click', () => {
  vscode.postMessage({ type: 'openMonitor', port: document.getElementById('com-port').value });
});
document.getElementById('btn-start-relay').addEventListener('click', () => {
  document.getElementById('relay-output').textContent = '';
  vscode.postMessage({ type: 'startRelay' });
});
document.getElementById('btn-start-relay-bg').addEventListener('click', () => {
  document.getElementById('relay-output').textContent = '';
  vscode.postMessage({ type: 'startRelayBackground' });
});
document.getElementById('btn-stop-relay').addEventListener('click', () => {
  vscode.postMessage({ type: 'stopRelay' });
});
document.getElementById('btn-stop-relay-bg').addEventListener('click', () => {
  vscode.postMessage({ type: 'stopRelayBackground' });
});
// ── Message listener ───────────────────────────────────────────────────────
window.addEventListener('message', ev => {
  const m = ev.data;
  switch (m.type) {
    case 'loadConfig':   load(m.config); if (m.availableEnvs) updateEnvs(m.availableEnvs); if (m.config.boardEnv) document.getElementById('board-env').value = m.config.boardEnv; if (m.detectedIp) applyDetectedIp(m.detectedIp); break;
    case 'portsUpdated': updatePorts(m.ports); break;
    case 'buildOutput':  appendOut('build-output', m.text); break;
    case 'relayOutput':  appendOut('relay-output', m.text); break;
    case 'relayStatus':  setRelayStatus(m.status || { running: !!m.running, mode: m.running ? 'foreground' : 'stopped' }); break;
    case 'saveResult':
      saveInProgress = false;
      toast(m.message, m.success);
      if (m.success) {
        setDirty(false);
        if (pendingBuildAction) {
          const next = pendingBuildAction;
          pendingBuildAction = null;
          runBuild(next);
        }
      } else {
        pendingBuildAction = null;
      }
      break;
  }
});

// Signal ready
vscode.postMessage({ type: 'ready' });
</script>
</body>
</html>`;
    }
}
