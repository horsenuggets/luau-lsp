import assert from "node:assert/strict";
import { mkdtemp, mkdir, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import {
  marketplaceVersion,
  prepareRelease,
  releaseLockfile,
  releaseManifest,
  vsixAssetName,
} from "./prepare-release.mjs";

const compare = (left, right) => {
  const a = left.split(".").map(Number);
  const b = right.split(".").map(Number);
  return a[0] - b[0] || a[1] - b[1] || a[2] - b[2];
};

test("VSIX filenames use canonical OS names without changing platform targets", () => {
  for (const [target, filename] of [
    ["darwin-arm64", "luau-lsp-macos-arm64.vsix"],
    ["darwin-x64", "luau-lsp-macos-x64.vsix"],
    ["linux-arm64", "luau-lsp-linux-arm64.vsix"],
    ["linux-x64", "luau-lsp-linux-x64.vsix"],
    ["win32-arm64", "luau-lsp-windows-arm64.vsix"],
    ["win32-x64", "luau-lsp-windows-x64.vsix"],
  ]) {
    assert.equal(vsixAssetName(target), filename);
    assert.equal(vsixAssetName(target), vsixAssetName(target));
  }
  for (const target of [
    undefined,
    "",
    "macos-arm64",
    "windows-arm64",
    "win32-ia32",
    "darwin-arm64\n",
    "../win32-x64",
  ]) {
    assert.throws(() => vsixAssetName(target), /supported VS Code platform/);
  }
});

test("fork counters determine a stable numeric Marketplace version", () => {
  for (const upstream of ["0.1.0", "1.70.1", "2.0.0"]) {
    assert.equal(marketplaceVersion(`${upstream}-horse.1.12`), "2.1.12");
    assert.equal(
      marketplaceVersion(`${upstream}-horse.1.12`),
      marketplaceVersion(`${upstream}-horse.1.12`),
    );
  }
  assert.equal(marketplaceVersion("1.70.1-horse.12.345"), "2.12.345");
  assert.equal(marketplaceVersion("1.70.1-horse.65535.65535"), "2.65535.65535");
});

test("Marketplace versions increase with fork releases and upstream syncs", () => {
  const versions = [
    "1.70.1-horse.1.9",
    "1.70.1-horse.1.10",
    "1.70.1-horse.1.11",
    "1.71.0-horse.1.12",
    "1.71.0-horse.2.0",
  ];
  assert.ok(compare(marketplaceVersion(versions[0]), "1.70.1") > 0);
  for (let index = 1; index < versions.length; index += 1) {
    assert.ok(
      compare(
        marketplaceVersion(versions[index]),
        marketplaceVersion(versions[index - 1]),
      ) > 0,
    );
  }
});

test("malformed or out-of-range fork versions fail", () => {
  for (const version of [
    "",
    "1.70.1",
    "1.70.1-horse.1",
    "1.70.1-horse.01.2",
    "1.70.1-horse.1.02",
    "1.70.1-horse.1.2-extra",
    "1.70.1-horse.65536.0",
    "1.70.1-horse.0.65536",
    "1.70.1-horse.-1.2",
    "1.70.1-horse.1.2\n",
  ]) {
    assert.throws(() => marketplaceVersion(version));
  }
});

test("release metadata preserves extension identity and does not mutate inputs", () => {
  const manifest = {
    name: "luau-lsp",
    publisher: "example",
    author: "Original Author",
    version: "1.70.1",
  };
  const original = structuredClone(manifest);
  const prepared = releaseManifest(
    manifest,
    "1.70.1-horse.1.12",
    "example/project",
  );
  assert.equal(prepared.name, manifest.name);
  assert.equal(prepared.publisher, manifest.publisher);
  assert.equal(prepared.author, manifest.author);
  assert.equal(prepared.version, "2.1.12");
  assert.equal(prepared.repository.url, "https://github.com/example/project");
  assert.equal(prepared.homepage, prepared.repository.url);
  assert.equal(prepared.bugs.url, `${prepared.repository.url}/issues`);
  assert.deepEqual(manifest, original);
  assert.deepEqual(
    prepared,
    releaseManifest(manifest, "1.70.1-horse.1.12", "example/project"),
  );
  for (const repository of [
    undefined,
    "",
    "project",
    "example/../project",
    "example/project?query",
    "example/project\n",
  ]) {
    assert.throws(() =>
      releaseManifest(manifest, "1.70.1-horse.1.12", repository),
    );
  }
});

test("lockfile version updates preserve dependency pins without mutating inputs", () => {
  const lockfile = {
    version: "1.70.1",
    packages: {
      "": { version: "1.70.1", dependencies: { example: "1.0.0" } },
      "node_modules/example": { version: "1.0.0" },
    },
  };
  const original = structuredClone(lockfile);
  const prepared = releaseLockfile(lockfile, "1.70.1-horse.1.12");
  assert.equal(prepared.version, "2.1.12");
  assert.equal(prepared.packages[""].version, "2.1.12");
  assert.deepEqual(
    prepared.packages[""].dependencies,
    lockfile.packages[""].dependencies,
  );
  assert.deepEqual(
    prepared.packages["node_modules/example"],
    lockfile.packages["node_modules/example"],
  );
  assert.deepEqual(lockfile, original);
});

test("preparation synchronizes both manifests and includes release documents", async () => {
  const root = await mkdtemp(join(tmpdir(), "extension-release-"));
  const directory = join(root, "editors", "code");
  try {
    await mkdir(directory, { recursive: true });
    await writeFile(join(root, "VERSION"), "1.70.1-horse.1.12\n");
    await writeFile(
      join(directory, "package.json"),
      JSON.stringify({
        name: "luau-lsp",
        publisher: "example",
        version: "1.70.1",
      }),
    );
    await writeFile(
      join(directory, "package-lock.json"),
      JSON.stringify({
        version: "1.70.1",
        packages: { "": { version: "1.70.1" } },
      }),
    );
    for (const name of ["CHANGELOG.md", "LICENSE.md", "README.md"]) {
      await writeFile(join(root, name), `Contents of ${name}.\n`);
    }
    await assert.rejects(
      prepareRelease(directory, undefined, true),
      /must all use version/,
    );
    await assert.rejects(prepareRelease(directory, undefined), /repository/);
    assert.equal(
      JSON.parse(await readFile(join(directory, "package.json"))).version,
      "1.70.1",
    );
    await prepareRelease(directory, "example/project");
    await prepareRelease(directory, undefined, true);
    for (const name of ["CHANGELOG.md", "LICENSE.md", "README.md"]) {
      assert.equal(
        await readFile(join(directory, name), "utf8"),
        await readFile(join(root, name), "utf8"),
      );
    }
    const first = await readFile(join(directory, "package.json"), "utf8");
    await prepareRelease(directory, "example/project");
    assert.equal(
      await readFile(join(directory, "package.json"), "utf8"),
      first,
    );
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});
