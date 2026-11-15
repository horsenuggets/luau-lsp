import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { fileURLToPath } from "node:url";
import test from "node:test";

test("extension test profiles are unique and removed when the runner exits", () => {
  const profiles = Array.from({ length: 2 }, () => {
    const result = spawnSync(
      process.execPath,
      [
        "-e",
        'const config = require("./.vscode-test.js"); process.stdout.write(JSON.stringify(config));',
      ],
      { cwd: fileURLToPath(new URL(".", import.meta.url)), encoding: "utf8" },
    );
    assert.equal(result.status, 0, result.stderr);
    const config = JSON.parse(result.stdout);
    assert.equal(config.files, "out/test/**/*.test.js");
    const profile = config.launchArgs[0].slice("--user-data-dir=".length);
    assert.ok(profile.length > 0);
    assert.equal(existsSync(profile), false);
    return profile;
  });
  assert.notEqual(profiles[0], profiles[1]);
});
