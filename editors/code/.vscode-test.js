const { mkdtempSync, rmSync } = require("node:fs");
const { tmpdir } = require("node:os");
const { join } = require("node:path");
const { defineConfig } = require("@vscode/test-cli");
const profile = mkdtempSync(join(tmpdir(), "lsp-test-"));

process.once("exit", () => rmSync(profile, { recursive: true, force: true }));

module.exports = defineConfig({
  files: "out/test/**/*.test.js",
  launchArgs: [`--user-data-dir=${profile}`],
});
