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
        vscode.commands.registerCommand('esp32td.startRelay', () => {
            relayManager.start();
        }),
        vscode.commands.registerCommand('esp32td.stopRelay', () => {
            relayManager.stop();
        })
    );

    // Open config panel automatically on activation
    ConfigPanel.createOrShow(context, relayManager);
}

export function deactivate() {
    relayManager?.stop();
    ConfigPanel.disposeAll();
}
