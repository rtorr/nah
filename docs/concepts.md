# Core Concepts

This document defines the key terms and artifacts in NAH.

## Overview

NAH separates concerns between three parties:

| Party | Owns | Artifact |
|-------|------|----------|
| App Developer | What the app needs | App Manifest |
| SDK Developer | What the SDK provides | NAK (Native App Kit) |
| Host Platform | Where things go, what's allowed | Host Profile |

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

Example (embedded in C++):
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

## NAK (Native App Kit)

A versioned SDK, runtime, or framework that apps depend on at launch.

**Owner:** SDK developer  
**Mutability:** Immutable after packaging  
**Format:** `.nak` archive with `META/nak.toml`

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

Example `META/nak.toml`:
```toml
schema = "nah.nak.pack.v1"

[nak]
id = "com.example.sdk"
version = "2.1.0"

[paths]
resource_root = "resources"
lib_dirs = ["lib"]

[environment]
SDK_VERSION = "2.1.0"
```

### Loader (Optional)

A NAK may include a loader binary that wraps app execution:

```toml
[loader]
exec_path = "bin/sdk-loader"
args_template = ["--app", "{NAH_APP_ENTRY}"]
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
- Optional `META/package.toml` with metadata

NAPs are installed by the host. At install time, NAH selects a compatible NAK version and pins it in the App Install Record.

## Host Profile

The host's configuration for NAH behavior.

**Owner:** Host platform  
**Mutability:** Mutable by host  
**Format:** TOML file in `host/profiles/`

**Controls:**
- NAK binding mode (canonical or mapped)
- Allowed/denied NAK versions
- Environment overrides
- Warning policy (warn, ignore, error)
- Capability mappings

Example:
```toml
schema = "nah.host.profile.v1"

[nak]
binding_mode = "canonical"
allow_versions = ["2.*"]

[environment]
DEPLOYMENT_ENV = "production"

[warnings]
nak_not_found = "error"
```

### Binding Modes

| Mode | Behavior |
|------|----------|
| `canonical` | Select highest version satisfying the app's requirement |
| `mapped` | Use explicit version mappings defined in `nak.map` |

## App Install Record

Per-installation state created when an app is installed.

**Owner:** Host  
**Mutability:** Mutable by host  
**Format:** TOML in `registry/installs/`

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
**Format:** TOML in `registry/naks/`

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
nah --root /nah contract show com.example.myapp
```

JSON output for scripting:
```bash
nah --root /nah --json contract show com.example.myapp
```

## On-Disk Layout

A NAH root has this structure:

```
/nah/
├── host/
│   ├── profiles/
│   │   └── default.toml      # Host profile
│   └── profile.current       # Symlink to active profile
├── apps/
│   └── <id>-<version>/       # Installed app payloads
├── naks/
│   └── <nak_id>/<version>/   # Installed NAKs
└── registry/
    ├── installs/             # App install records
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
