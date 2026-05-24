import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';

export function detectLocalIp(): string {
    const ifaces = os.networkInterfaces();
    const candidates: { ip: string; score: number }[] = [];

    for (const [name, addrs] of Object.entries(ifaces)) {
        if (!addrs) { continue; }
        // Skip loopback / known virtual adapters by interface name
        if (/loopback|vmware|vbox|docker|vethernet.*wsl|vethernet.*nat|vethernet.*default/i.test(name)) { continue; }

        for (const addr of addrs) {
            if (addr.family !== 'IPv4' || addr.internal) { continue; }
            const ip = addr.address;
            let score = 0;
            if (ip.startsWith('192.168.'))              { score = 30; } // home/office LAN — preferred
            else if (ip.startsWith('10.'))               { score = 20; } // corporate LAN
            else if (/^172\.(1[6-9]|2\d|3[01])\./.test(ip)) { score = 5; }  // Docker/WSL range — last resort
            else                                          { score = 10; }
            candidates.push({ ip, score });
        }
    }

    if (candidates.length === 0) { return ''; }
    candidates.sort((a, b) => b.score - a.score);
    return candidates[0].ip;
}

export interface ApiKey {
    name: string;
    key: string;
    isAnthropic: boolean;
}

export interface ESP32Config {
    // WiFi
    wifiSsid: string;
    wifiPassword: string;
    // Board / Serial / Build
    boardEnv: string;
    comPort: string;
    uploadSpeed: number;
    // Display
    updateInterval: number;
    displayTheme: 'dark' | 'light' | 'vivid';
    // API Keys
    apiKeys: ApiKey[];
    // Anthropic / OpenRouter
    anthropicModel: string;
    anthropicApiVersion: string;
    anthropicBaseUrl: string;
    openrouterApiUrl: string;
    // Claude.ai session (relay server)
    claudeSession: string;
    orgId: string;
    relayHost: string;
    relayPort: number;
}

const DEFAULTS: ESP32Config = {
    wifiSsid: '',
    wifiPassword: '',
    boardEnv: 'esp32dev',
    comPort: 'COM8',
    uploadSpeed: 460800,
    updateInterval: 30000,
    displayTheme: 'dark',
    apiKeys: [],
    anthropicModel: 'claude-haiku-4-5',
    anthropicApiVersion: '2023-06-01',
    anthropicBaseUrl: 'https://api.anthropic.com',
    openrouterApiUrl: 'https://openrouter.ai/api/v1/auth/key',
    claudeSession: '',
    orgId: '',
    relayHost: '192.168.1.100',
    relayPort: 8765,
};

// ── Parsers ────────────────────────────────────────────────────────────────

function parseStr(src: string, name: string): string {
    const m = src.match(new RegExp(`#define\\s+${name}\\s+"([^"]*)"`));
    return m ? m[1] : '';
}

function parseNum(src: string, name: string): number {
    const m = src.match(new RegExp(`#define\\s+${name}\\s+(\\d+)`));
    return m ? parseInt(m[1], 10) : 0;
}

function parseApiKeys(src: string): ApiKey[] {
    const keys: ApiKey[] = [];
    const re = /\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(true|false)\s*\}/g;
    let m: RegExpExecArray | null;
    while ((m = re.exec(src)) !== null) {
        keys.push({ name: m[1], key: m[2], isAnthropic: m[3] === 'true' });
    }
    return keys;
}

function parseEnv(src: string): Record<string, string> {
    const out: Record<string, string> = {};
    for (const raw of src.split('\n')) {
        const line = raw.trim();
        if (!line || line.startsWith('#') || !line.includes('=')) { continue; }
        const idx = line.indexOf('=');
        const k = line.slice(0, idx).trim();
        let v = line.slice(idx + 1).trim().replace(/^["']|["']$/g, '');
        out[k] = v;
    }
    return out;
}

// ── Prefs (board selection persistence) ───────────────────────────────────

function prefsPath(workspaceRoot: string): string {
    return path.join(workspaceRoot, '.vscode', 'esp32-display.json');
}

function loadPrefs(workspaceRoot: string): { boardEnv?: string } {
    try {
        const p = prefsPath(workspaceRoot);
        if (fs.existsSync(p)) { return JSON.parse(fs.readFileSync(p, 'utf-8')); }
    } catch { /* ignore */ }
    return {};
}

function savePrefs(workspaceRoot: string, prefs: { boardEnv?: string }): void {
    const p = prefsPath(workspaceRoot);
    fs.mkdirSync(path.dirname(p), { recursive: true });
    fs.writeFileSync(p, JSON.stringify(prefs, null, 2), 'utf-8');
}

// ── Public API ─────────────────────────────────────────────────────────────

/** Returns all [env:xxx] names found in platformio.ini. */
export function getAvailableEnvs(workspaceRoot: string): string[] {
    const pioIni = path.join(workspaceRoot, 'platformio.ini');
    if (!fs.existsSync(pioIni)) { return ['esp32dev']; }
    const src = fs.readFileSync(pioIni, 'utf-8');
    const envs: string[] = [];
    for (const m of src.matchAll(/^\[env:([^\]]+)\]/gm)) { envs.push(m[1]); }
    return envs.length ? envs : ['esp32dev'];
}

/**
 * Replace a key's value within a specific [env:name] section only.
 * Leaves all other sections untouched.
 */
function patchIniEnvSection(src: string, envName: string, replacements: Record<string, string>): string {
    const lines = src.split('\n');
    let inTarget = false;
    return lines.map(line => {
        const sec = line.match(/^\[env:([^\]]+)\]/);
        if (sec) { inTarget = sec[1] === envName; return line; }
        if (!inTarget) { return line; }
        for (const [key, value] of Object.entries(replacements)) {
            const re = new RegExp(`^(\\s*${key}\\s*=\\s*)\\S+`);
            if (re.test(line)) { return line.replace(re, `$1${value}`); }
        }
        return line;
    }).join('\n');
}

export class ConfigManager {
    static load(workspaceRoot: string): ESP32Config {
        const cfg: ESP32Config = { ...DEFAULTS };

        // config.h
        const configH = path.join(workspaceRoot, 'include', 'config.h');
        if (fs.existsSync(configH)) {
            const src = fs.readFileSync(configH, 'utf-8');
            cfg.wifiSsid           = parseStr(src, 'WIFI_SSID')           || cfg.wifiSsid;
            cfg.wifiPassword       = parseStr(src, 'WIFI_PASSWORD')       || cfg.wifiPassword;
            cfg.anthropicBaseUrl   = parseStr(src, 'ANTHROPIC_BASE_URL')   || cfg.anthropicBaseUrl;
            cfg.anthropicApiVersion= parseStr(src, 'ANTHROPIC_API_VERSION')|| cfg.anthropicApiVersion;
            cfg.anthropicModel     = parseStr(src, 'ANTHROPIC_MODEL')      || cfg.anthropicModel;
            cfg.openrouterApiUrl   = parseStr(src, 'OPENROUTER_API_URL')   || cfg.openrouterApiUrl;
            cfg.relayHost          = parseStr(src, 'RELAY_HOST')           || cfg.relayHost;
            const relayPort = parseNum(src, 'RELAY_PORT');
            if (relayPort) { cfg.relayPort = relayPort; }
            const interval = parseNum(src, 'UPDATE_INTERVAL');
            if (interval) { cfg.updateInterval = interval; }
            const themeM = src.match(/#define\s+DISPLAY_THEME\s+THEME_(DARK|LIGHT|VIVID)/);
            if (themeM) { cfg.displayTheme = themeM[1].toLowerCase() as 'dark' | 'light' | 'vivid'; }
            const keys = parseApiKeys(src);
            if (keys.length) { cfg.apiKeys = keys; }
        }

        // server/.env
        const envFile = path.join(workspaceRoot, 'server', '.env');
        if (fs.existsSync(envFile)) {
            const env = parseEnv(fs.readFileSync(envFile, 'utf-8'));
            cfg.claudeSession = env['CLAUDEAI_SESSION'] ?? '';
            cfg.orgId         = env['LASTACTIVE_ORG']   ?? '';
            const rport = parseInt(env['RELAY_PORT'] ?? '');
            if (rport) { cfg.relayPort = rport; }
        }

        // platformio.ini  (boardEnv, upload_port, upload_speed)
        const pioIni = path.join(workspaceRoot, 'platformio.ini');
        if (fs.existsSync(pioIni)) {
            const src = fs.readFileSync(pioIni, 'utf-8');
            const availableEnvs = getAvailableEnvs(workspaceRoot);
            const prefs = loadPrefs(workspaceRoot);
            // Use last saved board choice, fall back to first env in file
            if (prefs.boardEnv && availableEnvs.includes(prefs.boardEnv)) {
                cfg.boardEnv = prefs.boardEnv;
            } else {
                const envMatch = src.match(/^\[env:([^\]]+)\]/m);
                if (envMatch) { cfg.boardEnv = envMatch[1]; }
            }
            // Read port/speed from the chosen env section
            const lines = src.split('\n');
            let inTarget = false;
            for (const line of lines) {
                const sec = line.match(/^\[env:([^\]]+)\]/);
                if (sec) { inTarget = sec[1] === cfg.boardEnv; continue; }
                if (!inTarget) { continue; }
                const portM  = line.match(/^\s*upload_port\s*=\s*(\S+)/);
                const speedM = line.match(/^\s*upload_speed\s*=\s*(\d+)/);
                if (portM)  { cfg.comPort     = portM[1]; }
                if (speedM) { cfg.uploadSpeed = parseInt(speedM[1], 10); }
            }
        }

        return cfg;
    }

    static save(workspaceRoot: string, cfg: ESP32Config): void {
        // Write include/config.h
        fs.writeFileSync(
            path.join(workspaceRoot, 'include', 'config.h'),
            buildConfigH(cfg),
            'utf-8'
        );

        // Write server/.env
        fs.mkdirSync(path.join(workspaceRoot, 'server'), { recursive: true });
        fs.writeFileSync(
            path.join(workspaceRoot, 'server', '.env'),
            buildEnv(cfg),
            'utf-8'
        );

        // Patch platformio.ini — only the selected env section
        const pioIni = path.join(workspaceRoot, 'platformio.ini');
        if (fs.existsSync(pioIni)) {
            let src = fs.readFileSync(pioIni, 'utf-8');
            const envName = cfg.boardEnv || 'esp32dev';
            const replacements: Record<string, string> = {};
            if (cfg.comPort) {
                replacements['upload_port']  = cfg.comPort;
                replacements['monitor_port'] = cfg.comPort;
            }
            replacements['upload_speed'] = String(cfg.uploadSpeed);
            src = patchIniEnvSection(src, envName, replacements);
            fs.writeFileSync(pioIni, src, 'utf-8');
        }

        // Persist board selection so reload remembers it
        savePrefs(workspaceRoot, { boardEnv: cfg.boardEnv });
    }
}

// ── Generators ─────────────────────────────────────────────────────────────

function buildConfigH(cfg: ESP32Config): string {
    const keysBlock = cfg.apiKeys.map(k =>
        `    {"${k.name}", "${k.key}", ${k.isAnthropic}},`
    ).join('\n');

    return `#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID     "${cfg.wifiSsid}"
#define WIFI_PASSWORD "${cfg.wifiPassword}"

// OpenRouter API
#define OPENROUTER_API_URL "${cfg.openrouterApiUrl}"

// Anthropic API
#define ANTHROPIC_BASE_URL    "${cfg.anthropicBaseUrl}"
#define ANTHROPIC_API_VERSION "${cfg.anthropicApiVersion}"
#define ANTHROPIC_MODEL       "${cfg.anthropicModel}"

// API Keys Configuration
struct APIKeyConfig {
    const char* name;
    const char* key;
    bool        isAnthropic;
};

const APIKeyConfig API_KEYS[] = {
${keysBlock}
};

const int NUM_API_KEYS = sizeof(API_KEYS) / sizeof(APIKeyConfig);

// Display Configuration (landscape: 320x170)
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 170
#define BACKGROUND_COLOR TFT_BLACK
#define TEXT_COLOR TFT_WHITE
#define HIGHLIGHT_COLOR TFT_YELLOW

// Physical button pin (BOOT button = GPIO 0, active LOW)
#define BUTTON_PIN 0

// Relay server (PC running server/relay.py)
#define RELAY_HOST "${cfg.relayHost}"
#define RELAY_PORT ${cfg.relayPort}

// Update interval (milliseconds)
#define UPDATE_INTERVAL ${cfg.updateInterval}  // ${cfg.updateInterval / 1000} seconds

// Display theme — DARK (default), LIGHT (white bg), or VIVID (status-tinted header)
#define DISPLAY_THEME THEME_${(cfg.displayTheme ?? 'dark').toUpperCase()}


#endif // CONFIG_H
`;
}

function buildEnv(cfg: ESP32Config): string {
    return [
        '# Claude.ai Relay Server credentials',
        '# Generated by ESP32 Token Display Config extension — do not commit this file',
        '',
        `CLAUDEAI_SESSION="${cfg.claudeSession}"`,
        `LASTACTIVE_ORG="${cfg.orgId}"`,
        `RELAY_PORT=${cfg.relayPort}`,
    ].join('\n') + '\n';
}
