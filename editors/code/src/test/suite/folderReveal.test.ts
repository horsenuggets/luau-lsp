import * as assert from "assert";
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import * as vscode from "vscode";
import {
  FolderRevealer,
  PENDING_REVEAL_TIMEOUT_MS,
  PendingReveal,
  placeholderPosition,
  shouldReveal,
  singleTarget,
} from "../../folderReveal";

const MOUSE = vscode.TextEditorSelectionChangeKind.Mouse;
const KEYBOARD = vscode.TextEditorSelectionChangeKind.Keyboard;
const COMMAND = vscode.TextEditorSelectionChangeKind.Command;

suite("Folder Reveal Test Suite", () => {
  suite("singleTarget", () => {
    const uri = vscode.Uri.file("/project/Source");
    const location = new vscode.Location(uri, new vscode.Position(0, 0));

    test("returns the target of a single location or link", () => {
      assert.strictEqual(singleTarget(location)?.toString(), uri.toString());
      assert.strictEqual(singleTarget([location])?.toString(), uri.toString());
      const link: vscode.LocationLink = {
        targetUri: uri,
        targetRange: new vscode.Range(0, 0, 0, 0),
      };
      assert.strictEqual(singleTarget([link])?.toString(), uri.toString());
    });

    test("ignores empty and ambiguous results", () => {
      assert.strictEqual(singleTarget(undefined), undefined);
      assert.strictEqual(singleTarget(null), undefined);
      assert.strictEqual(singleTarget([]), undefined);
      assert.strictEqual(singleTarget([location, location]), undefined);
    });
  });

  suite("placeholderPosition", () => {
    const lineLength = (line: number) => [22, 30, 9, 40][line];

    test("lands just before the clicked word", () => {
      const word = new vscode.Range(0, 22, 0, 28);
      assert.ok(
        placeholderPosition(
          word,
          new vscode.Position(0, 25),
          lineLength,
        ).isEqual(new vscode.Position(0, 21)),
      );
      assert.ok(
        placeholderPosition(
          undefined,
          new vscode.Position(1, 4),
          lineLength,
        ).isEqual(new vscode.Position(1, 3)),
      );
    });

    test("moves to the previous line for words that start a line", () => {
      // e.g. "local value = project." followed by "Source" on the next line.
      const word = new vscode.Range(3, 0, 3, 6);
      const placeholder = placeholderPosition(
        word,
        new vscode.Position(3, 2),
        lineLength,
      );
      assert.ok(placeholder.isEqual(new vscode.Position(2, 9)));
      assert.ok(!word.contains(placeholder));
    });

    test("never lands inside the clicked word", () => {
      for (const word of [
        new vscode.Range(0, 0, 0, 6),
        new vscode.Range(1, 0, 1, 6),
        new vscode.Range(2, 3, 2, 9),
      ]) {
        const placeholder = placeholderPosition(word, word.start, lineLength);
        // Only a word at the very start of the document has nowhere else to go.
        if (!word.start.isEqual(new vscode.Position(0, 0))) {
          assert.ok(!word.contains(placeholder), `${word.start.line}`);
        }
      }
    });
  });

  suite("shouldReveal", () => {
    const pending: PendingReveal = {
      document: "file:///project/main.luau",
      position: new vscode.Position(0, 21),
      folder: vscode.Uri.file("/project/Source"),
      createdAt: 1000,
    };
    const at = (position: vscode.Position) =>
      new vscode.Selection(position, position);

    test("reveals only for a completed jump, across every condition", () => {
      const kinds = [MOUSE, KEYBOARD, COMMAND, undefined];
      for (let mask = 0; mask < 16; mask++) {
        const sameDocument = (mask & 1) !== 0;
        const singleCursor = (mask & 2) !== 0;
        const atPlaceholder = (mask & 4) !== 0;
        const inTime = (mask & 8) !== 0;
        const document = sameDocument ? pending.document : "file:///other.luau";
        const position = atPlaceholder
          ? pending.position
          : pending.position.translate(0, 1);
        const selections = singleCursor
          ? [at(position)]
          : [at(position), at(new vscode.Position(4, 0))];
        const now =
          pending.createdAt +
          (inTime ? PENDING_REVEAL_TIMEOUT_MS : PENDING_REVEAL_TIMEOUT_MS + 1);
        for (const kind of kinds) {
          const expected =
            sameDocument &&
            singleCursor &&
            atPlaceholder &&
            inTime &&
            kind !== MOUSE &&
            kind !== KEYBOARD;
          assert.strictEqual(
            shouldReveal(pending, document, selections, kind, now),
            expected,
            `mask ${mask}, kind ${kind}`,
          );
        }
      }
    });

    test("ignores selections that aren't a bare cursor", () => {
      const range = new vscode.Selection(
        pending.position,
        pending.position.translate(0, 3),
      );
      assert.strictEqual(
        shouldReveal(pending, pending.document, [range], COMMAND, 1000),
        false,
      );
    });

    test("does nothing without a pending reveal, and is pure", () => {
      const selections = [at(pending.position)];
      assert.strictEqual(
        shouldReveal(undefined, pending.document, selections, COMMAND, 1000),
        false,
      );
      const first = shouldReveal(
        pending,
        pending.document,
        selections,
        undefined,
        1500,
      );
      const second = shouldReveal(
        pending,
        pending.document,
        selections,
        undefined,
        1500,
      );
      assert.strictEqual(first, true);
      assert.strictEqual(first, second);
    });
  });

  suite("FolderRevealer", () => {
    let root: string;
    let folder: vscode.Uri;
    let file: vscode.Uri;
    let document: vscode.TextDocument;
    const token = new vscode.CancellationTokenSource().token;

    suiteSetup(async () => {
      root = fs.mkdtempSync(path.join(os.tmpdir(), "folder-reveal-"));
      fs.mkdirSync(path.join(root, "Source"));
      fs.writeFileSync(path.join(root, "Source", "Foo.luau"), "return {}\n");
      folder = vscode.Uri.file(path.join(root, "Source"));
      file = vscode.Uri.file(path.join(root, "Source", "Foo.luau"));
      document = await vscode.workspace.openTextDocument({
        language: "plaintext",
        content: "local value = project.Source.Foo",
      });
    });

    suiteTeardown(() => {
      fs.rmSync(root, { recursive: true, force: true });
    });

    function setup() {
      const revealed: string[] = [];
      let now = 5000;
      const revealer = new FolderRevealer(
        async (uri) => revealed.push(uri.toString()),
        () => now,
      );
      return {
        revealer,
        revealed,
        advance: (ms: number) => (now += ms),
      };
    }

    test("passes file definitions through unchanged", async () => {
      const { revealer, revealed } = setup();
      const location = new vscode.Location(file, new vscode.Position(0, 0));
      const result = await revealer.provideDefinition(
        document,
        new vscode.Position(0, 30),
        token,
        async () => location,
      );
      assert.strictEqual(result, location);
      revealer.handleSelection(
        document.uri,
        [new vscode.Selection(0, 21, 0, 21)],
        undefined,
      );
      assert.deepStrictEqual(revealed, []);
      revealer.dispose();
    });

    test("turns a folder into a jump, then reveals once the jump happens", async () => {
      const { revealer, revealed } = setup();
      const result = await revealer.provideDefinition(
        document,
        new vscode.Position(0, 25),
        token,
        async () => new vscode.Location(folder, new vscode.Position(0, 0)),
      );
      assert.ok(result instanceof vscode.Location);
      assert.strictEqual(result.uri.toString(), document.uri.toString());
      // "Source" starts at column 22, so the jump lands on the "." before it.
      assert.ok(result.range.start.isEqual(new vscode.Position(0, 21)));

      // The user's own click doesn't count, but the editor's jump does, exactly once.
      revealer.handleSelection(
        document.uri,
        [new vscode.Selection(0, 25, 0, 25)],
        MOUSE,
      );
      assert.deepStrictEqual(revealed, []);
      revealer.handleSelection(
        document.uri,
        [new vscode.Selection(0, 21, 0, 21)],
        undefined,
      );
      assert.deepStrictEqual(revealed, [folder.toString()]);
      revealer.handleSelection(
        document.uri,
        [new vscode.Selection(0, 21, 0, 21)],
        undefined,
      );
      assert.deepStrictEqual(revealed, [folder.toString()]);
      revealer.dispose();
    });

    test("reveals after VS Code performs a real Go to Definition", async () => {
      const revealed: string[] = [];
      const revealer = new FolderRevealer(async (uri) =>
        revealed.push(uri.toString()),
      );
      const provider = vscode.languages.registerDefinitionProvider(
        { language: "plaintext" },
        {
          provideDefinition: (doc, position, cancel) =>
            revealer.provideDefinition(
              doc,
              position,
              cancel,
              async () =>
                new vscode.Location(folder, new vscode.Position(0, 0)),
            ),
        },
      );
      try {
        const editor = await vscode.window.showTextDocument(document);
        // A click runs the provider and then navigates to its result. Both halves are run
        // explicitly, since the Chat input can hold focus in a fresh test instance and would
        // otherwise receive editor commands.
        const position = new vscode.Position(0, 25);
        const locations = await vscode.commands.executeCommand<
          vscode.Location[]
        >("vscode.executeDefinitionProvider", document.uri, position);
        await vscode.commands.executeCommand(
          "editor.action.goToLocations",
          document.uri,
          position,
          locations,
          "goto",
        );
        // The selection event that triggers the reveal arrives asynchronously.
        for (let i = 0; i < 50 && revealed.length === 0; i++) {
          await new Promise((resolve) => setTimeout(resolve, 20));
        }
        assert.deepStrictEqual(revealed, [folder.toString()]);
        assert.ok(editor.selection.active.isEqual(new vscode.Position(0, 21)));
      } finally {
        provider.dispose();
        revealer.dispose();
        await vscode.commands.executeCommand(
          "workbench.action.closeActiveEditor",
        );
      }
    });

    test("a canceled request keeps a newer request's pending reveal", async () => {
      const { revealer, revealed } = setup();
      const canceled = new vscode.CancellationTokenSource();
      let finishOlder: () => void = () => {};
      const olderStarted = new Promise<void>((resolve) => {
        finishOlder = resolve;
      });
      // The older request (on "Source") is still resolving when the newer one finishes.
      const older = revealer.provideDefinition(
        document,
        new vscode.Position(0, 25),
        canceled.token,
        async () => {
          await olderStarted;
          return new vscode.Location(folder, new vscode.Position(0, 0));
        },
      );
      const newerDocument = await vscode.workspace.openTextDocument({
        language: "plaintext",
        content: "local value = project.Source.Foo",
      });
      await revealer.provideDefinition(
        newerDocument,
        new vscode.Position(0, 25),
        token,
        async () => new vscode.Location(folder, new vscode.Position(0, 0)),
      );
      canceled.cancel();
      finishOlder();
      assert.strictEqual(await older, undefined);

      revealer.handleSelection(
        newerDocument.uri,
        [new vscode.Selection(0, 21, 0, 21)],
        undefined,
      );
      assert.deepStrictEqual(revealed, [folder.toString()]);
      revealer.dispose();
    });

    test("a hover that's never clicked expires", async () => {
      const { revealer, revealed, advance } = setup();
      await revealer.provideDefinition(
        document,
        new vscode.Position(0, 25),
        token,
        async () => [new vscode.Location(folder, new vscode.Position(0, 0))],
      );
      advance(PENDING_REVEAL_TIMEOUT_MS + 1);
      revealer.handleSelection(
        document.uri,
        [new vscode.Selection(0, 21, 0, 21)],
        undefined,
      );
      assert.deepStrictEqual(revealed, []);
      revealer.dispose();
    });
  });
});
