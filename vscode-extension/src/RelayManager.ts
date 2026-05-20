import * as vscode from 'vscode';
import * as path from 'path';
import * as cp from 'child_process';

type OutputCb = (text: string) => void;
type StatusCb = (running: boolean) => void;

export class RelayManager {
    private _process: cp.ChildProcess | null = null;
    private _outputCallbacks: OutputCb[] = [];
    private _statusCallbacks: StatusCb[] = [];
    private readonly _context: vscode.ExtensionContext;
    private readonly _statusBar: vscode.StatusBarItem;

    constructor(context: vscode.ExtensionContext) {
        this._context = context;
        this._statusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
        this._statusBar.command = 'esp32td.openConfig';
        this._setBarStopped();
        this._statusBar.show();
        context.subscriptions.push(this._statusBar);
    }

    onOutput(cb: OutputCb) { this._outputCallbacks.push(cb); }
    onStatusChange(cb: StatusCb) { this._statusCallbacks.push(cb); }

    get isRunning(): boolean {
        return this._process !== null && !this._process.killed;
    }

    start() {
        if (this.isRunning) return;

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
        this._emitStatus(true);

        this._process.stdout?.on('data', (d: Buffer) => this._emit(d.toString()));
        this._process.stderr?.on('data', (d: Buffer) => this._emit('[err] ' + d.toString()));

        this._process.on('close', (code: number | null) => {
            this._process = null;
            this._emit(`\nRelay stopped (exit ${code})\n`);
            this._emitStatus(false);
        });

        this._process.on('error', (err: Error) => {
            this._process = null;
            this._emit(`\nFailed to start: ${err.message}\n`);
            this._emitStatus(false);
            vscode.window.showErrorMessage(`Relay server error: ${err.message}`);
        });
    }

    stop() {
        if (!this._process) return;
        this._process.kill('SIGTERM');
        setTimeout(() => {
            if (this._process && !this._process.killed) {
                this._process.kill('SIGKILL');
            }
        }, 2000);
    }

    dispose() { this.stop(); }

    private _emit(text: string) { this._outputCallbacks.forEach(cb => cb(text)); }

    private _emitStatus(running: boolean) {
        this._statusCallbacks.forEach(cb => cb(running));
        if (running) {
            this._statusBar.text = '$(pass-filled) Relay Running';
            this._statusBar.tooltip = 'Claude.ai Relay Server is running — click to open config';
            this._statusBar.backgroundColor = new vscode.ThemeColor('statusBarItem.warningBackground');
        } else {
            this._setBarStopped();
        }
    }

    private _setBarStopped() {
        this._statusBar.text = '$(circle-slash) Relay Off';
        this._statusBar.tooltip = 'Claude.ai Relay Server is stopped — click to open config';
        this._statusBar.backgroundColor = undefined;
    }
}
