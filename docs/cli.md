# CLI reference

```text
nah [global options] <command> [command options]
```

Global options must precede the command:

| Option | Meaning |
| --- | --- |
| `--root <path>` | Managed root; defaults to `NAH_ROOT`, then `~/.nah` |
| `--json` | Emit machine-readable output for non-executing commands |
| `--trace` | Include composition provenance |
| `-v`, `--verbose` | Include diagnostic detail |
| `-q`, `--quiet` | Suppress nonessential messages |

Use `nah <command> --help` as the authoritative option list.

## Local package workflow

```bash
nah init --app --id com.example.app ./app
nah pack ./app --output app.nap
nah --root ./nah-root install app.nap
nah --root ./nah-root show com.example.app
nah --root ./nah-root run com.example.app -- arg1 "arg two"
nah --root ./nah-root uninstall com.example.app
```

`install` accepts a local directory, `.nap`, or `.nak`. Remote URL fetching is deliberately not part of the CLI; download and verify remote artifacts before installation.

## Commands

### `init [--app|--nak|--host] [--id <id>] [--name <name>] [dir]`

Creates one current-format manifest and minimal directory structure. App is the default type. `--host` creates a usable NAH root with `host/host.json` and empty managed directories.

### `pack <dir> [-o|--output <file>]`

Creates a deterministic gzip/tar package based on `nap.json` or `nak.json`. Package ids and versions are validated. Links and special files are rejected.

### `install <source> [--force] [--app|--nak] [--dry-run] [--loader <name>]`

Validates and stages one local package before atomically activating its files and registry record. For an app with a NAK requirement, installation pins the highest installed matching semantic version.

### `uninstall <id[@version]> [--app|--nak] [--force]`

Removes package files and their record. Without a version, the highest installed version is selected. A referenced NAK requires `--force`.

### `list [--apps|--naks]`

Lists installed records.

### `which <id[@version]>`

Prints the registry record and resolved installation path.

### `show [id[@version]] [--trace]`

With no target, prints root status. With an app target, composes and displays its launch contract without executing it.

### `run <id[@version]> [--loader <name>] [-- args...]`

Composes and executes an installed app. Arguments following `--` are appended to the manifest arguments.
`--json` is rejected because successful execution hands stdout to the child process; use `show` to inspect the contract.

### `components [--all]`

Lists installed app components.

### `launch <component-uri> [--referrer <uri>] [-- args...]`

Resolves and launches a component URI.
`--json` is rejected for the same reason as `run`.

## Exit status

- `0`: success
- nonzero: validation, composition, installation, or execution failed
