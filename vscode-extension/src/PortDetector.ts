import { exec } from 'child_process';
import { promisify } from 'util';
import * as path from 'path';
import * as fs from 'fs';
import * as os from 'os';

const execAsync = promisify(exec);

function pioBin(): string {
    const win = path.join(os.homedir(), '.platformio', 'penv', 'Scripts', 'platformio.exe');
    const unix = path.join(os.homedir(), '.platformio', 'penv', 'bin', 'platformio');
    if (process.platform === 'win32' && fs.existsSync(win)) { return win; }
    if (process.platform !== 'win32' && fs.existsSync(unix)) { return unix; }
    return 'pio';
}

export async function detectPorts(): Promise<string[]> {
    // 1. Try PlatformIO (full path, no & needed — exec uses process directly)
    try {
        const pio = pioBin();
        const { stdout } = await execAsync(`"${pio}" device list --serial`, { timeout: 8000 });
        const ports: string[] = [];
        for (const line of stdout.split('\n')) {
            const m = line.match(/^(COM\d+|\/dev\/tty\w+|\/dev\/cu\.\w+)/i);
            if (m) { ports.push(m[1].trim()); }
        }
        if (ports.length > 0) { return ports; }
    } catch { /* fall through */ }

    // 2. Try Python serial (pyserial)
    try {
        const cmd = 'python -c "import serial.tools.list_ports,json;print(json.dumps([p.device for p in serial.tools.list_ports.comports()]))"';
        const { stdout } = await execAsync(cmd, { timeout: 8000 });
        const trimmed = stdout.trim();
        if (trimmed.startsWith('[')) {
            return JSON.parse(trimmed) as string[];
        }
    } catch { /* fall through */ }

    // 3. PowerShell fallback (Windows)
    try {
        const { stdout } = await execAsync(
            'powershell -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames() -join \',\'"',
            { timeout: 5000 }
        );
        return stdout.trim().split(',').map(p => p.trim()).filter(p => /^COM\d+/i.test(p));
    } catch { /* fall through */ }

    return [];
}
