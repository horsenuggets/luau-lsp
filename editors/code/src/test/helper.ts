/* --------------------------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License. See License.txt in the project root for license information.
 * ------------------------------------------------------------------------------------------ */

import * as vscode from "vscode";
import * as path from "path";

export let doc: vscode.TextDocument;
export let editor: vscode.TextEditor;
export let documentEol: string;
export let platformEol: string;

export function getTestExtension(
  getExtension: (id: string) => vscode.Extension<unknown> | undefined = vscode
    .extensions.getExtension,
): vscode.Extension<unknown> {
  const { publisher, name } = require(
    path.resolve(__dirname, "../../package.json"),
  );
  const extensionId = `${publisher}.${name}`;
  const extension = getExtension(extensionId);
  if (!extension) {
    throw new Error(`The extension "${extensionId}" should be installed.`);
  }
  return extension;
}

/**
 * Activates the extension declared by this package.
 */
export async function activate(docUri: vscode.Uri) {
  const ext = getTestExtension();
  await ext.activate();
  try {
    doc = await vscode.workspace.openTextDocument(docUri);
    editor = await vscode.window.showTextDocument(doc);
    await sleep(2000); // Wait for server activation
  } catch (e) {
    console.error(e);
  }
}

async function sleep(ms: number) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export const getDocPath = (p: string) => {
  return path.resolve(__dirname, "../../testFixture", p);
};
export const getDocUri = (p: string) => {
  return vscode.Uri.file(getDocPath(p));
};

export async function setTestContent(content: string): Promise<boolean> {
  const all = new vscode.Range(
    doc.positionAt(0),
    doc.positionAt(doc.getText().length),
  );
  return editor.edit((eb) => eb.replace(all, content));
}
