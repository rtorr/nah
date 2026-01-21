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

**Owner:** App developer\
**Mutability:** Immutable after build\
**Format:** JSON (`nap.json` in package root)

**Contains:**

* Identity: `id`, `version`
* NAK requirement: `nak_id`, `nak_version_req`
* Entrypoint: path to binary relative to app root
* Layout: lib dirs, asset dirs
* Environment defaults
* Metadata (description, author, license, homepage)
* Asset exports

**Does not contain:**

* Host filesystem paths
* Host policy or trust decisions
* NAK install locations

### Manifest Structure

All apps use a JSON manifest at the root of the package:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v1.json",
  "app": {
    "identity": {
      "id": "com.example.myapp",
      "version": "1.0.0",
      "nak_id": "com.example.sdk",
      "nak_version_req": ">=2.0.0 <3.0.0"
    },
    "execution": {
      "entrypoint": "bin/myapp"
    },
    "layout": {
      "lib_dirs": ["lib"],
      "asset_dirs": ["assets"]
    },
    "environment": {
      "MY_VAR": "default_value"
    }
  },
  "metadata": {
    "description": "Example application",
    "author": "Your Name",
    "license": "MIT",
    "homepage": "https://example.com"
  }
}
```

The `$schema` field enables validation and IDE autocompletion. See [docs/schemas/README.md](schemas/README.md) for the complete schema documentation.

## NAK (Native App Kit)

A versioned SDK, runtime, or framework that apps depend on at launch.

**Owner:** SDK developer\
**Mutability:** Immutable after packaging\
**Format:** `.nak` archive (tar.gz) with `nak.json` at root

**Contains:**

* Identity: `id`, `version`
* Libraries (shared objects, dylibs, DLLs)
* Resources
* Environment defaults
* Optional loader binary

**Does not contain:**

* App identity or entrypoints
* Host policy
* Install locations

NAKs are installed on the host and referenced by apps via `nak_id` and a version requirement. Multiple versions of the same NAK can coexist.

Example `nak.json`:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nak.v1.json",
  "nak": {
    "identity": {
      "id": "com.example.sdk",
      "version": "2.1.0"
    },
    "layout": {
      "resource_root": "resources",
      "lib_dirs": ["lib"]
    },
    "environment": {
      "SDK_VERSION": "2.1.0"
    }
  },
  "metadata": {
    "description": "Example SDK",
    "author": "Vendor Name",
    "license": "Apache-2.0"
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

**Owner:** App developer\
**Format:** `.nap` archive (standard tar.gz)

**Contains:**

* Application binary and dependencies
* `nap.json` manifest at root
* Libraries (in `lib/` if specified)
* Assets (in `assets/` or as specified)

NAPs are installed by the host. At install time, NAH selects a compatible NAK version and pins it in the App Install Record.

## Host Environment

The host's configuration for NAH behavior.

**Owner:** Host platform\
**Mutability:** Mutable by host\
**Format:** JSON file at `host/nah.json`

**Controls:**

* Environment variables to inject
* Library path modifications (prepend/append)
* Override policy (allow/deny environment overrides)
* Packages to auto-install

Example:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nah.v1.json",
  "host": {
    "root": "/opt/nah",
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
    },
    "install": [
      "packages/sdk.nak",
      "packages/app.nap"
    ]
  }
}
```

The host environment is simple - it provides environment variables and library paths. NAK selection happens at install time based on the app's version requirements.

## App Install Record

Per-installation state created when an app is installed.

**Owner:** Host\
**Mutability:** Mutable by host\
**Format:** JSON in `registry/installs/`

**Contains:**

* App identity and version
* Install location
* Pinned NAK (id, version, record reference)
* Provenance (source, hash, install timestamp)
* Trust state
* Per-install overrides

The pinned NAK is selected at install time and does not change unless the app is reinstalled.

## NAK Install Record

Per-installation state for an installed NAK.

**Owner:** Host\
**Mutability:** Mutable by host\
**Format:** JSON in `registry/naks/`

**Contains:**

* NAK identity and version
* Install paths (root, lib dirs, resource root)
* Environment defaults
* Loader configuration (if present)

## Launch Contract

The final output of NAH composition.

**Contains:**

* `app`: id, version, root, entrypoint
* `nak`: id, version, root, resource root
* `execution`: binary, arguments, cwd, library paths
* `environment`: all environment variables
* `trust`: state, source, evaluation timestamp
* `warnings`: any issues encountered during composition

The contract is deterministic: given the same inputs (manifest, install records, profile), NAH produces the same contract.

### Viewing Details

```bash
nah show com.example.myapp
```

To run the application:

```bash
nah run com.example.myapp
```

JSON output for scripting:

```bash
nah show com.example.myapp --json
```

## On-Disk Layout

A NAH root has this structure:

```
/nah/
├── host/
│   └── nah.json                 # Host environment configuration
├── apps/
│   └── <id>-<version>/          # Installed app payloads
│       ├── nap.json             # App manifest
│       ├── bin/                 # Binaries
│       ├── lib/                 # Libraries
│       └── assets/              # Assets
├── naks/
│   └── <nak_id>/<version>/      # Installed NAKs
│       ├── nak.json             # NAK manifest
│       ├── lib/                 # SDK libraries
│       └── resources/           # SDK resources
└── registry/
    ├── apps/                    # App install records
    └── naks/                    # NAK install records
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

* [SPEC.md](../SPEC.md) - Complete normative specification
* [CLI Reference](cli.md) - Command-line tool documentation
