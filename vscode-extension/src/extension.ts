import * as vscode from 'vscode';
import { ConfigPanel } from './ConfigPanel';
import { RelayManager } from './RelayManager';

let relayManager: RelayManager;

export function activate(context: vscode.ExtensionContext) {
    relayManager = new RelayManager(context);

    context.subscriptions.push(
        vscode.commands.registerCommand('esp32td.openConfig', () => {
            ConfigPanel.createOrShow(context, relayManager);
        }),
        vscode.commands.registerCommand('esp32td.startRelay', async () => {
            await relayManager.start();
        }),
        vscode.commands.registerCommand('esp32td.stopRelay', async () => {
            await relayManager.stop();
        }),
        vscode.commands.registerCommand('esp32td.startRelayBackground', async () => {
            await relayManager.startBackground();
        }),
        vscode.commands.registerCommand('esp32td.stopRelayBackground', async () => {
            await relayManager.stopBackground();
        })
    );

    // Open config panel automatically on activation
    ConfigPanel.createOrShow(context, relayManager);
}

export function deactivate() {
    void relayManager?.stop();
    ConfigPanel.disposeAll();
}
