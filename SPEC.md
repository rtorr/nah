# NAH specification

Status: implemented contract for NAH 2.x. “Must” identifies behavior required for compatibility.

## Scope

NAH turns local package metadata and host state into a launch contract. It owns:

- app and runtime manifests;
- local package creation, installation, lookup, and removal;
- semantic-version selection at install time;
- deterministic contract composition;
- optional process execution.

NAH does not own remote acquisition, registries, updates, signatures, authorization, sandbox creation, or permission enforcement. Composition must never access the network or mutate installation state.

## Terms

- App: a native executable or payload described by `nap.json`.
- NAK: a local runtime or SDK described by `nak.json`.
- Host: the product or process that owns a NAH root and executes contracts.
- Install record: mutable host-owned state describing installed files and the pinned NAK.
- Launch contract: the complete executable, arguments, working directory, environment, library paths, declarations, and trust state produced by composition.

## Filesystem model

Given root `<root>`, the CLI owns:

```text
<root>/
  apps/<app-id>-<version>/
  naks/<nak-id>/<version>/
  registry/apps/<app-id>@<version>.json
  registry/naks/<nak-id>@<version>.json
  host/host.json
  staging/
```

Package ids must match `[A-Za-z0-9][A-Za-z0-9._-]*`. Versions must be semantic versions. These restrictions make derived filenames unambiguous.

Paths stored relative to `<root>` are portable. Before filesystem use, a consumer must resolve them against `<root>` and reject any result outside the applicable managed directory. Registry input is untrusted; a path in a record must never authorize deletion outside `<root>`.

## App manifest

An app package contains exactly one `nap.json`. Its current minimal form is:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v1.json",
  "app": {
    "identity": {
      "id": "com.example.app",
      "version": "1.0.0"
    },
    "execution": {
      "entrypoint": "bin/app",
      "args": []
    }
  }
}
```

`app.identity.nak_id` and `nak_version_req` request a runtime. `app.execution.loader` requests a named loader. Entrypoints and app layout paths are relative to the app root and must not escape it.

Environment values may be strings or operations with `op`, `value`, and optional `separator`. Supported operations are `set`, `prepend`, `append`, and `unset`.

Components are entries under `app.components.provides`. A component id must be unique within the app. Its entrypoint must be a packaged regular file. A wildcard URI pattern ending in `/*` matches only descendants at the next path boundary; `/open/*` must not match `/openly`.

## NAK manifest

A NAK package contains exactly one `nak.json`:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nak.v1.json",
  "nak": {
    "identity": {
      "id": "com.example.runtime",
      "version": "1.0.0"
    },
    "paths": {
      "lib_dirs": ["lib"]
    },
    "loaders": {
      "default": {
        "exec_path": "bin/runtime",
        "args_template": ["{NAH_APP_ENTRY}"]
      }
    }
  }
}
```

NAK resource, library, and loader paths must be relative, contained by the NAK root, and present in the package. A NAK may omit loaders when it contributes libraries or environment only.

## Package format and safety

`.nap` and `.nak` are gzip-compressed USTAR archives. Creation must:

- enumerate regular files and directories in lexicographic path order;
- write uid, gid, and mtime as zero;
- preserve only permission bits;
- reject links and special files;
- reject paths that USTAR cannot represent;
- write through a unique temporary file and activate the completed output without exposing a partial archive.

Extraction must stream with explicit limits. It must verify tar header checksums and reject:

- absolute, rooted, backslash-containing, empty, or parent-traversing paths;
- symlinks, hardlinks, devices, sockets, and FIFOs;
- malformed numeric fields, truncated entries, and checksum mismatches;
- more than 100,000 entries, an entry over 512 MiB, or total materialized data over 1 GiB.

Extraction failure must clean its unique temporary directory and must not alter the final installation.

## Installation

Installation accepts only a local directory, `.nap`, or `.nak`. A source must contain exactly one package manifest. The CLI validates the source tree, manifest identity, declared entrypoints, and contained paths before changing existing state.

Files and the new registry record are prepared under `<root>/staging`. Activation must not leave a partial new installation. `--force` may replace the same id and version; without it, an existing file tree or record is an error.

When installing an app with a NAK requirement:

1. Parse the semantic-version range.
2. Consider installed NAK records with the requested id and valid versions.
3. Select the highest matching version.
4. Pin its exact record filename and selected loader in the app install record.

If no NAK matches, installation may succeed with a warning, but composition cannot use an unpinned runtime. An explicitly selected loader that the chosen NAK does not provide is an installation error.

When a version is omitted from `which`, `show`, or `uninstall`, the highest installed semantic version is selected. Removing a NAK referenced by an app’s exact `record_ref` is rejected unless `--force` is present.

## Composition

Composition is a pure function of:

- app declaration;
- host environment;
- app install record;
- installed NAK inventory;
- composition options.

It must validate absolute resolved roots and containment before producing a contract. A pinned NAK is loaded by exact `record_ref`; composition must not re-resolve a newer version.

Environment precedence, from lowest to highest, is:

1. app declaration;
2. NAK operations;
3. host operations;
4. allowed install overrides.

Unset removes a key. Prepend and append use their declared separator. Placeholder expansion occurs after the environment has been composed. Missing placeholders produce diagnostics rather than implicit host environment reads.

Library paths are ordered as host prepend, install prepend, NAK libraries, app libraries, host append, then install append. Relative app paths resolve under the app root; relative NAK paths resolve under the NAK root. Any escape is a hard failure.

For a NAK loader, its executable becomes `execution.binary` and its expanded template begins the argument list. Otherwise the app entrypoint is the binary. Install argument prepends, manifest arguments, install appends, and caller-supplied execution arguments retain their order.

The contract serializer must produce valid JSON with stable field order and sorted map keys.

## Permissions and trust

Manifest permissions are declarations, not enforced rules. Composition leaves `enforcement.filesystem` and `enforcement.network` empty and reports normalized capability strings in manifest order:

- filesystem `read`, `write`, `execute` become `filesystem.<operation>:<selector>`;
- network `connect`, `listen`, `bind` become `network.<operation>:<selector>`;
- malformed or unknown operations produce warnings.

Selectors remain opaque strings.

Trust state is copied from the install record. Missing trust data is `unknown` and produces a warning. If `expires_at` precedes `CompositionOptions.now`, composition produces a stale-trust warning. NAH does not itself verify artifacts or signatures.

## Execution

Execution must pass the contract as an argument vector, never through a shell. Caller arguments are appended without splitting or reinterpretation. The contract working directory and environment are applied to the child. Platform-specific command-line encoding must preserve argument boundaries.

## CLI

The implemented command grammar is:

```text
nah [global options] init [--app|--nak|--host] [dir]
nah [global options] pack <dir> [--output <file>]
nah [global options] install <local-source> [--force] [--app|--nak] [--dry-run]
nah [global options] uninstall <id[@version]> [--app|--nak] [--force]
nah [global options] list [--apps|--naks]
nah [global options] which <id[@version]>
nah [global options] show [app-id[@version]] [--trace]
nah [global options] run <app-id[@version]> [--loader <name>] [-- args...]
nah [global options] components [--all]
nah [global options] launch <component-uri> [--referrer <uri>] [-- args...]
```

Global options precede the subcommand. JSON mode must emit one valid JSON value. A nonzero exit status means the requested operation failed.

## Compatibility boundary

The JSON schemas in `docs/schemas` are authoritative for authored manifests and records. Parsers may accept legacy fields for migration, but generators, documentation, tests, and new records must use the current shapes in this document.
