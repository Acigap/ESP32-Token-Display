import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import * as http from 'http';
import * as cp from 'child_process';

type OutputCb = (text: string) => void;
type RelayMode = 'stopped' | 'foreground' | 'background';

export interface RelayStatus {
    running: boolean;
    mode: RelayMode;
    pid?: number;
}

type StatusCb = (status: RelayStatus) => void;

export class RelayManager {
    private _process: cp.ChildProcess | null = null;
    private _outputCallbacks: OutputCb[] = [];
    private _statusCallbacks: StatusCb[] = [];
    private readonly _context: vscode.ExtensionContext;
    private readonly _statusBar: vscode.StatusBarItem;
    private _status: RelayStatus = { running: false, mode: 'stopped' };

    private static readonly BG_PID_KEY = 'relay.backgroundPid';
    private static readonly DEFAULT_RELAY_PORT = 8765;
    private static readonly STARTUP_RETRY_COUNT = 10;
    private static readonly STARTUP_RETRY_DELAY_MS = 500;
    private static readonly HEALTHCHECK_TIMEOUT_MS = 1500;

    constructor(context: vscode.ExtensionContext) {
        this._context = context;
        this._statusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
        this._statusBar.command = 'esp32td.openConfig';
        this._setBarStopped();
        this._statusBar.show();
        context.subscriptions.push(this._statusBar);

        void this.refreshStatus();
    }

    onOutput(cb: OutputCb) { this._outputCallbacks.push(cb); }
    onStatusChange(cb: StatusCb) { this._statusCallbacks.push(cb); }

    get status(): RelayStatus { return { ...this._status }; }

    get isRunning(): boolean {
        return this._status.running;
    }

    async refreshStatus(): Promise<void> {
        if (this._process && !this._process.killed) {
            this._emitStatus({ running: true, mode: 'foreground' });
            return;
        }

        const pid = await this._getBackgroundPid();
        if (pid && this._isProcessAlive(pid)) {
            const port = this._getRelayPort();
            const responsive = await this._waitForUsage(port, 1, 0);
            if (responsive) {
                this._emitStatus({ running: true, mode: 'background', pid });
                return;
            }

            this._emit(`Background relay PID ${pid} is alive but /usage is not reachable on localhost:${port}.\n`);
            await this._setBackgroundPid(undefined);
        }

        if (pid) {
            await this._setBackgroundPid(undefined);
        }
        this._emitStatus({ running: false, mode: 'stopped' });
    }

    async start(): Promise<void> {
        await this.refreshStatus();
        if (this._status.running) {
            vscode.window.showInformationMessage('Relay is already running.');
            return;
        }

        const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
        if (!workspaceRoot) {
            vscode.window.showErrorMessage('No workspace folder found');
            return;
        }

        const scriptPath = path.join(workspaceRoot, 'server', 'relay.py');

        this._process = cp.spawn('python', [scriptPath], {
            cwd: path.join(workspaceRoot, 'server'),
            stdio: ['ignore', 'pipe', 'pipe'],
        });

        this._emit('Relay server starting...\n');
        this._emitStatus({ running: true, mode: 'foreground' });

        this._process.stdout?.on('data', (d: Buffer) => this._emit(d.toString()));
        this._process.stderr?.on('data', (d: Buffer) => this._emit('[err] ' + d.toString()));

        this._process.on('close', (code: number | null) => {
            this._process = null;
            this._emit(`\nRelay stopped (exit ${code})\n`);
            void this.refreshStatus();
        });

        this._process.on('error', (err: Error) => {
            this._process = null;
            this._emit(`\nFailed to start: ${err.message}\n`);
            void this.refreshStatus();
            vscode.window.showErrorMessage(`Relay server error: ${err.message}`);
        });
    }

    async startBackground(): Promise<void> {
        await this.refreshStatus();
        if (this._status.running) {
            vscode.window.showInformationMessage('Relay is already running.');
            return;
        }

        const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
        if (!workspaceRoot) {
            vscode.window.showErrorMessage('No workspace folder found');
            return;
        }

        const scriptPath = path.join(workspaceRoot, 'server', 'relay.py');
        const child = cp.spawn('python', [scriptPath], {
            cwd: path.join(workspaceRoot, 'server'),
            detached: true,
            stdio: 'ignore',
            windowsHide: true,
        });

        child.unref();

        if (!child.pid) {
            vscode.window.showErrorMessage('Failed to start relay in background.');
            return;
        }

        await this._setBackgroundPid(child.pid);
        const port = this._getRelayPort();
        this._emit(`Relay background started (PID ${child.pid}), waiting for /usage on localhost:${port}...\n`);

        const responsive = await this._waitForUsage(
            port,
            RelayManager.STARTUP_RETRY_COUNT,
            RelayManager.STARTUP_RETRY_DELAY_MS
        );

        if (!responsive) {
            await this._setBackgroundPid(undefined);
            try {
                await this._killPid(child.pid);
            } catch {
                // ignore stop failure here; user already gets the main error
            }
            this._emitStatus({ running: false, mode: 'stopped' });
            this._emit(`Relay failed health-check: /usage did not respond on localhost:${port}.\n`);
            vscode.window.showErrorMessage(
                `Relay background started but /usage is not reachable on localhost:${port}. Please check server/.env and Python dependencies.`
            );
            return;
        }

        this._emit(`Relay /usage is reachable on localhost:${port}.\n`);
        this._emitStatus({ running: true, mode: 'background', pid: child.pid });
    }

    async stop(): Promise<void> {
        if (!this._process) {
            await this.refreshStatus();
            if (this._status.mode === 'background') {
                vscode.window.showInformationMessage('Relay is running in background. Use Stop Background Relay.');
            }
            return;
        }
        this._process.kill('SIGTERM');
        setTimeout(() => {
            if (this._process && !this._process.killed) {
                this._process.kill('SIGKILL');
            }
        }, 2000);
    }

    async stopBackground(): Promise<void> {
        await this.refreshStatus();
        const pid = this._status.mode === 'background' ? this._status.pid : undefined;
        if (!pid) return;

        try {
            await this._killPid(pid);
            this._emit(`Relay background stopped (PID ${pid})\n`);
        } catch (err: unknown) {
            const msg = err instanceof Error ? err.message : String(err);
            this._emit(`Failed to stop background relay: ${msg}\n`);
            vscode.window.showWarningMessage(`Could not stop background relay: ${msg}`);
        } finally {
            await this._setBackgroundPid(undefined);
            this._emitStatus({ running: false, mode: 'stopped' });
        }
    }

    dispose() { void this.stop(); }

    private _emit(text: string) { this._outputCallbacks.forEach(cb => cb(text)); }

    private _emitStatus(status: RelayStatus) {
        this._status = { ...status };
        this._statusCallbacks.forEach(cb => cb({ ...status }));
        if (status.mode === 'foreground') {
            this._statusBar.text = '$(pass-filled) Relay Running';
            this._statusBar.tooltip = 'Claude.ai Relay Server is running — click to open config';
            this._statusBar.backgroundColor = new vscode.ThemeColor('statusBarItem.warningBackground');
        } else if (status.mode === 'background') {
            this._statusBar.text = '$(broadcast) Relay BG Running';
            this._statusBar.tooltip = 'Claude.ai Relay Server is running in background — click to open config';
            this._statusBar.backgroundColor = new vscode.ThemeColor('statusBarItem.warningBackground');
        } else {
            this._setBarStopped();
        }
    }

    private async _getBackgroundPid(): Promise<number | undefined> {
        const pid = this._context.globalState.get<number>(RelayManager.BG_PID_KEY);
        return typeof pid === 'number' && pid > 0 ? pid : undefined;
    }

    private async _setBackgroundPid(pid: number | undefined): Promise<void> {
        await this._context.globalState.update(RelayManager.BG_PID_KEY, pid);
    }

    private _isProcessAlive(pid: number): boolean {
        try {
            process.kill(pid, 0);
            return true;
        } catch {
            return false;
        }
    }

    private _killPid(pid: number): Promise<void> {
        if (process.platform === 'win32') {
            return new Promise((resolve, reject) => {
                cp.execFile('taskkill', ['/PID', String(pid), '/T', '/F'], (err) => {
                    if (!err) {
                        resolve();
                        return;
                    }
                    const msg = String(err.message || err);
                    const isGone = /not found|no running instance|cannot find the process/i.test(msg);
                    if (isGone) {
                        resolve();
                        return;
                    }
                    reject(err);
                });
            });
        }

        return new Promise((resolve, reject) => {
            try {
                process.kill(pid, 'SIGTERM');
                resolve();
            } catch (err) {
                reject(err);
            }
        });
    }

    private _setBarStopped() {
        this._statusBar.text = '$(circle-slash) Relay Off';
        this._statusBar.tooltip = 'Claude.ai Relay Server is stopped — click to open config';
        this._statusBar.backgroundColor = undefined;
    }

    private _getWorkspaceRoot(): string | undefined {
        return vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
    }

    private _getRelayPort(): number {
        const workspaceRoot = this._getWorkspaceRoot();
        if (!workspaceRoot) {
            return RelayManager.DEFAULT_RELAY_PORT;
        }

        const envPath = path.join(workspaceRoot, 'server', '.env');
        if (!fs.existsSync(envPath)) {
            return RelayManager.DEFAULT_RELAY_PORT;
        }

        try {
            const content = fs.readFileSync(envPath, 'utf8');
            const match = content.match(/^\s*RELAY_PORT\s*=\s*(\d+)\s*$/m);
            const parsed = match ? Number.parseInt(match[1], 10) : NaN;
            if (Number.isInteger(parsed) && parsed > 0 && parsed <= 65535) {
                return parsed;
            }
        } catch {
            // fall through to default
        }

        return RelayManager.DEFAULT_RELAY_PORT;
    }

    private async _waitForUsage(port: number, retries: number, delayMs: number): Promise<boolean> {
        const attempts = Math.max(1, retries);
        for (let i = 0; i < attempts; i++) {
            if (await this._isUsageReachable(port)) {
                return true;
            }
            if (i < attempts - 1 && delayMs > 0) {
                await new Promise<void>((resolve) => setTimeout(resolve, delayMs));
            }
        }
        return false;
    }

    private _isUsageReachable(port: number): Promise<boolean> {
        return new Promise((resolve) => {
            let settled = false;
            const done = (value: boolean) => {
                if (settled) {
                    return;
                }
                settled = true;
                resolve(value);
            };

            const req = http.request(
                {
                    hostname: '127.0.0.1',
                    port,
                    path: '/usage',
                    method: 'GET',
                    timeout: RelayManager.HEALTHCHECK_TIMEOUT_MS,
                },
                (res) => {
                    res.resume();
                    done(true);
                }
            );

            req.on('timeout', () => {
                req.destroy();
                done(false);
            });
            req.on('error', () => done(false));
            req.end();
        });
    }
}
