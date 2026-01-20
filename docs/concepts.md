# Core Concepts

This document defines the key terms and artifacts in NAH.

## Overview

NAH separates concerns between three parties:

| Party | Owns | Artifact |
|-------|------|----------|
| App Developer | What the app needs | App Manifest |
| SDK Developer | What the SDK provides | NAK (Native App Kit) |
| Host Platform | Where things go, what's allowed | Host Environment |

NAH composes these into a **Launch Contract** - an exact specification of how to run the app.

## App Manifest

The application's declaration of identity and requirements.

**Owner:** App developer  
**Mutability:** Immutable after build  
**Format:** TLV binary (embedded in executable or as `manifest.nah`)

**Contains:**
- Identity: `id`, `version`
- NAK requirement: `nak_id`, `nak_version_req`
- Entrypoint: path to binary relative to app root
- Layout: lib dirs, asset dirs
- Environment defaults
- Declared capabilities

**Does not contain:**
- Host filesystem paths
- Host policy or trust decisions
- NAK install locations

### Native Apps: Embedded Manifest

For native applications (C, C++, Rust), embed the manifest in the binary:

```cpp
NAH_APP_MANIFEST(
    NAH_FIELD_ID("com.example.myapp")
    NAH_FIELD_VERSION("1.0.0")
    NAH_FIELD_NAK_ID("com.example.sdk")
    NAH_FIELD_NAK_VERSION_REQ(">=2.0.0 <3.0.0")
    NAH_FIELD_ENTRYPOINT("bin/myapp")
    NAH_FIELD_LIB_DIR("lib")
)
```

### Bundle Apps: File-based Manifest

For bundle applications (JavaScript, Python, etc.), create a `manifest.json` and generate the binary manifest:

```json
{
  "app": {
    "id": "com.example.myapp",
    "version": "1.0.0",
    "nak_id": "com.example.js-runtime",
    "nak_version_req": ">=2.0.0",
    "entrypoint": "bundle.js"
  }
}
```

Generate with:
```bash
nah manifest generate manifest.json -o manifest.nah
```

See [Getting Started: Bundle Apps](getting-started-bundle.md) for the complete workflow.

## NAK (Native App Kit)

A versioned SDK, runtime, or framework that apps depend on at launch.

**Owner:** SDK developer  
**Mutability:** Immutable after packaging  
**Format:** `.nak` archive with `META/nak.json`

**Contains:**
- Identity: `id`, `version`
- Libraries (shared objects, dylibs, DLLs)
- Resources
- Environment defaults
- Optional loader binary

**Does not contain:**
- App identity or entrypoints
- Host policy
- Install locations

NAKs are installed on the host and referenced by apps via `nak_id` and a version requirement. Multiple versions of the same NAK can coexist.

Example `META/nak.json`:
```json
{
  "nak": {
    "id": "com.example.sdk",
    "version": "2.1.0"
  },
  "paths": {
    "resource_root": "resources",
    "lib_dirs": ["lib"]
  },
  "environment": {
    "SDK_VERSION": "2.1.0"
  }
}
```

### Loader (Optional)

A NAK may include a loader binary that wraps app execution:

```json
{
  "loader": {
    "exec_path": "bin/sdk-loader",
    "args_template": ["--app", "{NAH_APP_ENTRY}"]
  }
}
```

When present, the loader is invoked instead of the app binary directly.

## NAP (Native App Package)

A packaged application ready for installation.

**Owner:** App developer  
**Format:** `.nap` archive

**Contains:**
- Application binary (with embedded manifest or separate `manifest.nah`)
- Libraries
- Assets
- Optional `META/package.json` with metadata

NAPs are installed by the host. At install time, NAH selects a compatible NAK version and pins it in the App Install Record.

## Host Environment

The host's configuration for NAH behavior.

**Owner:** Host platform  
**Mutability:** Mutable by host  
**Format:** JSON file at `host/host.json`

**Controls:**
- Environment variables to inject
- Library path modifications (prepend/append)
- Override policy (allow/deny environment overrides)

Example:
```json
{
  "environment": {
    "DEPLOYMENT_ENV": "production",
    "NAH_HOST_NAME": "myhost"
  },
  "paths": {
    "library_prepend": ["/opt/libs"],
    "library_append": []
  },
  "overrides": {
    "allow_env_overrides": true,
    "allowed_env_keys": []
  }
}
```

The host environment is simpler than the previous profile system - it just provides environment variables and library paths. NAK selection happens at install time based on the app's version requirements.

## App Install Record

Per-installation state created when an app is installed.

**Owner:** Host  
**Mutability:** Mutable by host  
**Format:** JSON in `registry/installs/`

**Contains:**
- App identity and version
- Install location
- Pinned NAK (id, version, record reference)
- Provenance (source, hash, install timestamp)
- Trust state
- Per-install overrides

The pinned NAK is selected at install time and does not change unless the app is reinstalled.

## NAK Install Record

Per-installation state for an installed NAK.

**Owner:** Host  
**Mutability:** Mutable by host  
**Format:** JSON in `registry/naks/`

**Contains:**
- NAK identity and version
- Install paths (root, lib dirs, resource root)
- Environment defaults
- Loader configuration (if present)

## Launch Contract

The final output of NAH composition.

**Contains:**
- `app`: id, version, root, entrypoint
- `nak`: id, version, root, resource root
- `execution`: binary, arguments, cwd, library paths
- `environment`: all environment variables
- `trust`: state, source, evaluation timestamp
- `warnings`: any issues encountered during composition

The contract is deterministic: given the same inputs (manifest, install records, profile), NAH produces the same contract.

### Viewing a Contract

```bash
nah status com.example.myapp
```

With full provenance trace:
```bash
nah status com.example.myapp --trace
```

JSON output for scripting:
```bash
nah status com.example.myapp --json
```

## On-Disk Layout

A NAH root has this structure:

```
/nah/
├── host/
│   └── host.json             # Host environment configuration
├── apps/
│   └── <id>-<version>/       # Installed app payloads
├── naks/
│   └── <nak_id>/<version>/   # Installed NAKs
└── registry/
    ├── apps/                 # App install records
    └── naks/                 # NAK install records
```

## Version Requirements

NAH uses [SemVer 2.0.0](https://semver.org/) for versioning.

| Syntax | Meaning |
|--------|---------|
| `1.2.3` | Exactly 1.2.3 |
| `>=1.2.0` | 1.2.0 or higher |
| `>=1.0.0 <2.0.0` | 1.x (space = AND) |
| `>=1.0.0 <2.0.0 \|\| >=3.0.0` | 1.x or 3.0+ (pipe = OR) |

Comparators: `=`, `<`, `<=`, `>`, `>=`

## Further Reading

- [SPEC.md](../SPEC.md) - Complete normative specification
- [CLI Reference](cli.md) - Command-line tool documentation
