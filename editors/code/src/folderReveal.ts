import * as vscode from "vscode";
import { ProvideDefinitionSignature } from "vscode-languageclient";

// How long a pending reveal waits for the editor to perform the jump it was given.
export const PENDING_REVEAL_TIMEOUT_MS = 2000;

export interface PendingReveal {
  document: string;
  position: vscode.Position;
  folder: vscode.Uri;
  createdAt: number;
}

type DefinitionResult = vscode.Definition | vscode.DefinitionLink[];

/**
 * Returns the single target of a definition result, or undefined when there are several.
 */
export function singleTarget(
  result: DefinitionResult | null | undefined,
): vscode.Uri | undefined {
  if (!result) {
    return undefined;
  }
  const entries = Array.isArray(result) ? result : [result];
  if (entries.length !== 1) {
    return undefined;
  }
  const entry = entries[0];
  return entry instanceof vscode.Location ? entry.uri : entry.targetUri;
}

/**
 * Picks where to jump instead of the folder. This is the character just before the clicked
 * word, which a click on the word itself can never place the cursor at, so the jump always
 * produces its own selection change.
 */
export function placeholderPosition(
  wordRange: vscode.Range | undefined,
  position: vscode.Position,
): vscode.Position {
  const start = wordRange?.start ?? position;
  return start.character > 0 ? start.translate(0, -1) : start;
}

/**
 * Decides whether a selection change is the editor completing a pending folder jump. Mouse
 * and keyboard changes are the user moving the cursor themselves, and anything after the
 * timeout is unrelated.
 */
export function shouldReveal(
  pending: PendingReveal | undefined,
  document: string,
  selections: readonly vscode.Selection[],
  kind: vscode.TextEditorSelectionChangeKind | undefined,
  now: number,
): boolean {
  if (!pending || pending.document !== document) {
    return false;
  }
  if (
    kind === vscode.TextEditorSelectionChangeKind.Mouse ||
    kind === vscode.TextEditorSelectionChangeKind.Keyboard
  ) {
    return false;
  }
  if (now - pending.createdAt > PENDING_REVEAL_TIMEOUT_MS) {
    return false;
  }
  return (
    selections.length === 1 &&
    selections[0].isEmpty &&
    selections[0].active.isEqual(pending.position)
  );
}

async function isDirectory(uri: vscode.Uri): Promise<boolean> {
  if (uri.scheme !== "file") {
    return false;
  }
  try {
    const stat = await vscode.workspace.fs.stat(uri);
    return (stat.type & vscode.FileType.Directory) !== 0;
  } catch {
    return false;
  }
}

/**
 * Reveals folders that Go to Definition resolves to. Definition providers run for both
 * Ctrl/Cmd-hover and Ctrl/Cmd-click, so the reveal can't happen in the provider itself.
 * Instead, the folder becomes a jump to a placeholder position next to the clicked word, and
 * the folder is revealed once the editor actually performs that jump.
 */
export class FolderRevealer implements vscode.Disposable {
  private pending: PendingReveal | undefined;
  private readonly subscription: vscode.Disposable;

  constructor(
    private readonly reveal: (folder: vscode.Uri) => Thenable<unknown> = (
      folder,
    ) => vscode.commands.executeCommand("revealInExplorer", folder),
    private readonly now: () => number = Date.now,
  ) {
    this.subscription = vscode.window.onDidChangeTextEditorSelection((event) =>
      this.handleSelection(
        event.textEditor.document.uri,
        event.selections,
        event.kind,
      ),
    );
  }

  provideDefinition = async (
    document: vscode.TextDocument,
    position: vscode.Position,
    token: vscode.CancellationToken,
    next: ProvideDefinitionSignature,
  ): Promise<DefinitionResult | null | undefined> => {
    const result = await next(document, position, token);
    const target = singleTarget(result);
    if (!target || !(await isDirectory(target))) {
      return result;
    }
    const placeholder = placeholderPosition(
      document.getWordRangeAtPosition(position),
      position,
    );
    this.pending = {
      document: document.uri.toString(),
      position: placeholder,
      folder: target,
      createdAt: this.now(),
    };
    return new vscode.Location(document.uri, placeholder);
  };

  handleSelection(
    document: vscode.Uri,
    selections: readonly vscode.Selection[],
    kind: vscode.TextEditorSelectionChangeKind | undefined,
  ): void {
    const pending = this.pending;
    if (
      !pending ||
      !shouldReveal(pending, document.toString(), selections, kind, this.now())
    ) {
      return;
    }
    this.pending = undefined;
    void this.reveal(pending.folder);
  }

  dispose(): void {
    this.subscription.dispose();
  }
}
