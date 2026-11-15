import * as assert from "assert";
import * as path from "path";
import type * as vscode from "vscode";
import { getTestExtension } from "../helper";

suite("Extension Identity Test Suite", () => {
  const { publisher, name } = require(
    path.resolve(__dirname, "../../../package.json"),
  );
  const extensionId = `${publisher}.${name}`;
  const ownExtension = {
    id: extensionId,
    packageJSON: { publisher, name },
  } as vscode.Extension<unknown>;
  const otherExtension = {
    id: `other-publisher.${name}`,
    packageJSON: { publisher: "other-publisher", name },
  } as vscode.Extension<unknown>;

  test("selects the package identity when another publisher has the same name", () => {
    for (const candidates of [
      [otherExtension, ownExtension],
      [ownExtension, otherExtension],
    ]) {
      const extension = getTestExtension((id) =>
        candidates.find((candidate) => candidate.id === id),
      );
      assert.strictEqual(extension, ownExtension);
    }
  });

  test("does not fall back to another publisher when the package is missing", () => {
    assert.throws(
      () =>
        getTestExtension((id) =>
          [otherExtension].find((candidate) => candidate.id === id),
        ),
      { message: `The extension "${extensionId}" should be installed.` },
    );
  });
});
