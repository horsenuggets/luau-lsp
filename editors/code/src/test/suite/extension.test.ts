import * as assert from "assert";

// You can import and use all API from the 'vscode' module
// as well as import your extension to test it
import * as vscode from "vscode";
// import * as myExtension from '../../extension';

suite("Extension Test Suite", () => {
  vscode.window.showInformationMessage("Start all tests.");

  test("Document links are enabled by default", () => {
    const setting = vscode.workspace
      .getConfiguration("luau-lsp.documentLinks")
      .inspect<boolean>("enabled");
    assert.ok(setting);
    assert.strictEqual(setting.defaultValue, true);
  });

  test("Sample test", () => {
    assert.strictEqual(-1, [1, 2, 3].indexOf(5));
    assert.strictEqual(-1, [1, 2, 3].indexOf(0));
  });
});
