import { copyFile, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

export function marketplaceVersion(version) {
  const match =
    /^(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)-horse\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.exec(
      version,
    );
  if (
    !match ||
    match[0] !== version ||
    match.slice(1).some((part) => Number(part) > 65535)
  ) {
    throw new Error(
      "Expected a fork version with numeric counters no greater than 65535.",
    );
  }
  return `2.${match[1]}.${match[2]}`;
}

export function releaseManifest(manifest, version, repository) {
  const match = /^[a-zA-Z0-9][a-zA-Z0-9_.-]*\/[a-zA-Z0-9][a-zA-Z0-9_.-]*$/.exec(
    repository,
  );
  if (!match || match[0] !== repository) {
    throw new Error("A GitHub repository in owner/name format is required.");
  }
  const url = `https://github.com/${repository}`;
  return {
    ...manifest,
    version: marketplaceVersion(version),
    homepage: url,
    repository: { type: "git", url },
    bugs: { url: `${url}/issues` },
  };
}

export function releaseLockfile(lockfile, version) {
  return {
    ...lockfile,
    version: marketplaceVersion(version),
    packages: {
      ...lockfile.packages,
      "": { ...lockfile.packages[""], version: marketplaceVersion(version) },
    },
  };
}

export async function prepareRelease(directory, repository, checkOnly = false) {
  const root = resolve(directory, "../..");
  const version = (await readFile(resolve(root, "VERSION"), "utf8")).trim();
  const manifestPath = resolve(directory, "package.json");
  const lockfilePath = resolve(directory, "package-lock.json");
  const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
  const lockfile = JSON.parse(await readFile(lockfilePath, "utf8"));
  const expected = marketplaceVersion(version);
  if (checkOnly) {
    if (
      [manifest.version, lockfile.version, lockfile.packages[""].version].some(
        (value) => value !== expected,
      )
    ) {
      throw new Error(
        `Extension manifests must all use version "${expected}".`,
      );
    }
    return;
  }
  const preparedManifest = releaseManifest(manifest, version, repository);
  const preparedLockfile = releaseLockfile(lockfile, version);
  await writeFile(
    manifestPath,
    `${JSON.stringify(preparedManifest, null, 2)}\n`,
  );
  await writeFile(
    lockfilePath,
    `${JSON.stringify(preparedLockfile, null, 2)}\n`,
  );
  for (const name of ["CHANGELOG.md", "LICENSE.md", "README.md"]) {
    await copyFile(resolve(root, name), resolve(directory, name));
  }
}

if (
  process.argv[1] &&
  pathToFileURL(resolve(process.argv[1])).href === import.meta.url
) {
  await prepareRelease(
    dirname(fileURLToPath(import.meta.url)),
    process.env.GITHUB_REPOSITORY,
    process.argv.includes("--check"),
  );
}
