# NAH - Native Application Host Complete Specification

Version 1.0

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Why NAH Exists](#why-nah-exists)
3. [Core Concepts and Architecture](#core-concepts-and-architecture)
4. [Definitions and Ownership](#definitions-and-ownership)
5. [On-Disk Layout](#on-disk-layout)
6. [Host Environment Format](#host-environment-format)
7. [Path Normalization and Root Containment (Normative)](#path-normalization-and-root-containment-normative)
8. [Contract Composition](#contract-composition)
9. [NAK Selection](#nak-selection)
10. [Security Model Integration](#security-model-integration)
11. [Binary Manifest Format](#binary-manifest-format)
12. [API Surface](#api-surface)
13. [CLI Reference](#cli-reference)
14. [Developer Flows](#developer-flows)
15. [Implementation Architecture](#implementation-architecture)
16. [Platform Support](#platform-support)
17. [NAP Package Format (Normative)](#nap-package-format-normative)
18. [NAK Pack Format (Normative)](#nak-pack-format-normative)
19. [Build-Time Remote Materialization (Normative)](#build-time-remote-materialization-normative)
20. [Non-Goals and Invariants](#non-goals-and-invariants)
21. [Versioning](#versioning)
22. [Conformance](#conformance)
23. [Conclusion](#conclusion)

---

## Executive Summary

NAH (Native Application Host) standardizes how native applications are installed, inspected, and launched in environments where application binaries must remain portable while hosts retain full control over policy, layout, and enforcement.

### The Problem

Native platforms repeatedly fail when launch behavior becomes an emergent property of scattered scripts, ad-hoc environment assumptions, and host-specific glue. This produces:

- Applications tied to specific filesystem layouts or host environments
- Runtime dependencies and launch wiring drift across machines and over time
- Trust/provenance handled inconsistently across install and launch
- Launch behavior that cannot be audited as a single contract
- Drift between what developers expect and what actually launches

The root cause is structural: build-time declarations, install-time state, launch-time wiring, and host policy are mixed together.

### The Approach

NAH fixes this by making launch behavior a deterministic composition of strictly owned inputs:

- Applications provide an immutable, host-agnostic declaration of intent and requirements.
- Hosts provide mutable policy, bindings, and per-install-instance state.
- Composition produces one concrete result: a launch contract that can be executed directly and audited.
- NAH provides mechanism and traceability; hosts provide policy and enforcement.

### The Outcome

NAH answers one precise question:

> Given this application, installed here, under this host's rules — how must it be launched?

The output is a directly executable Launch Contract (binary, argv, cwd, environment, library paths, enforcement IDs), plus structured warnings/diagnostics.

### Artifacts and Ownership

NAH enforces separation through exactly four artifacts:

1. **App Manifest** — Application-authored portable contract (immutable; embedded in binary or provided as `manifest.nah`)
2. **App Install Record** — Host-owned per-install-instance state and provenance (mutable; JSON)
3. **NAK Install Record** — Host-owned record describing the installed Native App Kit used at launch (mutable; JSON)
4. **Host Environment** — Host-owned configuration for environment variables, library paths, and override policy (mutable; JSON)

Note: the Native App Kit (NAK) is the host-installed, versioned set of shared runtime components and optional loader wiring that applications target at launch (defined normatively later in this spec).

### Key Properties

- **Minimal mechanism**: NAH composes and reports; hosts enforce.
- **Deterministic binding**: selection occurs once at install time and is pinned; launch composition uses the pin only.
- **Auditable launch contract**: launch behavior is represented as data, not scattered glue.
- **Portable application payloads**: no host paths or host policy embedded in application declarations.
- **Policy evolves without rebuilds**: hosts can update bindings and enforcement without changing apps.

---

## Why NAH Exists

### The Problem NAH Solves

Modern native application platforms repeatedly run into the same structural failures:

- Applications bake in assumptions about the host environment
- Native App Kit (NAK) layout/configuration and policy drift over time
- Trust and security decisions are made implicitly and inconsistently
- Launch behavior is defined by scattered glue code instead of a contract
- Operational changes require rebuilds instead of policy updates

These failures happen because **build-time concerns, install-time state, launch-time wiring, and host policy concerns are mixed together**. NAH exists to separate them — permanently and explicitly.

### The Core Insight

> **Applications cannot know the host they will run on. Hosts cannot allow applications to define host behavior.**

Any system that violates either side of that statement becomes fragile. NAH is designed to enforce this separation structurally, not culturally.

### The Three Forces NAH Reconciles

#### 1. Application Portability

Applications want to be built once, tested in isolation, and deployed unchanged across environments. This requires a **portable contract** that does not encode host-specific knowledge.

#### 2. Host Control and Policy

Hosts need to control NAK layout, enforce security policy, inject required environment, audit trust and provenance, and evolve independently of apps. This requires **host-owned state and bindings** that do not require rebuilding applications.

#### 3. Native App Kit (NAK) Evolution

Framework teams need to ship self-contained, versioned Native App Kits (NAKs), roll versions forward and backward, define compatibility guarantees, and support multiple hosts with different constraints. This requires a **stable binding model** between apps, NAKs, and hosts.

### The Guarantee NAH Provides

> **An application can be developed, built, and packaged in isolation from any host environment.**

At the same time:

> **A host can change policy, NAK layout, and enforcement without rebuilding applications.**

NAH guarantees both — and refuses to compromise either.

---

## Core Concepts and Architecture

### System Overview

NAH is built on these design principles:

- **Minimal mechanism**: NAH provides mechanism, not policy
- **Default permissive**: NAH continues execution and records warnings for non-critical missing/invalid data; composition halts only on CriticalError conditions defined in this specification.
- **Binary manifests, text configuration**: Embedded manifests use TLV, operational config uses JSON
- **Platform-native**: Direct binary format integration (ELF, Mach-O sections)
- **Debuggable**: All configuration readable with standard text tools
- **Escape hatches**: Environment overrides (`NAH_OVERRIDE_*`) are local-only and MAY override configuration only when permitted by Host Environment override policy. Overrides MUST NOT trigger any network access or artifact installation.

### Architecture Layers

```
┌─────────────────────────────────────────┐
│         Application Layer               │
│   (User applications with embedded NAH manifest) │
├─────────────────────────────────────────┤
│         Library Layer                   │
│   (Core NAH headers and types)          │
├─────────────────────────────────────────┤
│         Host Tools Layer                │
│   (CLI tools for deployment/launch)     │
├─────────────────────────────────────────┤
│         Platform Layer                  │
│   (OS-specific implementations)         │
└─────────────────────────────────────────┘
```

### The Core Invariant

> **Applications declare intent and requirements. Hosts declare bindings and enforcement. NAH composes them deterministically.**

If an implementation violates that rule, it is incorrect.

---

## Definitions and Ownership

### App Manifest

**Owner:** Application developer  
**Mutability:** Immutable after build  
**Storage:** Embedded in binary via NAH_APP_MANIFEST(...) OR as manifest.nah file in app payload  
**Format:** TLV binary (Tag-Length-Value)

**What it is (Normative):** The application-authored, immutable contract that declares identity, layout, and the required NAK version constraint.

**Contains (Normative):**

- Identity (`id`, `version`)
- NAK requirement (`nak_id`, `nak_version_req`)
- Entrypoint relative to the app root
- Internal layout (lib dirs, asset dirs)
- Default environment values
- Declared capabilities/permissions

ASSET_DIR entries are advisory for tooling/validation only and MUST NOT produce any Launch Contract fields.

**MUST NOT contain:**

- Host filesystem paths (except relative paths under app root)
- Host environment variables
- Host policy decisions or trust state
- Compose-time selection logic

When decoded, `ENTRYPOINT_PATH` MUST be represented as `entrypoint_relative_path` in the in-memory manifest model.

**Developer toolchains (headers, compilers, codegen) are out of scope for contract composition.** The manifest's `nak_version_req` refers only to the NAK pack version requirement used at launch.

**NAK version requirement (Normative):**

- `nak_id` is the NAK identifier required by the application.
- `nak_version_req` is a NAK version requirement string.
- `nak_version_req` MUST be parseable as a SemVer requirement (see SemVer Requirement (Normative)).
- If parsing fails, NAH MUST emit `invalid_manifest` and treat NAK as unresolved per warning policy.

**Standalone Apps (Normative):**

- `nak_id` is OPTIONAL. If `nak_id` is missing or empty, the app is a standalone app with no NAK dependency.
- Standalone apps skip NAK resolution entirely; no NAK-derived environment variables or library paths are set.
- This enables simple applications that don't require a runtime SDK.

### Native App Kit (NAK)

**What it is (Normative):** A host-installed, versioned distribution of shared components and optional loader wiring that an app depends on at launch.

**Contains (Normative):**

- Libraries
- Optional resources
- Optional loader executable + args template
- Default environment values

**MUST NOT contain:**

- App identity or entrypoints
- Host policy or trust decisions
- Compose-time selection logic
- Compose-time network requirements

**Libs-only NAK (Normative):** The `[loader]` section is OPTIONAL. A NAK MAY consist only of libraries/resources/environment defaults.

**NAK vs SDK/toolchains (Normative):** A NAK is the on-device distribution plus metadata; build-time SDK/toolchains are out of scope.

**Role in NAH (Normative):** Apps declare NAK requirements in the App Manifest. The host selects and pins a specific NAK version at install time. Contract composition uses only the pinned NAK Install Record.

### SemVer Requirement (Normative)

NAH uses [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html) for NAK versions. This section specifies version parsing, comparison, and range satisfaction semantics.

#### Version Format

A valid SemVer version string has the form:

```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
```

Where:
- `MAJOR`, `MINOR`, `PATCH` are non-negative decimal integers with no leading zeros
- `PRERELEASE` (optional) is a series of dot-separated identifiers (alphanumeric and hyphen)
- `BUILD` (optional) is a series of dot-separated identifiers (alphanumeric and hyphen)

Examples:
- `1.0.0` - release version
- `1.0.0-alpha.1` - pre-release version
- `1.0.0+build.123` - release with build metadata
- `1.0.0-beta.2+build.456` - pre-release with build metadata

#### Version Comparison (Normative)

Per SemVer 2.0.0 specification:

1. Compare by `(MAJOR, MINOR, PATCH)` as integers, left to right
2. A pre-release version has **lower** precedence than its release version: `1.0.0-alpha < 1.0.0`
3. Pre-release identifiers compare: numeric identifiers by integer value, alphanumeric identifiers lexically, numeric < alphanumeric, shorter < longer when prefixes match
4. Build metadata is **ignored** for comparison purposes

#### Version Range Syntax (Normative)

A version range specifies which versions satisfy a requirement. NAH uses standard comparator-based range syntax:

**Comparators:**
- `=X.Y.Z` or `X.Y.Z` - exact match (equal to)
- `<X.Y.Z` - less than
- `<=X.Y.Z` - less than or equal
- `>X.Y.Z` - greater than
- `>=X.Y.Z` - greater than or equal

**Combining Comparators:**
- Space-separated comparators are AND'd together: `>=1.0.0 <2.0.0` means both must be satisfied
- `||` separates alternative sets (OR): `>=1.0.0 <2.0.0 || >=3.0.0` means either set can satisfy

**Examples:**
- `1.0.0` - exactly version 1.0.0
- `>=1.0.0` - version 1.0.0 or higher
- `>=1.0.0 <2.0.0` - version 1.0.0 up to (but not including) 2.0.0
- `>=1.0.0 <2.0.0 || >=3.0.0 <4.0.0` - 1.x or 3.x versions

Whitespace around comparators and `||` MUST be trimmed. Leading/trailing whitespace in the entire requirement string MUST be trimmed before parsing.

#### Range Satisfaction (Normative)

A version `v` satisfies a range if:
1. For a single comparator: `v` meets the comparison condition
2. For a comparator set (AND): `v` satisfies ALL comparators in the set
3. For multiple sets (OR): `v` satisfies AT LEAST ONE comparator set

**Pre-release Handling:**
Pre-release versions satisfy ranges following SemVer 2.0.0 comparison rules. A pre-release version like `1.0.0-alpha.1` is less than `1.0.0` and satisfies `>=0.9.0 <1.0.0` but not `>=1.0.0`.

#### Mapped-mode min_version and selection_key (Normative)

For mapped binding mode, NAH derives a `selection_key` from the version range:

1. `min_version` is the minimum version that could satisfy the range (from the first comparator set's lower bound)
2. `selection_key = "<min_version.MAJOR>.<min_version.MINOR>"`

Examples:
- `>=1.2.3 <2.0.0` → min_version=1.2.3, selection_key="1.2"
- `>=3.0.0` → min_version=3.0.0, selection_key="3.0"
- `>=1.0.0 <2.0.0 || >=3.0.0` → min_version=1.0.0 (from first set), selection_key="1.0"

#### Error Handling (Normative)

If parsing fails or the range syntax is invalid, NAH MUST emit `invalid_manifest` and treat the NAK as unresolved per warning policy.

### App Install Record

**Owner:** Host
**Mutability:** Mutable (updated by host tooling)
**Storage:** Host registry (`<nah_root>/registry/installs/`)
**Format:** JSON text format

**What it is (Normative):** The host-owned record of a specific installed app instance, including the pinned NAK and host-owned trust/provenance.

**Contains (Normative):**

- Install identity (app id, app version, install instance id)
- Resolved install root path (host filesystem path)
- **Pinned NAK selection** (`[nak]` id/version/record_ref)
- Optional audit-only NAK selection reason
- Install timestamps and provenance
- Trust evaluation results (signature present/valid, signer, method, hashes)
- Optional host-local overrides applied at install time
- Verification history (last verified time, verifier version)

**MUST NOT contain:**

- Any field that changes the meaning of the App Manifest
- Any host environment configuration (those remain in Host Environment)
- Any NAK wiring (those remain in NAK Install Record)
- Any app-authored configuration (those remain in the app payload)

The App Manifest remains the authoritative source for app identity and NAK requirements during composition; App Install Record `[app]` fields are audit snapshots only.


**Normative Fields:**

```json
{
  "install": {
    "instance_id": "uuid-string"
  },
  "app": {
    "id": "com.example.app",
    "version": "1.2.3",
    "nak_id": "com.example.nak",
    "nak_version_req": ">=3.0.0 <4.0.0"
  },
  "nak": {
    "id": "com.example.nak",
    "version": "3.0.2",
    "record_ref": "com.example.nak@3.0.2.json"
  },
  "paths": {
    "install_root": "/nah/apps/com.example.app-1.2.3"
  },
  "provenance": {
    "package_hash": "sha256:...",
    "installed_at": "2024-01-15T10:30:00Z",
    "installed_by": "user",
    "source": "package.nap"
  },
  "trust": {
    "state": "verified",
    "source": "corp-verifier",
    "evaluated_at": "2025-12-30T16:21:00Z",
    "expires_at": "2026-01-30T16:21:00Z",
    "inputs_hash": "sha256:...",
    "details": {
      "method": "codesign",
      "signer": "Developer ID ...",
      "signature_present": true,
      "signature_valid": true
    }
  },
  "verification": {
    "last_verified_at": "2024-01-15T10:30:00Z",
    "last_verifier_version": "1.0.0"
  },
  "overrides": {
    "environment": {},
    "arguments": {},
    "paths": {}
  }
}
```

**Required fields for composition (Normative):** `[install].instance_id` and `[paths].install_root` MUST be present per Presence semantics. Missing or empty values MUST produce CriticalError::INSTALL_RECORD_INVALID.
`[app]` fields are audit snapshots only and MUST NOT affect behavior during composition.
`[nak].record_ref` MAY be absent; if missing or empty, NAH MUST emit `nak_pin_invalid` and treat the NAK as unresolved instead of raising a CriticalError.

The entrypoint path is derived during composition from `paths.install_root` + `manifest.entrypoint_relative_path` and MUST NOT be persisted in App Install Records.

### NAK Install Record

**Owner:** Host
**Mutability:** Mutable (updated by host tooling)
**Storage:** Host registry (`<nah_root>/registry/naks/`)
**Format:** JSON text format

**What it is (Normative):** The host-owned record of an installed NAK pack that defines loader wiring (if any), resource roots, and default environment for composition.

**Contains (Normative):**

- NAK identity (`[nak]` id/version)
- Resolved absolute NAK paths (root, resource_root, lib_dirs)
- Optional loader wiring (`[loader]`)
- Default environment values
- Optional execution templates (cwd)
- Provenance metadata

**MUST NOT contain:**

- App-specific identity or entrypoints
- Host policy or enforcement configuration
- Any app-authored configuration
- Compose-time selection logic

**Loader Optionality (Normative):**

- `[loader]` is OPTIONAL. If absent, composition MUST use the app entrypoint as `execution.binary` and MUST NOT apply `loader.args_template`.

**Execution Optionality (Normative):**

- `[execution]` is OPTIONAL. If absent, contract composition MUST default `execution.cwd` to `app.root`.


**Normative Fields:**

```json
{
  "nak": {
    "id": "com.example.nak",
    "version": "3.0.2"
  },
  "paths": {
    "root": "/nah/naks/com.example.nak/3.0.2",
    "resource_root": "/nah/naks/com.example.nak/3.0.2/resources",
    "lib_dirs": [
      "/nah/naks/com.example.nak/3.0.2/lib",
      "/nah/naks/com.example.nak/3.0.2/lib64"
    ]
  },
  "environment": {},
  "loader": {
    "exec_path": "/nah/naks/com.example.nak/3.0.2/bin/nah-runtime",
    "args_template": [
      "--app", "{NAH_APP_ENTRY}",
      "--root", "{NAH_APP_ROOT}",
      "--id", "{NAH_APP_ID}",
      "--version", "{NAH_APP_VERSION}",
      "--nak", "{NAH_NAK_ROOT}"
    ]
  },
  "execution": {
    "cwd": "{NAH_APP_ROOT}"
  },
  "provenance": {}
}
```

**Template expansion (Normative):**
If present, `args_template` and `execution.cwd` are compose-time templates and MUST be expanded during contract composition using `effective_environment`.
Placeholders are exact tokens of the form `{NAME}` and are subject to the expansion rules defined in the Composition Algorithm.

**Path rules (Normative):**

- `paths.root` MUST be absolute.
- `paths.resource_root` MUST be absolute and resolve under `paths.root`. If absent or empty, it MUST be treated as `paths.root`.
- `paths.lib_dirs` entries MUST be absolute and resolve under `paths.root`.
- If `[loader]` is present, `loader.exec_path` MUST be absolute and resolve under `paths.root`.
- If `[execution]` is present and `execution.cwd` is present, it is expanded at composition time; if the result is relative, it MUST be resolved against `paths.root`.

**Required fields for composition (Normative):** `[nak].id`, `[nak].version`, and `[paths].root` MUST be present per Presence semantics. Missing or empty values MUST emit `nak_pin_invalid` and mark the NAK as unresolved (no CriticalError).

### Trust State (Normative)

The `[trust]` section is host-authored, mutable, and install-instance specific. NAH does not compute trust; it only persists and surfaces it.

- If `[trust]` is absent, state MUST be treated as "unknown", NAH MUST emit `trust_state_unknown`, and `trust_state_stale` MUST NOT be emitted.
- If `[trust].state` is present but not one of {"verified","unverified","failed","unknown"}, NAH MUST emit `invalid_trust_state`, MUST treat state as "unknown", and MUST then emit `trust_state_unknown`.
- If `[trust].state` == "unverified", NAH MUST emit `trust_state_unverified`.
- If `[trust].state` == "failed", NAH MUST emit `trust_state_failed`.
- If `[trust].state` == "verified", NAH MUST emit no trust-state warning (unless stale).
- If `[trust].expires_at` exists and is earlier than now and `[trust]` is present, NAH MUST emit `trust_state_stale` regardless of state value.
- Only `trust.state`, `trust.source`, `trust.evaluated_at`, and `trust.expires_at` influence warning emission; `trust.details` MUST NOT influence warnings or behavior.
- NAH MUST NOT interpret `trust.details` keys beyond display/serialization.
- Trust state MUST be one of: "verified", "unverified", "failed", "unknown".
- Trust source identifies which host component wrote this trust state.

### App Install Record Update Rules (Normative)

App Install Records MUST be written atomically using: temp file + fsync(file) + rename + fsync(directory).

App Install Records MUST be append-only in meaning: host tooling MAY add fields and update timestamps and trust evaluation, but MUST NOT modify `[app].id`, `[app].version`, `[app].nak_id`, `[app].nak_version_req`, or `[install].instance_id` after creation.
Host tooling MUST NOT modify `[nak].id`, `[nak].version`, or `[nak].record_ref` after creation if `[nak].record_ref` is present and non-empty. If `[nak].record_ref` is absent or empty, host tooling MAY set it exactly once as part of completing an install that previously recorded an unresolved NAK; after it becomes non-empty it is immutable.

### Trust Timestamp Format (Normative)

- `evaluated_at` and `expires_at` MUST be JSON offset datetime values (RFC3339-compatible)
- When serialized in JSON output / Launch Contract, they MUST be RFC3339 strings

Derived output-only paths MUST be recomputable from (`paths.install_root` + `manifest.entrypoint_relative_path`).

### Host Environment

**Owner:** Framework team / host integrator
**Mutability:** Mutable, auditable
**Storage:** `<nah_root>/host/host.json`
**Format:** JSON text format

**What it is (Normative):** The host-owned configuration document that provides environment variables, library paths, and override policy for contract composition.

**Contains (Normative):**

- Default environment values
- Library path prepend/append lists
- Override policy

**MUST NOT contain:**

- App-specific identity, versioning, or entrypoints
- NAK selection rules (NAK selection happens at install time)
- Trust decisions or verification outputs
- Warning policy configuration (all warnings default to "warn")
- Capability-to-enforcement mapping (removed feature)

---

## On-Disk Layout

### Canonical Paths (Normative)

```
/nah/
├── apps/
│   └── <id>-<version>/
│       ├── bin/                    # Binaries with embedded manifest
│       ├── lib/                    # Libraries
│       └── share/                  # Assets
├── naks/
│   └── <nak_id>/<version>/     # NAK root
├── host/
│   └── host.json                  # Host Environment configuration
└── registry/
    ├── apps/
    │   └── <id>@<version>.json            # App Install Record
    ├── naks/
    │   └── <nak_id>@<version>.json        # NAK Install Record
    └── locks/                    # Host-only lock files (implementation-defined)
```

**Registry Semantics (Normative):**

- The authoritative source of app installation state is the set of App Install Records in `<nah_root>/registry/apps/`
- The authoritative source of NAK installations is `<nah_root>/registry/naks/`
- Multiple versions of the same NAK MAY be installed simultaneously
- Registry updates MUST be atomic using the same procedure defined for Host Environment updates (temp + fsync + rename + fsync directory)

### Atomic Update Requirements

All updates to Host Environment MUST be atomic:

1. Write to temporary file
2. fsync() temporary file
3. rename() to final location
4. fsync() directory

---

## Host Environment Format

### Host Environment Responsibilities (Normative)

The Host Environment captures host-owned configuration that an app cannot know:

- Default environment variables to inject into the launch environment
- Library path modifications (prepend/append)
- Override policy controlling which `NAH_OVERRIDE_*` environment variables are permitted

The Host Environment MUST NOT contain:

- App-specific identity, versioning, or entrypoints
- NAK installation roots (registry records are authoritative)
- NAK selection rules (NAK selection happens at install time only)
- Warning policy configuration (all warnings default to "warn")
- Capability-to-enforcement mapping (removed feature)

### Host Environment Loading (Normative)

NAH MUST load the Host Environment as follows:

1. Attempt to load `<nah_root>/host/host.json`
2. If the file exists and is readable, parse it as JSON
3. If the file is missing, use the Built-in Empty Host Environment
4. If the file exists but fails to parse, emit `host_env_parse_error` and use the Built-in Empty Host Environment

### Built-in Empty Host Environment (Normative)

```json
{
  "environment": {},
  "paths": {
    "library_prepend": [],
    "library_append": []
  },
  "overrides": {
    "allow_env_overrides": true,
    "allowed_env_keys": []
  }
}
```

**Default Warning Action (Normative):** All warnings default to `"warn"`. There is no configurable warning policy in the Host Environment.

### Host Environment JSON Format (Normative)

```json
{
  "environment": {
    "NAH_HOST_VERSION": "1.0",
    "NAH_HOST_MODE": "production"
  },
  "paths": {
    "library_prepend": ["/opt/host/lib"],
    "library_append": ["/usr/local/lib"]
  },
  "overrides": {
    "allow_env_overrides": true,
    "allowed_env_keys": []
  }
}
```

**Fields (Normative):**

- `environment` (optional): A map of environment variable names to values. These are merged as defaults (fill-only) during contract composition.
- `paths.library_prepend` (optional): A list of absolute library paths to prepend to the library path list. Defaults to empty list.
- `paths.library_append` (optional): A list of absolute library paths to append to the library path list. Defaults to empty list.
- `overrides.allow_env_overrides` (optional): Boolean indicating whether `NAH_OVERRIDE_ENVIRONMENT` is permitted. Defaults to `true`.
- `overrides.allowed_env_keys` (optional): Reserved for future use. Currently ignored.

**Override Policy (Normative):**

- If `overrides.allow_env_overrides` is `true` (the default), `NAH_OVERRIDE_ENVIRONMENT` is permitted.
- If `overrides.allow_env_overrides` is `false`, `NAH_OVERRIDE_ENVIRONMENT` MUST be ignored and MUST emit `override_denied`.
- Supported override targets in v1.0 are only `NAH_OVERRIDE_ENVIRONMENT`. Any other `NAH_OVERRIDE_<X>` MUST be treated as not permitted, MUST emit `override_denied`, and MUST NOT emit `override_invalid`.

**Override Semantics (Normative):**

- `NAH_OVERRIDE_ENVIRONMENT` MUST be a JSON object string mapping environment keys to values.
  - The JSON value MUST decode to an object whose keys and values are strings.
  - When permitted by override policy, it MUST be merged into the effective_environment map at the final precedence layer.
  - Merge semantics: keys overwrite existing keys.
  - If permitted and JSON parsing fails, emit `override_invalid` (reason `parse_failure`) and ignore this override.
- Any other `NAH_OVERRIDE_<X>` MUST be treated as not permitted and MUST emit `override_denied` only.
- Override error priority is deterministic: if an override is not permitted by policy, emit `override_denied` and MUST NOT emit `override_invalid`. If permitted by policy but malformed/unknown/invalid, emit `override_invalid`.
- File-level overrides parse/shape failures (missing/unreadable/invalid overrides file) MUST emit `override_invalid` with target `OVERRIDES_FILE` regardless of override policy and MUST NOT emit `override_denied`.
- These override semantics apply equally to process environment overrides and file-based overrides.

### Example Host Environment

**host.json:**

```json
{
  "environment": {
    "NAH_HOST_VERSION": "1.0",
    "NAH_HOST_MODE": "production",
    "NAH_CLUSTER": "us-west-2"
  },
  "paths": {
    "library_prepend": ["/opt/monitoring/lib"],
    "library_append": ["/usr/local/lib"]
  },
  "overrides": {
    "allow_env_overrides": true
  }
}
```

---

## Path Normalization and Root Containment (Normative)

### Path Model and Terminology (Normative)

Canonical persisted paths (absolute):

- App Install Record: `paths.install_root` (absolute app root).
- NAK Install Record: `paths.root` (absolute NAK root), `paths.resource_root` (absolute, defaults to `paths.root`), `paths.lib_dirs` (absolute list).
- If `[loader]` is present in the NAK Install Record, `loader.exec_path` MUST be absolute and under `paths.root`.

Derived/output-only paths (absolute):

- Launch Contract `app.entrypoint`, `execution.binary`, `execution.cwd`, `execution.library_paths`.
- These are derived during composition and MUST NOT be persisted in App Install Records.

Environment keys:

- Standard keys (`NAH_APP_*`) MUST have exactly one derived value in the final environment map.
- `NAH_NAK_*` keys MUST be present only when the NAK is resolved, and when present MUST have exactly one derived value.

Manifest path fields remain relative:

- `ENTRYPOINT_PATH`, `LIB_DIR`, `ASSET_DIR`, and `ASSET_EXPORT` are relative and MUST be resolved under app root.

ASSET_DIR entries are validated for containment/symlink rules when resolved, but do not affect Launch Contract output.

Containment rules apply to canonical roots:

- All resolved app paths MUST be under `paths.install_root`.
- All resolved NAK paths MUST be under `paths.root`.
- Containment rules MUST NOT be applied to capability selectors.

Before any containment check, NAH MUST normalize paths by:

1. Rejecting any path segment containing NUL
2. Collapsing "." and ".." segments
3. Treating absolute paths in manifest fields as invalid
4. Rejecting any symlink encountered while resolving `ENTRYPOINT_PATH`, `LIB_DIR`, `ASSET_DIR`, or `ASSET_EXPORT` targets

For ENTRYPOINT_PATH, LIB_DIR, ASSET_DIR, and ASSET_EXPORT paths:

- The manifest-provided path MUST be relative (reject absolute paths before join)
- The resolved absolute path MUST be under `paths.install_root` after normalization
- Any manifest-provided relative path MUST be resolved using an approach that prevents escaping via symlinks (fd-relative openat style resolution with no-follow semantics, or equivalent platform APIs).
- If any symlink is encountered while resolving `ENTRYPOINT_PATH`, `LIB_DIR`, `ASSET_DIR`, or any `ASSET_EXPORT` target, it MUST produce CriticalError::PATH_TRAVERSAL.

For NAK Install Record paths:

- `paths.resource_root` and `paths.lib_dirs` MUST be absolute
- If `[loader]` is present, `loader.exec_path` MUST be absolute
- Each MUST resolve under `paths.root` after normalization
- Any resolution that escapes `paths.root` MUST produce CriticalError::PATH_TRAVERSAL

---

## Contract Composition

### Precedence Rules (Normative)

NAH MUST apply configuration inputs in the following precedence order, lowest to highest.
Sources categorized as “defaults” MUST be merged using **fill-only** semantics (set only if the key is unset).
Sources categorized as “overrides” MUST be merged using **overwrite** semantics (the incoming key replaces any prior value).

1. Host Environment defaults (**fill-only**)
2. NAK Install Record defaults (**fill-only**; only if `load_pinned_nak` returned `loaded == true`)
3. App Manifest defaults (**fill-only**)
4. App Install Record overrides (**overwrite**)
5. NAH standard variables (`NAH_APP_*`, `NAH_NAK_*`) (**overwrite**)
6. Process environment overrides (**overwrite** when permitted by override policy)
7. File-based overrides (**overwrite** when permitted by override policy)

If multiple inputs set the same key, the highest-precedence input MUST win.
`effective_environment` MUST be constructed once using this order and used for all placeholder expansion in composition.

The output of contract composition is the Launch Contract.

### Environment Algebra (Normative)

Environment values in Host Environment, NAK Install Record, and App Install Record overrides MAY specify an operation instead of a simple string value. This enables composable environment manipulation.

**Value Format:**

Environment values MUST be one of:

1. **String value** (simple set operation):
   ```json
   "environment": {
     "MY_VAR": "my_value"
   }
   ```

2. **Object with operation**:
   ```json
   "environment": {
     "PATH_VAR": { "op": "prepend", "value": "/new/path", "separator": ":" },
     "OTHER_VAR": { "op": "append", "value": "/extra", "separator": ":" },
     "REMOVE_ME": { "op": "unset" }
   }
   ```

**Operations (Normative):**

| Operation | Description | Fields |
|-----------|-------------|--------|
| `set` | Replace/set the value (default) | `value` (required) |
| `prepend` | Prepend value to existing with separator | `value` (required), `separator` (default `:`) |
| `append` | Append value to existing with separator | `value` (required), `separator` (default `:`) |
| `unset` | Remove the variable entirely | none |

**Application Rules:**

1. When `op` is absent, default to `set`.
2. When `separator` is absent, default to `:`.
3. For `prepend`: result is `<new_value><separator><existing_value>`. If no existing value, result is `<new_value>`.
4. For `append`: result is `<existing_value><separator><new_value>`. If no existing value, result is `<new_value>`.
5. For `unset`: the variable MUST be removed from `effective_environment`.
6. Operations apply in precedence order; each layer sees the result of all prior layers.

**Example Composition:**

Given:
- Host Environment: `"PATH": { "op": "set", "value": "/usr/bin" }`
- NAK: `"PATH": { "op": "prepend", "value": "/nak/bin" }`
- Install override: `"PATH": { "op": "append", "value": "/custom" }`

Result: `PATH=/nak/bin:/usr/bin:/custom`

### Algorithm (Normative)

**Inputs (Normative):**

0. NAH root ("nah_root"): the filesystem root selected by CLI --root, NAH_ROOT environment variable, or auto-detected (default `~/.nah`). All registry paths in this specification are relative to nah_root unless explicitly stated otherwise.
1. App Manifest (embedded in binary or `manifest.nah` within installed app root)
2. App Install Record (selected installed instance)
3. Pinned NAK Install Record (loaded only by `<nah_root>/registry/naks/<install_record.nak.record_ref>` when present)
4. Host Environment (from `<nah_root>/host/host.json`)
5. Current process environment (including NAH overrides)
6. Optional overrides file (JSON) supplied by the caller (e.g., `nah contract show --overrides`)
7. now: an RFC3339 timestamp representing the current time for composition. The CLI MUST default now to the system clock. now MUST be used ONLY for evaluating trust.expires_at staleness warnings and MUST NOT influence any other composition behavior.

**Output:**
Launch Contract (LaunchContract)

### Composition Algorithm (Normative)

**Presence semantics (Normative):**

- A JSON table is “present” if the table exists in the parsed document.
- A JSON key is “present” if the key exists in the parsed document.
- For **string** values: a key’s value is “present” only if the string is non-empty after trimming ASCII whitespace.
- For **arrays**: if the key is absent, treat it as an empty list; if present, preserve the element order exactly as in the document.
- For **datetime/boolean/integer/float**: if the key is present, the value is present (no “empty” concept).
- For optional maps (e.g., `[environment]`), absence means “empty map”.

1. **Load inputs**

- Load App Manifest, App Install Record as `install_record`, Host Environment as `host_env`, process environment, and now.

- If no manifest is found (neither embedded nor `manifest.nah`), emit CriticalError::MANIFEST_MISSING and abort composition.

- If a manifest is found but fails the CRC32 verification, emit CriticalError::MANIFEST_MISSING and abort composition.

- All other manifest invalidities are non-fatal: NAH MUST emit invalid_manifest and continue composition permissively by treating only the affected manifest fields as absent, using the rules below.

- If an overrides file is provided, load and parse it as JSON.
  - The document MUST decode to a JSON object whose only top-level keys are "environment" and/or "warnings".

  For either form:
  - environment MUST be a map/object of string keys to string values.
  - warnings MUST be a map/object of string keys to string values.
  - Any other top-level key/table, or any non-map/non-object shape for environment or warnings, is invalid.
  - If the file is missing, unreadable, fails to parse, does not decode to an object/table, or is invalid by the above rules, emit override_invalid (reason parse_failure or invalid_shape) and treat file overrides as empty.
  - For file-level failures, target MUST be OVERRIDES_FILE, source_kind MUST be overrides_file, and source_ref MUST be <file_path>.
  - For shape failures scoped to a subsection, source_ref MUST be <file_path>:environment or <file_path>:warnings.

- If the App Install Record is missing, unreadable, fails JSON parsing, has a missing/mismatched schema, or is missing required fields, emit CriticalError::INSTALL_RECORD_INVALID and MUST NOT produce a Launch Contract.

- If `[app].id`, `[app].version`, `[app].nak_id`, or `[app].nak_version_req` are present in the App Install Record and any differ from the corresponding values in the App Manifest (`manifest.id`, `manifest.version`, `manifest.nak_id`, `manifest.nak_version_req`), NAH MUST emit `invalid_configuration` with `fields.reason = "app_field_mismatch"`, `fields.source_path = "install_record.app"`, and `fields.fields` listing the differing field names as a comma-separated string; continue composition and treat the App Manifest as authoritative.

- If `install_record.nak` is missing or any of `install_record.nak.id`, `install_record.nak.version`, or `install_record.nak.record_ref` are missing or empty, emit `nak_pin_invalid` and treat the NAK as unresolved.
  **Normative:** `nak_pin_invalid` MUST be emitted only for missing/empty pin fields, pinned-record load/schema/required-field failures, or an invalid pinned NAK version string; incompatibility MUST emit `nak_version_unsupported` and MUST result in `load_pinned_nak.loaded == false`.

- If `install_record.nak.record_ref` is present, construct:
  - `pin.id = install_record.nak.id`
  - `pin.version = install_record.nak.version`
  - `pin.record_ref = install_record.nak.record_ref`

- Then attempt to load the pinned NAK via:
  - `PinnedNakLoadResult r = load_pinned_nak(pin, manifest)`

- If `r.loaded == true`, set `nak_record = r.nak_record` and treat the NAK as resolved for the remainder of composition. Otherwise, treat the NAK as unresolved and do not access `nak_record`.
  **Normative:** In this specification, “NAK resolved” means “a pinned NAK record is usable for composition,” i.e., `load_pinned_nak` returned `loaded == true`.

- `load_pinned_nak` performs the normative pinned-record validation and warning emission; composition MUST NOT re-validate the same pinned record fields a second time.

- If `r.loaded == false`, composition MUST treat the NAK as unresolved and MUST NOT emit `nak_not_found`. Any warnings emitted by `load_pinned_nak` MUST be preserved subject to warning policy.

- NAK selection MUST NOT occur at composition time; only the pinned record MAY be used.

2. **Derive app fields**
   - `app.id` = manifest id.
   - `app.version` = manifest version.
   - If `manifest.id` or `manifest.version` are absent due to manifest invalidity handling, NAH MUST treat the missing value as the empty string for environment variables and trace output. This MUST NOT introduce any new CriticalError.
   - `app.root` = `install_record.paths.install_root`.
   - If `manifest.entrypoint_relative_path` is absent or is an empty string after trimming ASCII whitespace, NAH MUST emit `invalid_manifest` and abort composition with CriticalError::ENTRYPOINT_NOT_FOUND.
   - If `manifest.entrypoint_relative_path` is present but is an absolute path, NAH MUST emit `invalid_manifest` and abort composition with CriticalError::ENTRYPOINT_NOT_FOUND.
   - Resolve `app.entrypoint` by joining `app.root` with `manifest.entrypoint_relative_path` and applying no-symlink-escape resolution.
   - If resolution fails or escapes, emit CriticalError::PATH_TRAVERSAL.
   - If the entrypoint file does not exist, emit CriticalError::ENTRYPOINT_NOT_FOUND.

3. **Validate NAK requirement**

- Parse `manifest.nak_version_req` as a SemVer requirement (see SemVer Requirement (Normative)).
- If parsing fails, emit `invalid_manifest`.
- This step MUST NOT change whether the NAK is resolved; NAK resolution is determined solely by `load_pinned_nak` in Step 1.
- No lexicographic fallback is permitted.

4. **Derive NAK fields (if resolved)**
   - `nak.id` = `nak_record.nak.id`.
   - `nak.version` = `nak_record.nak.version`.
   - `nak.root` = `nak_record.paths.root`.
   - `nak.resource_root` = `nak_record.paths.resource_root` if set, else `nak_record.paths.root`.
   - `nak.record_ref` = `install_record.nak.record_ref`.
   - Validate `nak.resource_root` and `nak_record.paths.lib_dirs` are absolute and under `nak_record.paths.root`.
   - If `nak_record.loader` is present, `nak_record.loader.exec_path` MUST be absolute and under `nak_record.paths.root`.
   - If any NAK path escapes `nak_record.paths.root`, emit CriticalError::PATH_TRAVERSAL.
   - Schema and required-field validation of the pinned record is performed by `load_pinned_nak`; step 4 performs only composition-time derivations and path containment validation.

5. **Set standard environment variables**
   - `NAH_APP_ID`, `NAH_APP_VERSION`, `NAH_APP_ROOT`, `NAH_APP_ENTRY` MUST be set from derived app fields.
   - If NAK resolved, `NAH_NAK_ID`, `NAH_NAK_ROOT`, and `NAH_NAK_VERSION` MUST be set from derived NAK fields.
   - These keys MUST be set to the derived values regardless of prior inputs.

   **Encountered overrides (Normative):**
   - Encountered overrides include all process environment variables whose keys start with `NAH_OVERRIDE_` plus any overrides provided via `nah contract show --overrides <file>`.
   - Implementations MUST process encountered override keys in lexicographic order by override key within each override source.
   - Override source order MUST be deterministic: process environment overrides first, then file-based overrides (so file overrides win if both set).

6. **Build effective_environment (single merged map)**
   - Start with `host_env.environment` as **defaults (fill-only)**. If `host_env.environment` is absent, treat it as an empty map.
   - If NAK resolved, merge `nak_record.environment` as **defaults (fill-only)**. If `nak_record.environment` is absent, treat it as an empty map.
   - Merge manifest environment defaults as **defaults (fill-only)**. If manifest environment defaults are absent, treat them as an empty map.
   - Merge `install_record.overrides.environment` (**overwrite**). If `install_record.overrides.environment` is absent, treat it as an empty map.
   - Set standard NAH\_\* variables as defined in step 5 (**overwrite**).
   - Apply encountered overrides at final precedence ONLY when permitted by Host Environment override policy (**overwrite**). Only `NAH_OVERRIDE_ENVIRONMENT` may merge into `effective_environment`.
     - Override allow/deny MUST follow the Host Environment Override Policy rules.
     - Process environment overrides: for each `NAH_OVERRIDE_*` key in lexicographic order, if the target is `NAH_OVERRIDE_ENVIRONMENT`, its value MUST be a JSON object string. The JSON value MUST decode to an object whose keys and values are strings; when permitted it MUST be merged into `effective_environment` (keys overwrite existing keys). If permitted but JSON parsing fails, emit `override_invalid` (reason `parse_failure`) and ignore it.
     - File overrides: the `[environment]` table maps to `NAH_OVERRIDE_ENVIRONMENT` and MUST be a map of string keys to string values. If permitted and the shape is invalid, emit `override_invalid` (reason `invalid_shape`) and ignore it. When permitted, merge the map into `effective_environment` (keys overwrite existing keys).
     - If an encountered override is not permitted by policy, emit `override_denied` only and ignore it.
   - The resulting map is `effective_environment`. All placeholder expansion MUST use `effective_environment` only (not process env directly).
   - When producing trace output for merged maps, implementations MUST emit per-key trace entries in lexicographic order of the map key.

7. **Placeholder expansion (single-pass, deterministic)**
   - Placeholders are exact tokens of the form `{NAME}` with no nesting and no recursion.
   - Expansion MUST be single-pass using a snapshot of `effective_environment` taken before expansion.
   - Placeholders produced by expansion MUST NOT be expanded again.
   - Missing placeholders MUST emit `missing_env_var`, include the missing placeholder name and `source_path`, and substitute an empty string.
   - Each expanded string MUST be limited to 64 KiB and at most 128 placeholders.
     - Exceeding 128 placeholders MUST emit `invalid_configuration` with reason `placeholder_limit`, include `source_path`, and substitute an empty string.
     - Exceeding 64 KiB after expansion MUST emit `invalid_configuration` with reason `expansion_overflow`, include `source_path`, and substitute an empty string.
   - Expansion MUST be applied to the following fields:
     - `effective_environment` values (per key)
     - `nak_record.loader.args_template[]` ONLY if NAK is resolved AND `[loader]` is present
     - `nak_record.execution.cwd` ONLY if NAK is resolved AND `[execution]` is present AND `cwd` is present
     - `host_env.paths.library_prepend[]`, `host_env.paths.library_append[]`
     - `install_record.overrides.arguments.prepend[]`, `install_record.overrides.arguments.append[]`
     - `install_record.overrides.paths.library_prepend[]`
     - Manifest `ENTRYPOINT_ARG[]`
   - When expanding `effective_environment` values, implementations MUST process keys in lexicographic order of the environment key to ensure deterministic warning ordering and trace output.
   - Expanded values MUST replace the corresponding entries in `effective_environment`. After step 7, `effective_environment` refers to the expanded environment map used for all subsequent steps and Launch Contract output.

8. **Derive capabilities (Normative)**
   - Collect manifest permissions (`PERMISSION_FILESYSTEM` and `PERMISSION_NETWORK`) in TLV order (the decode order after applying tag ordering rules). Selectors are opaque and MUST NOT be expanded or normalized.
   - For each permission: if it lacks ":", emit `capability_malformed` and use `key = permission`, `selector = ""`. Otherwise split on the first ":" into `operation` and `selector`.
   - Map operations via `derive_capability`: `read`→`filesystem.read`, `write`→`filesystem.write`, `execute`→`filesystem.execute`, `connect`→`network.connect`, `listen`→`network.listen`, `bind`→`network.bind`. Unknown operations emit `capability_unknown` (include the unknown operation) and use `key = <operation>`, `selector = <rest>`.
   - Set `capability_usage.present = true` if any permissions exist. `capability_usage.required_capabilities` MUST be the derived `<key>:<selector>` strings in manifest order. `capability_usage.optional_capabilities` and `capability_usage.critical_capabilities` MUST be empty in v1.0.
   - Capability-to-enforcement mapping is not performed by NAH; `enforcement.filesystem` and `enforcement.network` MUST be empty arrays.
   - Selectors remain opaque; no path containment or placeholder expansion applies.

9. **Determine execution binary and arguments**

- If NAK resolved and `[loader]` is present, `execution.binary` = `nak_record.loader.exec_path`.
- Otherwise, `execution.binary` = `app.entrypoint`.
- If `[loader]` is absent, `execution.arguments` MUST NOT include `nak_record.loader.args_template`.
- `execution.arguments` begins with expanded `nak_record.loader.args_template` if `[loader]` is present; otherwise empty.
- If `install_record.overrides.arguments.prepend` or `install_record.overrides.arguments.append` are absent, treat them as empty lists.
- Prepend expanded `install_record.overrides.arguments.prepend`.
- Append expanded manifest entrypoint args.
- Append expanded `install_record.overrides.arguments.append`.

10. **Determine execution cwd**
    - If NAK resolved and `[execution]` is present in `nak_record` and `nak_record.execution.cwd` is present:
      - Use the expanded value from step 7.
      - If the expanded value is empty, use `app.root`.
      - If absolute, use as-is.
      - If relative, resolve against `nak_record.paths.root` and enforce containment.
    - Otherwise, `execution.cwd` = `app.root`.

11. **Set library path environment key**
    - `execution.library_path_env_key` MUST be the platform-specific key: `LD_LIBRARY_PATH` (Linux), `DYLD_LIBRARY_PATH` (macOS), `PATH` (Windows).

12. **Build library path list (ordered)**
    - Initialize `execution.library_paths` empty.
    - Append expanded `host_env.paths.library_prepend` entries; if `host_env.paths.library_prepend` is absent, treat it as an empty list. Each MUST be absolute, otherwise emit `invalid_library_path` (include `source_path` + offending value) and skip.
    - If `install_record.overrides.paths.library_prepend` is absent, treat it as an empty list.
    - Append expanded `install_record.overrides.paths.library_prepend` entries; same validation as above.
    - If NAK resolved, validate each `nak_record.paths.lib_dirs` entry is absolute and under `nak_record.paths.root`, then append (CriticalError::PATH_TRAVERSAL on escape). If `nak_record.paths.lib_dirs` is absent, treat it as an empty list.
    - For each manifest `LIB_DIR`: if the manifest-provided path is absolute, emit `invalid_manifest` and ignore that LIB_DIR entry. Otherwise resolve it under `app.root` using the symlink-escape-safe method; if resolution escapes `app.root` or encounters a symlink, emit CriticalError::PATH_TRAVERSAL; otherwise append.
    - Append expanded `host_env.paths.library_append` entries; if `host_env.paths.library_append` is absent, treat it as an empty list. Same validation as above.

13. **Resolve asset exports**
    - For each `manifest.asset_exports` entry in manifest order:
      - If the manifest path is absolute, emit `invalid_manifest` and ignore this ASSET_EXPORT entry (continue to next export).
      - Resolve the path against `app.root` using a symlink-escape-safe method (no-follow resolution). If resolution escapes `app.root` or encounters a symlink, emit CriticalError::PATH_TRAVERSAL.
      - Construct `AssetExport { id, path = resolved absolute path, type = manifest type }` and store it in the Launch Contract exports map keyed by `id`. If the same `id` appears multiple times, the last occurrence wins.

14. **Finalize environment map**
    - Launch Contract environment MUST be set to the expanded `effective_environment` produced after step 7.

15. **Warnings vs CriticalError**
    - Any path traversal or symlink escape is a CriticalError::PATH_TRAVERSAL.
    - Missing entrypoint is CriticalError::ENTRYPOINT_NOT_FOUND.
    - Invalid or missing App Install Record is CriticalError::INSTALL_RECORD_INVALID.
    - All warnings default to action "warn".

**Warning payload details (Normative):**

**Canonical Warning Object (Normative):** Each emitted warning in JSON output MUST be a JSON object with:

- `key` (string): canonical lowercase snake_case warning identifier
- `action` (string): always `"warn"` (all warnings default to warn action)
- `fields` (object): warning-specific fields as defined below

Top-level keys of each warning object MUST be serialized in lexicographic order (`action`, `fields`, `key`). Keys inside `fields` MUST be serialized in lexicographic order.
The `warnings` array MUST preserve emission order. Implementations MUST NOT sort the warnings array.

- `missing_env_var` required data MUST be `fields.missing` and `fields.source_path`.
- `override_denied` required data MUST be `fields.target`, `fields.source_kind`, `fields.source_ref`.
- `override_invalid` required data MUST be `fields.target`, `fields.reason`, `fields.source_kind`, `fields.source_ref`.
- For `override_invalid`, `fields.target` MUST be `OVERRIDES_FILE` for file-level parse/shape failures; otherwise `fields.target` MUST be the specific override key (`NAH_OVERRIDE_ENVIRONMENT`).
- `capability_missing` required data MUST be `fields.capability`.
- `capability_malformed` required data MUST be `fields.permission`.
- `capability_unknown` required data MUST be `fields.operation`.
- `invalid_library_path` required data MUST be `fields.value` and `fields.source_path`.
- `invalid_configuration` required data MUST be `fields.reason` and `fields.source_path`. For app field mismatches, `fields.fields` MUST be a comma-separated list of differing field names.

### Capability Key Derivation (Normative)

NAH MUST normalize capability permissions into capability keys:

**Selector Opacity (Normative):**
Selectors are opaque strings. NAH MUST NOT perform placeholder expansion inside selectors. NAH MUST NOT normalize, resolve, or apply root containment checks to selectors. NAH MUST NOT interpret selector semantics (filesystem, URLs, globbing, etc.). Selectors are passed through unchanged to host enforcement mapping/evaluation.
Selectors MAY resemble file paths or URLs, but NAH MUST treat selectors as opaque strings and MUST NOT parse, normalize, expand, or apply containment checks to them.

```cpp
struct Capability {
    std::string key;       // Capability key (e.g., "filesystem.read")
    std::string selector;  // Resource selector (e.g., "host://user-documents/*")
};

Capability derive_capability(const std::string& permission) {
    // Format: "operation:selector"
    auto colon = permission.find(':');
    if (colon == std::string::npos) {
        emit_warning("capability_malformed", permission);
        return {permission, ""};  // Use whole string as key
    }

    auto operation = permission.substr(0, colon);
    auto selector = permission.substr(colon + 1);

    // Filesystem permissions
    if (operation == "read") return {"filesystem.read", selector};
    if (operation == "write") return {"filesystem.write", selector};
    if (operation == "execute") return {"filesystem.execute", selector};

    // Network permissions
    if (operation == "connect") return {"network.connect", selector};
    if (operation == "listen") return {"network.listen", selector};
    if (operation == "bind") return {"network.bind", selector};

    // Unknown operation
    emit_warning("capability_unknown", operation);
    return {operation, selector};
}
```

**Normative mapping:**

- `filesystem_permission("read:<selector>")` → capability key `filesystem.read`
- `filesystem_permission("write:<selector>")` → capability key `filesystem.write`
- `filesystem_permission("execute:<selector>")` → capability key `filesystem.execute`
- `network_permission("connect:<host>:<port?>")` → capability key `network.connect`
- `network_permission("listen:<port>")` → capability key `network.listen`
- `network_permission("bind:<port>")` → capability key `network.bind`

The **capability key** identifies the capability type.
The **selector/resource** is passed through to host policy for evaluation.

## NAK Selection

### Install-Time NAK Selection (Normative)

```cpp
struct NakPin {
    std::string id;         // NAK id (must match manifest.nak_id)
    std::string version;    // Pinned version (must match nak_record.nak.version)
    std::string record_ref; // "<nak_id>@<version>.json" filename under <nah_root>/registry/naks/
};

NakPin select_nak_for_install(
    const AppManifest& manifest,
    const NakRegistry& naks
);
```

Selection MUST be stable and deterministic for the same inputs.

- Parse `manifest.nak_version_req` as a SemVer requirement (see SemVer Requirement (Normative)).
- If parsing fails, emit `invalid_manifest` and treat NAK as unresolved.
- No lexicographic fallback is permitted.
- Filter registry records where `record.nak.id == manifest.nak_id`.
- If no records remain, emit `nak_not_found` and treat NAK as unresolved.
  - This warning is only emitted at install time and MUST NOT be emitted by contract composition.
- Choose the highest installed `record.nak.version` that satisfies `manifest.nak_version_req`.
- If no candidate exists, emit `nak_version_unsupported` and treat NAK as unresolved.

`nak_not_found` MUST NOT be emitted by `compose_contract` or `load_pinned_nak`.

The selected record MUST be written into the App Install Record `[nak]` pin.

### Compose-Time Pinned NAK Load (Normative)

```cpp
struct PinnedNakLoadResult {
    bool loaded;                  // true iff the pinned NAK record is usable for composition (see rules below)
    NakInstallRecord nak_record;  // meaningful when loaded == true
};

PinnedNakLoadResult load_pinned_nak(
    const NakPin& pin,
    const AppManifest& manifest
);
```

- If `manifest.nak_id` is empty, the app is standalone; return `PinnedNakLoadResult{ .loaded = false }` without emitting any warning.
- Load `<nah_root>/registry/naks/<pin.record_ref>`.
- If `pin.record_ref` is missing or empty, emit `nak_pin_invalid` and return `PinnedNakLoadResult{ .loaded = false }`.
- If the pinned record cannot be loaded as a valid `nah.nak.install.v2` record with required fields, emit `nak_pin_invalid` and return `PinnedNakLoadResult{ .loaded = false }`.
- If the pinned record loads successfully, validate:

  - `pin.id == nak_record.nak.id == manifest.nak_id`
    - If not, emit `nak_version_unsupported` and return `PinnedNakLoadResult{ .loaded = false }`.

  - `pin.version == nak_record.nak.version`
    - If not, emit `nak_pin_invalid` and return `PinnedNakLoadResult{ .loaded = false }`.

  - `nak_record.nak.version` MUST be a core SemVer version (`MAJOR.MINOR.PATCH` with non-negative decimal integers, no pre-release or build metadata).
    - If not, emit `nak_pin_invalid` and return `PinnedNakLoadResult{ .loaded = false }`.

  - `manifest.nak_version_req` MUST parse as a SemVer requirement (see SemVer Requirement (Normative)).
    - If parsing fails, emit `invalid_manifest` and return `PinnedNakLoadResult{ .loaded = false }`.

  - The pinned version MUST satisfy `manifest.nak_version_req` (SemVer requirement evaluation).
    - If not, emit `nak_version_unsupported` and return `PinnedNakLoadResult{ .loaded = false }`.

`load_pinned_nak` MUST attempt to read and parse `<nah_root>/registry/naks/<pin.record_ref>`, validate `"$schema": "nah.nak.install.v2"`, and require `nak.id`, `nak.version`, and `paths.root`. On any failure (file missing, parse error, schema missing/mismatch, missing required fields), it MUST emit `nak_pin_invalid` and return `PinnedNakLoadResult{ .loaded = false }`. On success, it MUST return `PinnedNakLoadResult{ .loaded = true, .nak_record = nak_record }`, and `loaded == true` MUST imply that no `nak_pin_invalid` or `nak_version_unsupported` condition occurred during the validation rules above.

**Warning emission (Normative):**

- `nak_pin_invalid` MUST be emitted only when the pin is missing/empty, the pinned record file cannot be read/parsed, the schema is missing/mismatched, required fields are missing/empty, or the pinned NAK version string is invalid.
- `nak_version_unsupported` MUST be emitted only when the pinned record was read and parsed but is incompatible with the manifest’s `nak_id` or `nak_version_req`; in these cases, `load_pinned_nak` MUST return `loaded == false`.

`compose_contract` MUST only load the pinned NAK and MUST NOT perform dynamic selection at composition time.

### NAK Not Resolved (Normative)

- If `load_pinned_nak.loaded == false`, NAH MUST omit NAK-derived library paths, omit NAK loader wiring, and omit `NAH_NAK_*` environment variables. All warnings default to action "warn".
- If the NAK is unresolved, Launch Contract `nak.id`, `nak.version`, `nak.root`, `nak.resource_root`, and `nak.record_ref` MUST be empty strings.

### Launch Contract Structure

**Normative:** `LaunchContract` is the payload produced by composition on success; CLI JSON output wraps it in an envelope containing `schema`, `warnings`, optional `trace`, and `critical_error`.

```cpp
struct LaunchContract {
    struct {
        std::string id;
        std::string version;
        std::string root;
        std::string entrypoint;
    } app;

    struct {
        std::string id;
        std::string version;
        std::string root;
        std::string resource_root;
        std::string record_ref;
    } nak;

    struct {
        std::string binary;
        std::vector<std::string> arguments;
        std::string cwd;
        std::string library_path_env_key;
        std::vector<std::string> library_paths;
    } execution;

    std::unordered_map<std::string, std::string> environment;

    struct {
        std::vector<std::string> filesystem;
        std::vector<std::string> network;
    } enforcement;

    struct {
        std::string state;        // "verified" | "unverified" | "failed" | "unknown"
        std::string source;     // host-defined identifier
        std::string evaluated_at; // RFC3339/ISO8601 string
        std::string expires_at;   // RFC3339/ISO8601 string, optional
        std::unordered_map<std::string, std::string> details; // host-defined
    } trust;

    std::unordered_map<std::string, AssetExport> exports;

    struct {
        bool present = false;
        std::vector<std::string> required_capabilities;
        std::vector<std::string> optional_capabilities;
        std::vector<std::string> critical_capabilities;
    } capability_usage;

};
```

**Asset exports (Normative):** `AssetExport` has fields `{ id (string), path (absolute path under app.root), type (string, optional) }`. Launch Contract exports is a map keyed by export id containing resolved `AssetExport` entries derived from manifest `ASSET_EXPORT` values. Paths MUST be the symlink-safe, absolute resolutions of the manifest-provided relative paths under `app.root`. If duplicate ids occur, the last manifest entry wins.

Launch Contract exists only when composition completes without a CriticalError. JSON output MUST wrap the Launch Contract fields in an envelope containing `schema = "nah.launch.contract.v1"`, `warnings`, optional `trace`, and `critical_error = null` on success. If a CriticalError occurs, the tool MUST emit `schema = "nah.launch.contract.v1"` and `critical_error = <enum name>`, MUST omit Launch Contract fields, and MUST exit with code 1.

When `[trust]` is absent, Launch Contract trust.state MUST be `"unknown"`; `source`, `evaluated_at`, and `expires_at` MUST be empty strings; `details` MUST be an empty map.

`capability_usage.present` MUST be true whenever the manifest declares any permissions. `capability_usage.required_capabilities` MUST be the normalized `<key>:<selector>` strings from capability derivation. `capability_usage.optional_capabilities` and `capability_usage.critical_capabilities` MUST be empty in v1.0. `enforcement.filesystem` and `enforcement.network` MUST be empty arrays (capability-to-enforcement mapping is not performed by NAH).

**NAK Fields (Normative):**

- `manifest.nak_version_req` is the NAK version requirement string from the App Manifest.
- `manifest.nak_version_req` MUST be parseable as a SemVer requirement (see SemVer Requirement (Normative)).
- If parsing fails, NAH MUST emit `invalid_manifest` and treat NAK as unresolved per warning policy.
- `nak.version` is the resolved NAK version.
- `nak.root` and `nak.resource_root` are resolved from the NAK Install Record.

**Execution Rules (Normative):**

- `execution.binary` MUST be a single concrete executable path.
- `execution.arguments` MUST be the final argv with templates expanded and overrides applied.
- `execution.cwd` MUST be a concrete working directory.
- `execution.library_path_env_key` MUST be the platform-specific key: `LD_LIBRARY_PATH` (Linux), `DYLD_LIBRARY_PATH` (macOS), `PATH` (Windows).
- `execution.library_paths` MUST be absolute paths.
- When forming an environment variable string, paths MUST be joined using the platform separator (`:` on Unix, `;` on Windows).

**Trace Output (Normative):**
When `--trace` is provided to `nah contract show` or `nah contract diff`, JSON output MUST include an optional `trace` block. Trace is output-only and MUST NOT affect composition behavior.

Each trace entry MUST include:

- `value`
- `source_kind` (one of `host_env`, `nak_record`, `manifest`, `install_record`, `process_env`, `overrides_file`, `standard`)
- `source_path`
- `precedence_rank`

`precedence_rank` MUST be the integer rank from Precedence Rules: 1=Host Environment, 2=NAK Install Record defaults, 3=App Manifest defaults, 4=App Install Record overrides, 5=NAH standard variables, 6=process environment overrides, 7=file-based overrides.

For `environment.<KEY>` entries corresponding to NAH standard variables (`NAH_APP_*`, `NAH_NAK_*`), the trace entry MUST use `source_kind = "standard"`, `source_path` pointing to the standard derivation site (e.g., `standard.NAH_APP_ID`), and `precedence_rank = 5`, regardless of whether the value originated from manifest/app fields.

For list fields (`execution.arguments`, `execution.library_paths`), trace MUST be per-element.
For merged maps (`environment`), trace MUST be per-key.
All trace objects that are maps (including trace.environment and any nested per-field maps) MUST serialize keys in lexicographic order.
If warnings are included in trace payloads, warning keys MUST use canonical lowercase snake_case identifiers.
If warnings include `source_path` fields, they MUST align with the same path notation used in trace entries.

### Error Handling

Warning keys (`WarningObject.key` in JSON) are lowercase snake_case and MUST be used consistently in:

- JSON warning objects (`warnings[*].key`)
- Trace output warning keys (when warnings are included in trace)

Implementations MUST serialize warnings using the canonical lowercase snake_case identifiers exactly as written in this specification, regardless of internal enum representation.
When warnings are serialized in JSON, any warning object fields MUST be emitted with deterministic key ordering as required by Deterministic JSON serialization (Normative).

NAH operates permissively - most issues produce warnings rather than failures:

```cpp
enum class Warning {
    invalid_manifest,           // Continue with defaults
    invalid_configuration,      // Invalid configuration or expansion limits exceeded
    host_env_parse_error,       // Host environment parse failure
    nak_pin_invalid,            // Pinned NAK record missing or invalid
    nak_not_found,              // Install-time only; MUST NOT be emitted by compose_contract
    nak_version_unsupported,    // No installed version satisfies requirement
    nak_loader_required,        // NAK has loaders but app didn't specify one
    nak_loader_missing,         // App requested a loader that NAK doesn't have
    binary_not_found,           // Diagnostic only; MUST NOT be emitted by compose_contract
    capability_missing,         // Continue without capability
    capability_malformed,       // Malformed permission string
    capability_unknown,         // Unknown permission operation
    missing_env_var,            // Variable expansion failed
    invalid_trust_state,        // Invalid trust state treated as unknown
    override_denied,            // NAH_OVERRIDE_* blocked by host policy
    override_invalid,           // Invalid override payload
    invalid_library_path,       // Invalid or non-absolute library path

    // Trust state warnings
    trust_state_unknown,        // No trust information available
    trust_state_unverified,     // Trust state = "unverified"
    trust_state_failed,         // Trust state = "failed"
    trust_state_stale,          // Trust evaluation expired (expires_at in past)
};

// Only critical errors stop execution
enum class CriticalError {
    MANIFEST_MISSING,          // Cannot proceed without manifest
    ENTRYPOINT_NOT_FOUND,      // No binary to execute
    PATH_TRAVERSAL,            // Entrypoint or export escapes app root or NAK root
    INSTALL_RECORD_INVALID,    // Selected App Install Record missing/invalid; cannot compose
    NAK_LOADER_INVALID,        // Pinned loader not found in NAK or loader binary missing
};
```

All warnings default to action "warn". compose_contract MUST NOT emit binary_not_found; a missing entrypoint is always CriticalError::ENTRYPOINT_NOT_FOUND.

---

## Security Model Integration

### Security as Host-Owned Trust State

NAH MUST NOT verify signatures or evaluate trust. NAH MUST only surface host-authored trust state from App Install Records. Hosts decide whether/how to verify using external tooling or host-integrated verification components.

Contract composition (`nah contract show` / `compose_contract`) is a **pure function over inputs** (manifest + app install record + NAK record + host environment + env) and **MUST be non-interactive** (no network, no keychain/cert store calls, no plugin loads, no trust source calls). `compose_contract` MUST NOT use dynamic loader APIs (`dlopen`, `LoadLibrary`, or anything that loads executable code not already mapped as part of the current process). `nah contract show` MUST treat manifest + app install record + NAK record + host environment + env as untrusted inputs and MUST NOT execute target binaries or load target DSOs. Verification is **out-of-band**, performed by host tooling/components, and results are **persisted** into the App Install Record.

**Normative Security Boundaries:**

- Contract composition MUST NOT invoke any trust source, signature verification, keychain/cert APIs, or network calls
- `nah contract show` / `compose_contract` reads trust state only from the App Install Record (or "unknown" if missing)
- `nah contract show` MUST NOT call dynamic loader APIs (`dlopen`/`LoadLibrary`) and MUST NOT call OS trust APIs
- NAH MUST treat all `trust.details` fields as opaque and MUST NOT interpret or branch on them
- `nah contract show` MUST NOT branch on `trust.details.*` for any behavior (including warning policy)
- Only `trust.state`, `trust.source`, `trust.evaluated_at`, and `trust.expires_at` may influence warning emission
- `trust.details` is for display/serialization only
- The ONLY blocking mechanism inside NAH is CriticalError (path traversal, missing manifest/entrypoint, invalid App Install Record); warning policy upgrades affect exit status but do not change composition outputs.

**Conformance Note:** A compliant implementation can be audited by checking that `nah contract show` never calls dynamic loader APIs (`dlopen`/`LoadLibrary`) and never calls OS trust APIs.

### Operational Safety

All app payloads, manifests, and install records MUST be treated as untrusted input. Manifest parsing / TLV decode / JSON parse MUST be hardened (bounds checks, size caps). `nah manifest show` / `nah contract show` MUST NOT execute or load code from the target app.

```
NAH Provides (Mechanism):
├── Manifest validation results
├── Trust state storage and retrieval
├── Capability-to-enforcement-ID mapping
└── Warning propagation

Host Decides (Policy):
├── Whether to require verification
├── Which trust sources to use
├── How to apply sandboxing (consuming enforcement IDs)
└── Whether to block on warnings
```

### Trust Source Interface (Optional)

Host tools (nah app install/verify only) MAY use an optional trust source to populate App Install Record trust:

```cpp
struct TrustResult {
    enum class State { Verified, Unverified, Failed, Unknown } state;
    std::string source;                 // e.g. "corp-verifier"
    std::string evaluated_at;             // RFC3339/ISO8601
    std::unordered_map<std::string, std::string> details; // host-defined
    std::vector<std::string> warnings;    // host-defined strings
};

class ITrustSource {
public:
    virtual ~ITrustSource() = default;
    virtual TrustResult evaluate_install(
        const std::string& install_root,
        const std::string& entrypoint,
        const std::string& package_hash
    ) = 0;
};
```

**Normative constraints:**

- ITrustSource MUST NOT be invoked during nah contract show / compose_contract
- `nah contract show` / contract composition MUST NOT dlopen/load plugins (even if configured)
- Only `nah app install` / `nah app verify` MAY load source plugins
- ITrustSource MAY be called by `nah app install` and `nah app verify` to populate/update App Install Record [trust]
- NAH MUST NOT block/allow based on trust results; any gating is via warning->error policy only

---

## Failure Modes

NAH distinguishes between two categories of failures: **Critical Errors** that halt execution immediately, and **Warnings** that are always emitted with "warn" action.

### Critical Errors (Hard Failures)

Critical errors indicate unrecoverable conditions where NAH cannot safely proceed. These always halt execution.

| Error Code | When Emitted | Effect |
|------------|--------------|--------|
| `MANIFEST_MISSING` | Manifest file does not exist, is unreadable, or CRC32 check fails | Contract composition fails; host MUST NOT exec |
| `ENTRYPOINT_NOT_FOUND` | Resolved entrypoint path does not exist or is not executable | Contract composition fails; host MUST NOT exec |
| `PATH_TRAVERSAL` | Any path in manifest, install record, or NAK attempts to escape its root via `..` or symlinks | Contract composition fails; host MUST NOT exec |
| `INSTALL_RECORD_INVALID` | App Install Record is malformed, missing required fields, or references non-existent paths | Contract composition fails; host MUST NOT exec |
| `NAK_LOADER_INVALID` | NAK loader entry is malformed, specifies non-existent binary, or fails validation | Contract composition fails; host MUST NOT exec |

**Recovery:** Critical errors require user intervention. The host should display the error code and relevant details (path, expected location) to help the user diagnose and fix the issue.

### Warnings

Warnings indicate conditions that may or may not be problems depending on context. All warnings are emitted with action "warn" and do not halt execution.

#### Warning Codes

**Manifest Warnings:**

| Warning Code | When Emitted | Recommended Fix |
|--------------|--------------|-----------------|
| `invalid_manifest` | Manifest has structural issues (bad TLV ordering, invalid semver, repeated non-repeatable tags) but is partially readable | Rebuild the application with a valid manifest |

**Host Environment Warnings:**

| Warning Code | When Emitted | Recommended Fix |
|--------------|--------------|-----------------|
| `host_env_parse_error` | Host Environment exists but cannot be parsed as JSON | Fix JSON syntax errors in host.json |
| `invalid_configuration` | Configuration has semantic issues (conflicting settings, invalid combinations) | Review and correct configuration settings |

**NAK Warnings:**

| Warning Code | When Emitted | Recommended Fix |
|--------------|--------------|-----------------|
| `nak_pin_invalid` | App Install Record specifies a NAK pin that doesn't match available NAKs | Update NAK pin or install matching NAK |
| `nak_not_found` | Pinned NAK directory does not exist (install-time only; MUST NOT be emitted by compose_contract) | Install the required NAK |
| `nak_version_unsupported` | NAK exists but its version doesn't satisfy app's semver requirement | Install a compatible NAK version |
| `nak_loader_required` | NAK declares loaders but app didn't specify which one to use | Add loader selection to App Install Record |
| `nak_loader_missing` | App requests a loader that doesn't exist in the NAK | Use an available loader or install different NAK |

**Binary/Path Warnings:**

| Warning Code | When Emitted | Recommended Fix |
|--------------|--------------|-----------------|
| `binary_not_found` | Referenced binary doesn't exist (diagnostic only; MUST NOT be emitted by compose_contract) | Verify application installation |
| `invalid_library_path` | Library path in env or manifest points to non-existent directory | Fix library path configuration |

**Capability Warnings:**

| Warning Code | When Emitted | Recommended Fix |
|--------------|--------------|-----------------|
| `capability_malformed` | Capability specification is syntactically invalid | Fix capability format in manifest or install record |
| `capability_unknown` | Capability name is not recognized | Check for typos; capability may not be supported |

**Environment Warnings:**

| Warning Code | When Emitted | Recommended Fix |
|--------------|--------------|-----------------|
| `missing_env_var` | Environment variable referenced but not defined in any source | Define the variable in Host Environment or NAK |

**Override Warnings:**

| Warning Code | When Emitted | Recommended Fix |
|--------------|--------------|-----------------|
| `override_denied` | An override is not permitted by Host Environment override policy | Update Host Environment override policy or remove the override |
| `override_invalid` | Override specification is malformed | Fix override format |

**Trust State Warnings:**

| Warning Code | When Emitted | Recommended Fix |
|--------------|--------------|-----------------|
| `invalid_trust_state` | Trust state value is not a recognized enum | Fix trust state in App Install Record |
| `trust_state_unknown` | Trust evaluation hasn't been performed | Run `nah app verify` to evaluate trust |
| `trust_state_unverified` | Trust source returned unverified status | Application may need signing or verification |
| `trust_state_failed` | Trust evaluation explicitly failed | Investigate trust failure; app may be tampered |
| `trust_state_stale` | Trust evaluation expired | Re-run `nah app verify` to refresh trust state |

All warnings default to action "warn".

### Machine-Readable Error Output

When `--format json` is specified, NAH outputs structured error information:

```json
{
  "critical_error": {
    "code": "ENTRYPOINT_NOT_FOUND",
    "path": "/app/bin/myapp",
    "message": "Entrypoint does not exist or is not executable"
  }
}
```

Warning output format:

```json
{
  "warnings": [
    {
      "key": "capability_missing",
      "action": "warn",
      "capability": "network",
      "source": "manifest"
    }
  ]
}
```

---

## Binary Manifest Format

### TLV Format

All NAH manifests use the unified TLV (Tag-Length-Value) binary format.

### TLV Encoding Rules (Normative)

Each TLV entry MUST be encoded as:

- tag: uint16 little-endian
- length: uint16 little-endian (number of value bytes)
- value: length bytes

Entries MUST appear in ascending tag order.

If an entry’s tag is less than the previous successfully accepted tag, NAH MUST emit `invalid_manifest` and MUST ignore that entry; decoding MUST continue with subsequent entries.

total_size MUST equal the exact number of bytes in the manifest blob (header + TLV payload). If total_size does not match the available bytes, NAH MUST emit invalid_manifest and treat all manifest fields as absent for this decode attempt (i.e., no TLV entries are trusted).

**Decode Limits (Normative):**

- `total_size` MUST be <= 64 KiB. If larger, NAH MUST emit `invalid_manifest` and treat all manifest fields as absent for this decode attempt.
- The TLV payload MUST contain at most 512 entries. If exceeded, NAH MUST emit `invalid_manifest` and ignore all entries after the 512th.
- Any single TLV string value MUST be <= 4096 bytes. If exceeded, NAH MUST emit `invalid_manifest` and ignore that entry.
- Each repeated tag (ENTRYPOINT_ARG, ENV_VAR, LIB_DIR, ASSET_DIR, ASSET_EXPORT, PERMISSION_FILESYSTEM, PERMISSION_NETWORK) MUST have at most 128 occurrences; additional occurrences MUST emit `invalid_manifest` and MUST be ignored.

The END tag (tag 0) is OPTIONAL. If present, it MUST be the final TLV entry and MUST have length = 0. Any END tag that is not final, or any END tag with non-zero length, MUST emit invalid_manifest and MUST be ignored.

Repeated tags are allowed only for: ENTRYPOINT_ARG, ENV_VAR, LIB_DIR, ASSET_DIR, ASSET_EXPORT, PERMISSION_FILESYSTEM, PERMISSION_NETWORK. Repeated non-repeatable tags MUST emit invalid_manifest and MUST use the first occurrence while ignoring all subsequent occurrences.

Strings MUST be UTF-8 without NUL terminator. Integer fields MUST be little-endian.

SCHEMA_VERSION value MUST be a uint16 little-endian integer with value 1. If present and not equal to 1, NAH MUST emit invalid_manifest and MUST ignore the SCHEMA_VERSION field (treat it as absent).

The crc32 field MUST be IEEE CRC-32 (poly 0x04C11DB7, reflected form 0xEDB88320) computed over the TLV payload bytes only (excluding the header), and verified before decode. CRC failure MUST be treated as manifest missing and MUST produce CriticalError::MANIFEST_MISSING; it is not permissive because the manifest cannot be trusted. Structural or semantic manifest issues (e.g., invalid tag ordering) MUST emit invalid_manifest and proceed permissively per warning policy. An invalid SemVer requirement MUST emit invalid_manifest and treat NAK as unresolved per warning policy.

**Manifest invalidity handling (Normative):**

- CRC32 failure is the ONLY manifest invalidity that is treated as CriticalError::MANIFEST_MISSING.
- All other manifest invalidities MUST emit invalid_manifest and MUST be handled as field-scoped invalidity: NAH MUST ignore only the invalid TLV entry or invalid field value and treat that specific field as absent, while continuing to decode other valid entries.
- If a required field for composition becomes absent due to invalidity (ID, VERSION, NAK_ID, NAK_VERSION_REQ, ENTRYPOINT_PATH), composition MUST still proceed until it reaches the first applicable CriticalError already defined by this specification (e.g., ENTRYPOINT_NOT_FOUND or PATH_TRAVERSAL). Implementations MUST NOT introduce any new CriticalError types.

### Manifest Structure

```cpp
struct ManifestHeader {
    uint32_t magic;      // ASCII bytes "NAHM" in little-endian; numeric value 0x4D48414E
    uint16_t version;    // Format version (1)
    uint16_t reserved;   // Reserved for future use
    uint32_t total_size; // Total manifest size
    uint32_t crc32;      // CRC32 checksum
    // Followed by TLV entries
};

enum class ManifestTag : uint16_t {
    END = 0,
    SCHEMA_VERSION = 1,

    // Identity
    ID = 10,
    VERSION = 11,
    NAK_ID = 12,
    NAK_VERSION_REQ = 13,

    // Execution
    ENTRYPOINT_PATH = 20,
    ENTRYPOINT_ARG = 21,

    // Environment
    ENV_VAR = 30,

    // Layout
    LIB_DIR = 40,
    ASSET_DIR = 41,
    ASSET_EXPORT = 42,

    // Permissions
    PERMISSION_FILESYSTEM = 50,
    PERMISSION_NETWORK = 51,

    // Metadata
    DESCRIPTION = 60,
    AUTHOR = 61,
    LICENSE = 62,
    HOMEPAGE = 63,
};
```

**ASSET_EXPORT encoding (Normative):** Each ASSET_EXPORT value MUST be a UTF-8 string of the form `<id>:<relative_path>[:<type>]`. `<id>` and `<relative_path>` MUST be non-empty. `<relative_path>` MUST be relative to the app root. `<type>` is optional. If the string lacks the first `:` or has an empty `<id>` or `<relative_path>`, NAH MUST emit `invalid_manifest` and ignore that ASSET_EXPORT entry. Repeated ASSET_EXPORT entries are allowed.

**Other TLV string encodings (Normative):**

- `ENTRYPOINT_PATH`, `ENTRYPOINT_ARG`, `LIB_DIR`, `ASSET_DIR`, `PERMISSION_FILESYSTEM`, and `PERMISSION_NETWORK` values MUST be UTF-8 strings without NUL.
- `ENTRYPOINT_PATH`, `LIB_DIR`, and `ASSET_DIR` values MUST be relative paths. Absolute paths MUST emit `invalid_manifest` and be ignored.
- `ENV_VAR` values MUST be UTF-8 strings of the form `KEY=VALUE`, where `KEY` is non-empty and MUST NOT contain `=`. Invalid `ENV_VAR` values MUST emit `invalid_manifest` and be ignored.

### Embedding Mechanism

```cpp
// Application code
#include <nah/manifest.hpp>

NAH_APP_MANIFEST(
    nah::manifest()
        .id("myapp")
        .version("1.0.0")
        .nak_id("com.example.nak")
        .nak_version_req("2.0.0")
        .entrypoint("bin/myapp")
        .lib_dir("lib")
        .asset_dir("share")
        .env("LOG_LEVEL", "info")
        .filesystem_permission("read:host://user-documents/*")
        .network_permission("connect:https://api.example.com:443")
        .description("Example NAH application")
        .author("Developer Name")
        .license("MIT")
        .build()
);

int main() {
    // Application code
    return 0;
}
```

### Platform-Specific Sections

#### macOS (Mach-O)

```cpp
#define NAH_MANIFEST_SECTION \
    __attribute__((used)) \
    __attribute__((section("__NAH,__manifest"))) \
    __attribute__((aligned(16)))
```

#### Linux (ELF)

```cpp
#define NAH_MANIFEST_SECTION \
    __attribute__((used)) \
    __attribute__((section(".nah_manifest"))) \
    __attribute__((aligned(16)))
```

#### Windows (PE/COFF)

```cpp
#pragma section(".nah", read)
#define NAH_MANIFEST_SECTION \
    __declspec(allocate(".nah"))
```

---

## API Surface

### C++ Library API (libnahhost)

```cpp
namespace nah {

struct WarningObject {
    std::string key;                       // lowercase snake_case
    std::string action;                    // always "warn"
    std::unordered_map<std::string, std::string> fields; // warning-specific
};

struct TraceEntry {
    std::string value;
    std::string source_kind;   // host_env | nak_record | manifest | install_record | process_env | overrides_file | standard
    std::string source_path;
    int precedence_rank;       // 1..7
};

// Trace is an output-only structure mirroring the JSON trace schema.
struct Trace {
    // Implementation-defined in-memory representation; JSON output MUST follow the Trace Output (Normative) rules.
};

struct ContractEnvelope {
    LaunchContract contract;                 // Present on all successful calls
    std::vector<WarningObject> warnings;      // Emitted warnings after policy, in emission order
    std::optional<Trace> trace;              // Present only when requested
};

class NahHost {
public:
    // Creation
    static std::unique_ptr<NahHost> create(const std::string& root_path);

    // Application discovery
    std::vector<AppInfo> listApplications() const;
    AppInfo findApplication(const std::string& id,
                          const std::string& version = "") const;

    // Host environment
    HostEnvironment getHostEnvironment() const;

    // Launch contract generation
    Result<ContractEnvelope> getLaunchContract(
        const std::string& app_id,
        const std::string& version = "") const;

    // Warnings and trace are returned via ContractEnvelope; only CriticalError conditions fail the call.
};
```

**Normative:** On CriticalError conditions, the API MUST return `Result::err(...)` with `ErrorCode` matching the corresponding CriticalError (`MANIFEST_MISSING`, `INSTALL_RECORD_INVALID`, `PATH_TRAVERSAL`, `ENTRYPOINT_NOT_FOUND`) and MUST NOT return a `ContractEnvelope` payload.

```cpp
// Result type for error handling
template<typename T, typename E = Error>
class Result {
public:
    static Result ok(T value);
    static Result err(E error);

    bool isOk() const;
    bool isErr() const;

    T& value();
    E& error();

    T valueOr(T default_value) const;

    template<typename F>
    auto map(F func) -> Result<decltype(func(std::declval<T>())), E>;

    template<typename F>
    auto flatMap(F func) -> decltype(func(std::declval<T>()));
};

// Error types
enum class ErrorCode {
    // System / IO
    FILE_NOT_FOUND,
    PERMISSION_DENIED,
    IO_ERROR,

    // Contract composition critical errors (normative)
    MANIFEST_MISSING,
    INSTALL_RECORD_INVALID,
    PATH_TRAVERSAL,
    ENTRYPOINT_NOT_FOUND,

    // Host environment load failures
    HOST_ENV_PARSE_ERROR,
};

class Error {
public:
    Error(ErrorCode code, const std::string& message);
    Error& withContext(const std::string& context);

    ErrorCode code() const;
    std::string message() const;
    std::string toString() const;
};

} // namespace nah
```

---

## CLI Reference

### Canonical Grammar (Normative)

All canonical commands MUST follow: `nah <resource> <action> [target]`.

Canonical top-level resources MUST be limited to:

- `app`
- `nak`
- `host`
- `contract`
- `manifest`
- `doctor`
- `validate`
- `format`

No other top-level command families are defined.

### Global Flags (Normative)

The ONLY global flags (valid on every command) MUST be:

- `--root <path>` (default `~/.nah`)
- `--json` (machine output; default is human-readable text)
- `--trace` (include provenance/trace payload where applicable)
- `-v/--verbose`, `-q/--quiet`

### Canonical Commands (Normative)

#### App lifecycle

```
nah app list
nah app show <id[@version]>
nah app install <package.nap>
nah app uninstall <id[@version]>
nah app verify <id[@version]>
```

**Target Resolution (Normative):** When a command accepts `<id[@version]>` and `@version` is omitted:

- If exactly one installed version exists for that `id`, use it.
- If more than one installed version exists for that `id`, the command MUST fail and list the installed versions, instructing the caller to specify `@version`.
- If no installed versions exist for that `id`, the command MUST fail with a not-installed / FILE_NOT_FOUND style error appropriate to the command.

Semantics:

- `nah app install` MUST write App Install Record atomically.
- `nah app install` MUST resolve and pin a specific NAK version into the App Install Record.
- `nah app install` MAY populate initial trust state via host tooling/trust source (install-time only).
- `nah app verify` MAY update trust state (host-owned) and timestamps.
- `nah app verify` MUST NOT affect contract composition rules.
- `nah app install` MUST extract into a temporary staging directory under `<nah_root>`, validate extraction safety invariants, then atomically rename the staging directory into the final `<nah_root>/apps/<id>-<version>/` location and fsync the parent directory.
- Only after the final directory rename succeeds, `nah app install` MUST write the App Install Record atomically.
- On any failure before the final rename, `nah app install` MUST delete the staging directory and MUST NOT write any App Install Record.

#### Scaffold + packaging (build/deploy tooling)

```
nah app init <dir>
nah app pack <dir> -o <package.nap>
nah nak init <dir>
nah nak pack <dir> -o <pack.nak>
```

Semantics:

- `nah app init` MUST generate a minimal app skeleton: manifest embedding snippet + canonical package layout + minimal README using only canonical commands.
- `nah app pack` MUST produce a deterministic gzip tar archive.
- `nah app pack` MUST enforce extraction safety invariants (path traversal protections) during pack-time checks.
- `nah nak init` MUST generate a minimal NAK pack skeleton including `META/nak.json`.
- `nah nak pack` MUST produce a deterministic gzip tar archive.

#### Deterministic Packaging (Normative)

These rules apply to both `.nap` and `.nak` archives:

- **Entry ordering:** Tar entries MUST be written in lexicographic order by full path, with directories before files within the same prefix.
- **Metadata normalization:** `uid=0`, `gid=0`, `uname=""`, `gname=""`, `mtime=0`.
- **Permissions:** Directories MUST be `0755`. Files MUST be `0644` unless executable.
  - If a file has any executable bit set in source OR resides under `bin/`, it MUST be written as `0755`.
- **Gzip header:** `mtime=0`, original filename omitted, OS field set to `255` (unknown).
- **Symlinks:** Symlinks and hardlinks are NOT permitted. If any symlink or hardlink is encountered, packing MUST fail with an error.

**Install-time extraction safety (Normative):** `nah app install` and `nah nak install` MUST enforce these constraints while extracting archives:

- Reject any entry with an absolute path.
- Reject any entry whose normalized path contains `..` or would escape the extraction root.
- Reject symlinks, hardlinks, device files, FIFOs, and sockets.
- Materialize only regular files and directories.
- On any rejection, the command MUST fail and MUST NOT leave partial extracted outputs in the final install locations.

- **Determinism:** Given identical input bytes and relative paths, the produced archive bytes MUST be identical.

#### NAK lifecycle

```
nah nak list
nah nak show <nak_id[@version]>
nah nak install <pack.nak>
nah nak path <nak_id@version>
```

Semantics:

- nah nak install MUST materialize the NAK pack under <root>/naks/<nak_id>/<version>/.
- nah nak install MUST write the NAK Install Record atomically under <root>/registry/naks/<nak_id>@<version>.json.
- `nah nak install` MUST extract into a temporary staging directory under `<nah_root>`, validate extraction safety invariants, then atomically rename into `<nah_root>/naks/<nak_id>/<version>/` and fsync the parent directory.
- Only after the final directory rename succeeds, `nah nak install` MUST write the NAK Install Record atomically.
- On any failure before the final rename, `nah nak install` MUST delete the staging directory and MUST NOT write any NAK Install Record or final NAK directory.

#### Host

```
nah host init <dir>
nah host install <host_dir> [--clean]
```

Semantics:

- `nah host init` MUST create a minimal NAH root directory structure.
- `nah host init` MUST create: `<dir>/host/host.json`, `<dir>/apps/`, `<dir>/naks/`, `<dir>/registry/apps/`, `<dir>/registry/naks/`.
- `nah host init` MUST generate a README.md in `<dir>` documenting next steps.
- `nah host init` MUST fail if `<dir>` already exists and contains a `host/` directory.
- The generated `host.json` MUST be a valid Host Environment with default settings.

- `nah host install` MUST read `<host_dir>/host.json` and validate schema `nah.host.manifest.v1`.
- `nah host install` MUST resolve `host.nah_root` relative to `<host_dir>` (unless absolute).
- `nah host install` MUST create NAH root directory structure at resolved path.
- `nah host install` MUST copy `<host_dir>/host_env.json` to `<nah_root>/host/host.json` if present.
- `nah host install` MUST install NAKs listed in `naks[]` in order.
- `nah host install` MUST install apps listed in `apps[]` in order.
- If `--clean` is provided, `nah host install` MUST remove existing NAH root first.
- On any failure, `nah host install` MUST NOT leave partial state.

Host Manifest Format (`host.json` in host_dir):

```json
{
  "$schema": "nah.host.manifest.v1",
  "host": {
    "name": "example-host",
    "version": "1.0.0",
    "description": "Optional description",
    "nah_root": "./nah-root"
  },
  "naks": [
    "../sdk/build/mysdk.nak"
  ],
  "apps": [
    "../app/build/myapp.nap"
  ]
}
```

Required fields: `$schema`, `host.name`, `host.version`, `host.nah_root`.

Package paths in `naks[]` and `apps[]` are resolved relative to `<host_dir>`.

#### Contract (launch UX)

```
nah contract show <id[@version]> [--overrides <file>] [--json] [--trace]
nah contract explain <id[@version]> <path> [--json]
nah contract resolve <id[@version]> [--json]
```

Semantics:

- `nah contract show` MUST generate Launch Contract from: Manifest + App Install Record + pinned NAK Install Record + Host Environment + process env.
- `nah contract show` MUST NOT call trust sources or trust APIs.
- `nah contract show` MUST NOT perform NAK selection during composition; MUST load pinned NAK record only.
- If the selected App Install Record is missing, unreadable, fails JSON parsing, has a missing/mismatched schema, or is missing REQUIRED fields, `nah contract show` MUST fail with CriticalError::INSTALL_RECORD_INVALID and MUST NOT produce a Launch Contract.
- `nah contract show` MUST apply overrides in deterministic order: process environment overrides first, then `--overrides <file>` (file overrides win on conflicts), with lexicographic ordering by override key within each source.
- `nah contract explain` MUST return the effective value and provenance for a single `<path>`.
- `nah contract explain` MUST support `<path>` addressing for `app.*`, `nak.*`, `execution.*`, `environment.<KEY>`, `warnings[*]` as applicable.
- `nah contract resolve` MUST explain install-time NAK selection reasoning for the installed instance without changing selection rules.
- `nah contract resolve` output MUST include registry candidates (NAK id + version + record_ref), semver satisfaction results, and the final selection or unresolved reason.

#### Manifest inspection

```
nah manifest show <binary|id[@version]> [--json]
```

#### Manifest generation

```
nah manifest generate <input.json> -o <manifest.nah> [--json]
nah manifest generate --stdin -o <manifest.nah> [--json]
```

Semantics:

- `nah manifest generate` MUST produce a valid TLV-encoded `manifest.nah` file from a JSON input.
- The input JSON MUST conform to the Manifest Input Format (Normative) defined below.
- The output MUST be a valid TLV binary manifest per the Binary Manifest Format specification.
- If `-o` is omitted, the output MUST be written to `manifest.nah` in the current directory.
- `--stdin` MUST read JSON input from standard input instead of a file.
- `--json` MUST output a JSON object with `ok`, `path`, and `warnings` fields instead of human-readable output.
- On validation failure, `nah manifest generate` MUST exit with code 1 and MUST NOT write any output file.
- On success, `nah manifest generate` MUST exit with code 0.

**Manifest Input Format (Normative):**

The input JSON file MUST have the following structure:

```json
{
  "$schema": "nah.manifest.input.v2",
  "app": {
    "id": "com.example.myapp",
    "version": "1.0.0",
    "nak_id": "com.example.runtime",
    "nak_version_req": ">=2.0.0",
    "entrypoint": "bundle.js",
    "entrypoint_args": ["--mode", "production"],
    "description": "My application",
    "author": "Developer Name",
    "license": "MIT",
    "homepage": "https://example.com",
    "lib_dirs": ["lib", "vendor/lib"],
    "asset_dirs": ["assets", "share"],
    "exports": [
      {
        "id": "config",
        "path": "share/config.json",
        "type": "application/json"
      }
    ],
    "environment": {
      "LOG_LEVEL": "info",
      "NODE_ENV": "production"
    },
    "permissions": {
      "filesystem": ["read:app://assets/*"],
      "network": ["connect:https://api.example.com:443"]
    }
  }
}
```

**Required fields (Normative):** `schema`, `[app].id`, `[app].version`, and `[app].entrypoint` MUST be present. Missing required fields MUST cause validation failure.

**Optional fields (Normative):** `[app].nak_id` and `[app].nak_version_req` are OPTIONAL. If `nak_id` is omitted or empty, the app is a standalone app with no NAK dependency.

**Schema field (Normative):** The `schema` field MUST equal `nah.manifest.input.v2`. Missing or mismatched schema MUST cause validation failure.

**Path validation (Normative):**

- `entrypoint`, `lib_dirs`, `asset_dirs`, and `exports[*].path` MUST be relative paths.
- Absolute paths MUST cause validation failure.
- Paths containing `..` MUST cause validation failure.

**Permission format (Normative):**

- `permissions.filesystem` entries MUST be strings of the form `<operation>:<selector>` where `<operation>` is one of `read`, `write`, `execute`.
- `permissions.network` entries MUST be strings of the form `<operation>:<selector>` where `<operation>` is one of `connect`, `listen`, `bind`.
- Invalid permission formats MUST cause validation failure.

**TLV Generation (Normative):**

- Fields MUST be encoded in ascending tag order per TLV Encoding Rules.
- SCHEMA_VERSION (tag 1) MUST be emitted with value 1.
- Repeated fields (entrypoint_args, lib_dirs, asset_dirs, exports, environment, permissions) MUST emit one TLV entry per item.
- ENV_VAR entries MUST be encoded as `KEY=VALUE` strings.
- ASSET_EXPORT entries MUST be encoded as `<id>:<path>[:<type>]` strings.
- The CRC32 MUST be computed over the TLV payload bytes only (excluding header).

#### Doctor

```
nah doctor <binary|id[@version]> [--json] [--fix]
```

Semantics:

- `nah doctor` MUST be prescriptive: diagnoses + exact next commands or exact file edits required.
- `--fix` is optional and MUST be restricted to safe, rebuildable operations only:
  - apply canonical formatting (equivalent to `nah format`)
  - repair missing `host.json` with empty defaults if unambiguous
- `--fix` MUST NOT install apps/NAKs, change pins, change trust state, change policy, or fetch network resources.

#### Validate (authoring/CI lane)

```
nah validate <kind> <path> [--strict] [--json]
```

`<kind>` MUST be one of: `host-env`, `install-record`, `nak-record`, `package`, `nak-pack`, `capabilities`.

Semantics:

- In `--strict` mode, violations MUST fail fast (non-zero).
- Without `--strict`, the tool MUST report issues deterministically and SHOULD align severity with the spec’s warning model.
- `nah validate capabilities` MUST lint app id format deterministically at authoring/packaging time (warn by default; error in strict).
- `nah validate capabilities` MUST lint capability selectors for hazards (unexpanded vars, machine-specific absolute paths) deterministically and advisory-only.
- Selector linting MUST be advisory-only and MUST NOT infer selector semantics; selectors remain opaque to NAH.
- `nah validate capabilities` MUST NOT interpret selectors as enforcement policy.

#### Format

```
nah format <file> [--check] [--json]
```

Semantics:

- MUST output canonical JSON formatting with stable key ordering and normalized whitespace.
- `--check` MUST exit non-zero if formatting differs.
- MUST NOT reorder arrays where order is semantically meaningful.
- MUST write updates atomically.

#### File-based overrides (contract show)

`nah contract show <id[@version]> --overrides <file>` MUST accept JSON. The parsed document MUST satisfy the JSON shape rules defined in Contract Composition.

Overrides MUST be local-only, MUST NOT trigger network access, MUST NOT persist host state,
and MUST be gated by Host Environment override policy exactly like env overrides.
File overrides MUST be applied after process environment overrides and MUST be processed in lexicographic order by derived override key.
If the overrides file is missing, unreadable, fails to parse, or has an invalid shape, NAH MUST emit `override_invalid` (reason `parse_failure` or `invalid_shape`) and ignore all file overrides. This file-level error MUST be emitted regardless of override policy and MUST use target = OVERRIDES_FILE.
File overrides MUST map to the same supported targets:

- `[environment]` maps to `NAH_OVERRIDE_ENVIRONMENT` semantics (merge map)

Denied vs invalid handling MUST match env overrides:

- Not permitted by policy → emit `override_denied` only
- Permitted but malformed/unknown/invalid → emit `override_invalid`

Any other `NAH_OVERRIDE_<X>` MUST be treated as not permitted, MUST emit `override_denied`, and MUST NOT emit `override_invalid`.
Trace output MUST indicate which overrides applied and which were denied/invalid.

Minimal overrides schema:

```json
{
  "environment": {}
}
```

### CLI Exit Codes (Normative)

Commands MUST exit:

- 0 if completed successfully (with or without warnings; all warnings default to "warn" action)
- 1 if any CriticalError occurs (including `INSTALL_RECORD_INVALID`)

For `--json` outputs, tools MUST still emit the JSON envelope (including `warnings`) whenever composition produces a Launch Contract.

Effective warning evaluation MUST use canonical lowercase snake_case warning keys.

### Example NAK Install Record (with loader)

```json
{
  "$schema": "nah.nak.install.v2",
  "nak": {
    "id": "com.example.nak",
    "version": "3.1.2"
  },
  "paths": {
    "root": "/nah/naks/com.example.nak/3.1.2",
    "resource_root": "/nah/naks/com.example.nak/3.1.2/resources",
    "lib_dirs": [
      "/nah/naks/com.example.nak/3.1.2/lib",
      "/nah/naks/com.example.nak/3.1.2/lib64"
    ]
  },
  "environment": {
    "NAH_NAK_FLAG": "1"
  },
  "loader": {
    "exec_path": "/nah/naks/com.example.nak/3.1.2/bin/nah-runtime",
    "args_template": [
      "--app", "{NAH_APP_ENTRY}",
      "--root", "{NAH_APP_ROOT}",
      "--id", "{NAH_APP_ID}",
      "--version", "{NAH_APP_VERSION}"
    ]
  },
  "execution": {
    "cwd": "{NAH_APP_ROOT}"
  }
}
```

### Example NAK Install Record (libs-only)

```json
{
  "$schema": "nah.nak.install.v2",
  "nak": {
    "id": "com.example.nak",
    "version": "3.1.3"
  },
  "paths": {
    "root": "/nah/naks/com.example.nak/3.1.3",
    "resource_root": "/nah/naks/com.example.nak/3.1.3/resources",
    "lib_dirs": [
      "/nah/naks/com.example.nak/3.1.3/lib"
    ]
  },
  "environment": {
    "NAH_NAK_FLAG": "1"
  },
  "execution": {
    "cwd": "{NAH_APP_ROOT}"
  }
}
```

### Example App Install Record (pinned NAK)

```json
{
  "$schema": "nah.app.install.v2",
  "install": {
    "instance_id": "0f9c9d2a-8c7b-4b2a-9e9e-5c2a3b6b2c2f"
  },
  "app": {
    "id": "com.example.app",
    "version": "1.2.3",
    "nak_id": "com.example.nak",
    "nak_version_req": ">=3.1.0 <4.0.0"
  },
  "nak": {
    "id": "com.example.nak",
    "version": "3.1.2",
    "record_ref": "com.example.nak@3.1.2.json"
  },
  "paths": {
    "install_root": "/nah/apps/com.example.app-1.2.3"
  }
}
```

### Example JSON Output (Machine-Readable)

When `--trace` is requested, JSON output MUST include an optional `trace` block; otherwise it MUST be omitted.

**Deterministic JSON serialization (Normative):**

All JSON output produced by NAH tools MUST be deterministic given identical inputs.

- JSON **map-like objects** (e.g., `environment`, `exports`, `trace.environment`, and any `fields` object inside a warning) MUST serialize keys in lexicographic (byte) order of the key string.
- The top-level envelope object and other non-map schema objects (e.g., `app`, `nak`, `execution`, `trust`, `capability_usage`) MUST be serialized deterministically, but their key ordering is **implementation-defined** and MUST be consistent for identical inputs.
- For nested objects, the same rule applies at every object level.
- JSON arrays MUST preserve the ordering rules defined elsewhere in this specification (e.g., arguments order, library_paths order, manifest/TLV order for derived lists, and deterministic override processing order).
- Implementations MUST NOT rely on hash map iteration order for JSON output.

In JSON output, `warnings` is a top-level field representing warnings emitted during composition and MUST NOT be nested inside the Launch Contract object.

```json
{
  "schema": "nah.launch.contract.v1",
  "app": {
    "id": "com.example.app",
    "version": "1.2.3",
    "root": "/nah/apps/com.example.app-1.2.3",
    "entrypoint": "/nah/apps/com.example.app-1.2.3/bin/app"
  },
  "nak": {
    "id": "com.example.nak",
    "version": "3.1.2",
    "root": "/nah/naks/com.example.nak/3.1.2",
    "resource_root": "/nah/naks/com.example.nak/3.1.2/resources",
    "record_ref": "com.example.nak@3.1.2.json"
  },
  "execution": {
    "binary": "/nah/naks/com.example.nak/3.1.2/bin/nah-runtime",
    "arguments": [
      "--app",
      "/nah/apps/com.example.app-1.2.3/bin/app",
      "--root",
      "/nah/apps/com.example.app-1.2.3",
      "--id",
      "com.example.app",
      "--version",
      "1.2.3"
    ],
    "cwd": "/nah/apps/com.example.app-1.2.3",
    "library_path_env_key": "LD_LIBRARY_PATH",
    "library_paths": [
      "/nah/naks/com.example.nak/3.1.2/lib",
      "/nah/naks/com.example.nak/3.1.2/lib64",
      "/nah/apps/com.example.app-1.2.3/lib"
    ]
  },
  "environment": {
    "NAH_APP_ID": "com.example.app",
    "NAH_APP_VERSION": "1.2.3",
    "NAH_APP_ROOT": "/nah/apps/com.example.app-1.2.3",
    "NAH_APP_ENTRY": "/nah/apps/com.example.app-1.2.3/bin/app",
    "NAH_NAK_ID": "com.example.nak",
    "NAH_NAK_ROOT": "/nah/naks/com.example.nak/3.1.2",
    "NAH_NAK_VERSION": "3.1.2"
  },
  "enforcement": {
    "filesystem": [],
    "network": []
  },
  "trust": {
    "state": "unknown",
    "source": "",
    "evaluated_at": "",
    "expires_at": "",
    "details": {}
  },
  "exports": {},
  "capability_usage": {
    "present": false,
    "required_capabilities": [],
    "optional_capabilities": [],
    "critical_capabilities": []
  },
  "warnings": [
    {
      "action": "warn",
      "fields": {
        "missing": "NAH_SOME_VAR",
        "source_path": "nak_record.loader.args_template[0]"
      },
      "key": "missing_env_var"
    }
  ],
  "critical_error": null,
  "trace": {
    "app": {
      "root": {
        "value": "/nah/apps/com.example.app-1.2.3",
        "source_kind": "install_record",
        "source_path": "install_record.paths.install_root",
        "precedence_rank": 4
      }
    },
    "execution": {
      "arguments": [
        {
          "value": "--app",
          "source_kind": "nak_record",
          "source_path": "nak_record.loader.args_template[0]",
          "precedence_rank": 2
        }
      ]
    },
    "environment": {
      "NAH_APP_ID": {
        "value": "com.example.app",
        "source_kind": "standard",
        "source_path": "standard.NAH_APP_ID",
        "precedence_rank": 5
      }
    }
  }
}
```

### Example Text Output (Human-Readable)

```
Application: com.example.app v1.2.3
NAK: com.example.nak v3.1.2
Binary: /nah/naks/com.example.nak/3.1.2/bin/nah-runtime
CWD: /nah/apps/com.example.app-1.2.3

Library Paths (LD_LIBRARY_PATH):
  /nah/naks/com.example.nak/3.1.2/lib
  /nah/naks/com.example.nak/3.1.2/lib64
  /nah/apps/com.example.app-1.2.3/lib

Environment (selected):
  NAH_APP_ID=com.example.app
  NAH_APP_VERSION=1.2.3
  NAH_APP_ROOT=/nah/apps/com.example.app-1.2.3
  NAH_APP_ENTRY=/nah/apps/com.example.app-1.2.3/bin/app
  NAH_NAK_ID=com.example.nak
  NAH_NAK_ROOT=/nah/naks/com.example.nak/3.1.2
  NAH_NAK_VERSION=3.1.2
```

---

## Developer Flows

### Application Developer Flow

#### 1. Create Application with Manifest

```cpp
// main.cpp
#include <nah/manifest.hpp>

NAH_APP_MANIFEST(
    nah::manifest()
        .id("data-processor")
        .version("2.1.0")
        .nak_id("com.example.nak")
        .nak_version_req("3.0.0")
        .entrypoint("bin/processor")
        .entrypoint_arg("--config={NAH_APP_ROOT}/config.ini")
        .lib_dir("lib")
        .asset_dir("share")
        .env("LOG_LEVEL", "info")
        .env("DATA_DIR", "{NAH_APP_ROOT}/data")
        .filesystem_permission("read:host://user-documents/*")
        .filesystem_permission("write:host://app-storage/*")
        .network_permission("connect:https://api.example.com:443")
        .description("Data processing application")
        .author("Engineering Team")
        .license("Proprietary")
        .build()
);

int main(int argc, char* argv[]) {
    // Application implementation
    return 0;
}
```

#### 2. Build and Package

```bash
# Build with embedded manifest
g++ -std=c++17 -I/path/to/nah/include main.cpp -o processor

# Verify manifest is embedded
nah manifest show processor

# Create package structure
mkdir -p package/bin package/lib package/share
cp processor package/bin/
cp libs/*.so package/lib/
cp -r assets/* package/share/

# Create NAP package (deterministic)
nah app pack package -o data-processor-2.1.0.nap

# Sign package if needed by host policy
gpg --detach-sign data-processor-2.1.0.nap
```

#### 3. Test Installation

```bash
# Install in development environment
nah app install data-processor-2.1.0.nap

# Verify installation
nah app verify data-processor@2.1.0

# Validate capability usage
nah validate capabilities data-processor@2.1.0

# Get launch contract
nah contract show data-processor@2.1.0

# View capability usage
nah contract show data-processor@2.1.0 --json

# Run application (host-dependent)
```

### Bundle Application Developer Flow

Bundle applications (JavaScript, Python, or other interpreted code) use a file-based manifest instead of embedding the manifest in a native binary. The NAK provides the runtime/loader that executes the bundle.

#### 1. Create Manifest Input File

```json
{
  "$schema": "nah.manifest.input.v2",
  "app": {
    "id": "com.example.my-rn-app",
    "version": "1.0.0",
    "nak_id": "com.mycompany.rn-runtime",
    "nak_version_req": ">=2.0.0",
    "entrypoint": "bundle.js",
    "description": "My React Native Application",
    "author": "Mobile Team",
    "exports": [
      {
        "id": "splash",
        "path": "assets/splash.png",
        "type": "image/png"
      }
    ],
    "environment": {
      "NODE_ENV": "production"
    }
  }
}
```

#### 2. Generate Manifest and Package

```bash
# Generate TLV manifest from JSON input
nah manifest generate manifest.json -o manifest.nah

# Verify the generated manifest
nah manifest show manifest.nah

# Create package structure
mkdir -p package/assets
cp bundle.js package/
cp manifest.nah package/
cp -r assets/* package/assets/

# Create NAP package
nah app pack package -o my-rn-app-1.0.0.nap
```

#### 3. NAK Runtime Setup (Host/SDK Team)

The NAK provides the runtime that loads and executes bundles:

```json
{
  "$schema": "nah.nak.pack.v2",
  "nak": {
    "id": "com.mycompany.rn-runtime",
    "version": "2.0.0"
  },
  "paths": {
    "lib_dirs": ["lib"],
    "resource_root": "resources"
  },
  "loader": {
    "exec_path": "bin/rn-loader",
    "args_template": [
      "--bundle", "{NAH_APP_ENTRY}",
      "--assets", "{NAH_APP_ROOT}/assets",
      "--nak-resources", "{NAH_NAK_ROOT}/resources"
    ]
  },
  "execution": {
    "cwd": "{NAH_APP_ROOT}"
  },
  "environment": {
    "RN_RUNTIME_VERSION": "2.0.0"
  }
}
```

#### 4. Installation and Launch

```bash
# Install the NAK (done once by host/platform)
nah nak install rn-runtime-2.0.0.nak

# Install the bundle app
nah app install my-rn-app-1.0.0.nap

# View the launch contract
nah contract show my-rn-app@1.0.0 --json

# The launch contract will show:
# - execution.binary = /nah/naks/com.mycompany.rn-runtime/2.0.0/bin/rn-loader
# - execution.arguments = ["--bundle", "/nah/apps/.../bundle.js", ...]
# - execution.cwd = /nah/apps/com.example.my-rn-app-1.0.0
```

#### Key Differences from Native Apps

| Aspect | Native App | Bundle App |
|--------|------------|------------|
| Manifest storage | Embedded in binary | `manifest.nah` file |
| Manifest creation | C++ macro at compile time | `nah manifest generate` |
| Entrypoint | Native executable | Script/bundle file |
| Execution | Direct execution | NAK loader invokes runtime |
| Permissions | Declared per-app | Typically inherited from NAK sandbox |

#### Security Model for Bundle Apps

Bundle apps run inside the NAK runtime's sandbox. The trust model is:

1. **NAK is the trust boundary** - The runtime defines what the bundle can access
2. **Bundle permissions are advisory** - The runtime enforces its own sandbox
3. **Package signature covers the bundle** - Tampering is detected at install time
4. **Host environment controls overrides** - Host configuration governs environment injection

This mirrors how React Native apps work on iOS/Android: the JS bundle runs inside a native container that defines the security boundary.

### Host Integrator Flow

#### 1. Create Host Environment Configuration

```json
{
  "environment": {
    "NAH_HOST_VERSION": "1.0",
    "NAH_HOST_MODE": "production",
    "NAH_CLUSTER": "us-west-2"
  },
  "paths": {
    "library_prepend": ["/opt/monitoring/lib"],
    "library_append": ["/usr/local/lib"]
  },
  "overrides": {
    "allow_env_overrides": true
  }
}
```

#### 2. Integrate with Host Application

```cpp
// host_launcher.cpp
#include <nah/nahhost.hpp>

class HostLauncher {
public:
    bool launchApplication(const std::string& app_id) {
        // Create NAH host instance
        auto nah = nah::NahHost::create("/nah");

        // Security policy handled by host outside NAH

        // Generate launch contract
        auto contract_result = nah->getLaunchContract(app_id);
        if (!contract_result.isOk()) {
            log_error("Failed to generate contract: {}",
                     contract_result.error().toString());
            return false;
        }

        auto envelope = contract_result.value();
        auto& contract = envelope.contract;

        // Log warnings (all warnings default to "warn" action)
        for (const auto& w : envelope.warnings) {
            log_warning("Contract warning: {}", w.key);
        }

        // Set up environment
        for (const auto& [key, value] : contract.environment) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        // Set library paths
        std::string ld_path = join(contract.execution.library_paths, platform_path_separator());
        setenv(contract.execution.library_path_env_key.c_str(), ld_path.c_str(), 1);

        // Set working directory
        set_working_directory(contract.execution.cwd);

        // Launch process
        return spawn_process(
            contract.execution.binary,
            contract.execution.arguments
        );
    }
};
```

### Operations Flow

#### Override Configuration

```bash
# Force development mode (merged into final environment)
NAH_OVERRIDE_ENVIRONMENT='{"NAH_HOST_MODE":"development"}' nah contract show myapp
```

#### Update Host Environment Without App Rebuilds

```bash
# Edit host.json directly
vim /nah/host/host.json

# Host environment changes take effect immediately
nah contract show myapp  # Uses updated host environment
```

---

## Implementation Architecture

### Recommended Libraries

NAH implementors should leverage existing, well-tested libraries rather than reimplementing core functionality:

- **LIEF** - ELF/Mach-O/PE section extraction and binary inspection. Used by `nah manifest show` and installation-time validation; the contract library can optionally use a minimal extractor if splitting the codebase.

- **nlohmann/json** - JSON parsing/writing for Host Environment, App Install Record, NAK Install Record, and tooling output. The canonical persisted formats are TLV (manifest) and JSON (records/host environment). Output must be stable and atomic-write safe.

- **CLI11** - Command-line parsing and subcommands for NAH tools.

- **spdlog** + **fmt** - Structured logging with compile-time optimizations and string formatting.

- **std::filesystem** - C++17 filesystem/path operations (or ghc::filesystem for older toolchains).

- **libarchive** + **zlib** - NAP/NAK tar.gz read/write with deterministic metadata normalization and symlink rejection; enforce path traversal protections during extract.

- **OpenSSL** - Used only for hashing (e.g., SHA-256 for package_hash) and CRC helpers if needed. MUST NOT be used to make trust decisions inside NAH contract composition.

- **simdutf** - UTF-8 validation for manifest strings and JSON-derived values before use (especially for env var keys/values).

- **semver** (optional) - Strict SemVer parsing/comparison for binding_mode=mapped and canonical selection. Parse failures MUST emit `invalid_manifest` and treat NAK as unresolved.

Platform trust sources (used by external host tooling / `nah app verify` only, not linked into core contract composition):

- macOS: **Security.framework** - Code Signing Services/codesign verification
- Windows: **WinVerifyTrust** + **Crypt32** - Authenticode verification
- Linux: **gpgme** - GPG signature verification

### Core Components

#### Library Core (`include/nah/`)

**Key Headers:**

- `manifest.hpp` - Compile-time manifest builder
- `manifest_types.hpp` - Core type system with bounds
- `error.h` - Result<T,E> error handling
- `app_registry.hpp` - Application registry
- `trust_state.hpp` - Types for trust fields and serialization
- `telemetry.hpp` - Optional event sinks for host monitoring
- `binary_buffer.hpp` - Binary I/O operations
- `path_utils.hpp` - Path manipulation utilities

#### Host Tools (`tools/`)

**Core Tools:**

- `nah` - Canonical CLI (app/nak/host/contract/manifest/doctor/validate/format)
- `libnahhost` - Host integration library

#### Platform Layer

**Platform-Specific:**

- ELF parsing (Linux)
- Mach-O parsing (macOS)
- PE/COFF parsing (Windows - planned)
- Section reading (all platforms)

### Repository Layout and Test Suite (Normative)

The NAH implementation MUST follow this standardized repository structure:

```
nah/
├── CMakeLists.txt
├── cmake/                              # build configuration helpers
├── include/nah/                        # public headers (manifest, contract, host_env, registry, io)
├── src/
│   ├── manifest/                       # TLV encode/decode + extraction
│   ├── contract/                       # compose_contract + warnings + capabilities
│   ├── config/                         # JSON load/write + schema validate
│   ├── registry/                       # install discovery + index
│   ├── platform/                       # ELF/Mach-O/PE section readers + path safety
│   ├── io/                             # atomic write + fsync helpers
│   └── nahhost/                         # NahHost implementation
├── tools/nah/                          # CLI commands + json/text output + exit codes
├── tools/trust/                        # trust sources for install/verify only
├── third_party/                        # vendored dependencies
├── tests/
│   ├── unit/                           # pure unit tests
│   ├── integration/                    # CLI/FS integration tests
│   ├── golden/                         # expected outputs
│   └── fuzz/                           # fuzz targets
├── scripts/                            # format/lint/test helpers
├── docs/                               # spec + implementation notes
└── LICENSE
```

#### Build Targets (Normative)

The implementation MUST produce the following build targets:

- **nah_contract** (library): compose_contract + models + path safety + expansion + warning policy. MUST NOT link trust sources, plugins, or heavy dependencies.
- **nah_manifest** (library): TLV encode/decode + manifest builder macros. Header-only or minimal static lib.
- **nah_platform** (library): ELF/Mach-O/PE minimal section readers. No LIEF dependency for contract composition.
- **nah_config** (library): JSON load/write + schema validate. Used by tools and host integration, NOT by pure contract composition.
- **nah_registry** (library): Registry scanning + instance selection.
- **nahhost** (library): Host-facing API (NahHost::create, getLaunchContract, etc.). Links nah_contract + nah_config + nah_registry.
- **nah** (executable): CLI tool. MAY link all libraries including trust source plugins.

#### Test Coverage (Normative)

The test suite MUST include:

**Unit Tests** (pure, fast):

- TLV decode/encode with valid/invalid inputs
- CRC32 computation and verification
- Variable expansion ({NAH\_\*} substitution, bounds, and missing vars)
- Path normalization and root containment
- Capability derivation from permissions
- NAK resolution (registry-first, canonical/mapped modes)
- Warning policy application
- JSON parsing for host environment and install records (app + NAK)

**Integration Tests** (filesystem operations):

- Install record atomic writes with crash safety
- Registry scanning with multiple instances
- CLI contract command JSON/text output
- CLI app install/verify trust behavior
- Host environment loading and override behavior

**Golden Tests** (regression detection):

- Fixed manifest binaries → expected decoded output
- Fixed host environment/record inputs → expected launch contracts
- CLI commands with known inputs → expected stdout/stderr

**Fuzz Tests** (security hardening):

- TLV decoder with malformed inputs
- JSON parser with malformed host environment
- Variable expansion with malformed placeholders and overflow limits

### Memory Safety Features

- **Fixed-size types**: All manifest strings/arrays have compile-time bounds
- **RAII wrappers**: Automatic resource management
- **Bounds checking**: Safe array access with debug assertions
- **Static payload**: Embedded manifest uses fixed-size struct
- **Minimal allocation**: Registry uses string interning

### Performance Characteristics

#### Compile-time Performance

- Build-time validation via constexpr
- Direct TLV decoding at runtime
- Binary overhead: ~40KB

#### Runtime Performance

- O(1) registry lookups via hashtable
- Single binary scan for extraction
- Zero-copy buffer operations
- Mmap-friendly formats

#### Memory Usage

- Typical embedded manifest size: 200–2000 bytes, depending on string content and number of entries
- Extraction cost: single section read + linear TLV decode
- BinaryReader buffer: 8KB default
- Registry: Varies by app count

---

## Platform Support

### Current Support Matrix

| Platform | Architecture | Binary Format | Status       |
| -------- | ------------ | ------------- | ------------ |
| Linux    | x86_64       | ELF64         | Full Support |
| Linux    | ARM64        | ELF64         | Full Support |
| macOS    | x86_64       | Mach-O        | Full Support |
| macOS    | ARM64        | Mach-O        | Full Support |
| Windows  | x86_64       | PE/COFF       | Planned      |

### Platform-Specific Details

Signature verification hooks listed below apply only to install/verify tooling; contract composition MUST NOT invoke platform trust APIs.

#### Linux

- Uses ELF binary format
- Manifest stored in the `.nah_manifest` section (`NAH_MANIFEST_SECTION`).
- GPG signature verification via libgpgme

#### macOS

- Uses Mach-O binary format
- Manifest stored in the `__NAH,__manifest` section (`NAH_MANIFEST_SECTION`).
- Codesign integration for signatures
- Universal binary support

#### Windows (Planned)

- PE/COFF format support
- Manifest stored in the `.nah` section (`NAH_MANIFEST_SECTION`).
- Authenticode signature verification

---

## NAP Package Format (Normative)

A .nap package MUST be a gzip-compressed tar archive and MUST follow the Deterministic Packaging rules.

The archive root MUST contain:

- `bin/` (optional) - Application binaries
- `lib/` (optional) - Application libraries
- `share/` (optional) - Application assets
- `manifest.nah` (optional; used only if binaries do not embed a manifest)
- `META/install.json` (optional; installer hints; host-owned and ignored by apps)

If both an embedded manifest and `manifest.nah` exist, the embedded manifest MUST take precedence.

A package MUST supply a manifest (embedded in a binary or as `manifest.nah`). If neither is present or the manifest fails CRC/decoding, `nah app install` MUST fail with CriticalError::MANIFEST_MISSING and MUST NOT write any install state.

Example structure:

```
myapp-1.0.0.nap
├── bin/
│   └── myapp          # Binary with embedded manifest
├── lib/
│   ├── libfoo.so
│   └── libbar.so
├── share/
│   └── assets/
└── META/
    └── install.json   # Optional installer hints
```

The `META/install.json` file MAY contain:

```json
{
  "package": {
    "name": "myapp",
    "version": "1.0.0",
    "description": "Example application"
  },
  "install": {
    "preferred_location": "/nah/apps/myapp-1.0.0",
    "signature_file": "myapp-1.0.0.nap.sig"
  }
}
```

---

## NAK Pack Format (Normative)

A NAK pack (`.nak`) MUST be a gzip-compressed tar archive and MUST follow the Deterministic Packaging rules.

**What it is (Normative):** The installable archive that packages a NAK distribution for materialization on a host.

**Contains (Normative):**

- `META/nak.json` (required)
- Optional `lib/`, `resources/`, and `bin/` trees

**MUST NOT contain:**

- App identity or entrypoints
- Host policy or trust decisions
- Absolute paths in `paths.*` or `loader.exec_path`
- Compose-time selection logic

**Schema Field (Informative):** The top-level `$schema` field in `META/nak.json` is OPTIONAL and is intended for editor tooling (autocomplete, validation). It is NOT validated at runtime - the JSON structure itself defines validity.

If `META/nak.json` is missing, unreadable, fails JSON parsing, or is missing REQUIRED fields, `nah nak install` MUST fail with a non-zero exit and MUST NOT write any filesystem outputs (no partial install).

**Required fields (Normative):** `[nak].id` and `[nak].version` MUST be present per Presence semantics.

The archive root MUST contain:

- `META/nak.json` (required) - NAK pack manifest
- `resources/` (optional) - NAK resource root used at launch
- `lib/` (optional) - NAK libraries
- `bin/` (optional) - NAK loader or tools

Example structure:

```
nak-3.0.2.nak
├── META/
│   └── nak.json
├── resources/
│   └── lib/
└── lib/
```

The `META/nak.json` file MUST contain the pack manifest. In this pack format:

- `paths.resource_root`, `paths.lib_dirs`, and `loader.exec_path` MUST be relative to the pack root and MUST NOT be absolute.
- These relative paths MUST be resolved at materialization time against the extracted pack root, and the resulting absolute paths MUST be written into the NAK Install Record.
- `execution.cwd` and `loader.args_template` are compose-time templates and MUST NOT be resolved at materialization time; they are resolved at contract composition time using `effective_environment`.

**Example (with loader):**

```json
{
  "$schema": "nah.nak.pack.v2",
  "nak": {
    "id": "com.example.nak",
    "version": "3.0.2"
  },
  "paths": {
    "resource_root": "resources",
    "lib_dirs": ["lib"]
  },
  "environment": {},
  "loader": {
    "exec_path": "bin/nah-runtime",
    "args_template": [
      "--app", "{NAH_APP_ENTRY}",
      "--root", "{NAH_APP_ROOT}"
    ]
  },
  "execution": {
    "cwd": "{NAH_APP_ROOT}"
  }
}
```

**Example (libs-only NAK, no loader):**

```json
{
  "$schema": "nah.nak.pack.v2",
  "nak": {
    "id": "com.example.nak",
    "version": "3.0.3"
  },
  "paths": {
    "resource_root": "resources",
    "lib_dirs": ["lib"]
  },
  "environment": {}
}
```

`nah nak install` MUST extract the pack and write a NAK Install Record
under `<root>/registry/naks/` with resolved absolute paths.

---

## Build-Time Remote Materialization (Normative)

### Purpose

NAH supports a build-time and deployment-time workflow where NAK packs are fetched from remote artifact stores and materialized into a target filesystem root (e.g., a production image, installer payload, or development sandbox). This workflow exists only to produce the local on-disk state that NAH contract composition consumes.

### Scope and Boundary (Normative)

In scope:

- CI pipelines assembling production images that include one or more NAK versions.
- Developer workflows that hydrate a local `/nah` root for testing.
- Release tooling that writes NAK Install Records deterministically.

Out of scope (MUST NOT be implemented by `nah contract show` / `compose_contract`):

- Any network access, artifact fetching, installation, update, verification, or remote resolution during contract composition.
- Any dynamic install/update of NAKs at launch time.
- Any dependency solving or version negotiation.

### Remote Artifact Reference (Normative)

A build-time materializer MUST accept a NAK pack reference in one of the following forms:

- `file:<absolute_or_relative_path_to_pack.nak>`
- `https://...` (HTTPS URL)
- `https://...#sha256=<hex>` (HTTPS URL with integrity verification)

For HTTPS references, SHA-256 verification is OPTIONAL but RECOMMENDED for production deployments. When a `#sha256=<hex>` fragment is present, the materializer MUST verify the downloaded bytes match the digest and fail if they do not match.

The fragment MUST use the format `#sha256=<64_lowercase_hex_chars>`.

### Integrity and Provenance (Normative)

For any materialized NAK pack, the materializer MUST:

- If a `sha256` digest was provided, verify the artifact bytes match and fail on mismatch.
- Record provenance into the written NAK Install Record:
  - `provenance.source` = the original reference string
  - `provenance.package_hash` = `sha256:<hex>`
  - `provenance.installed_at` = RFC3339 timestamp
  - `provenance.installed_by` = `"image-builder"` or `"dev-tooling"` (implementation-defined but stable)

Signature verification MAY exist as build tooling, but NAH contract composition MUST NOT perform signature verification.

### Materializing NAK Packs (Normative)

Given a NAK pack artifact reference that contains `META/nak.json` (see “NAK Pack Format (Normative)”), the materializer MUST:

1. Download (if HTTPS) or read from disk (if file). If a SHA-256 digest was provided, verify and fail on mismatch.
2. Extract the pack into:
   - `<TARGET_ROOT>/naks/<nak.id>/<nak.version>/`
3. Write a NAK Install Record at:
   - `<TARGET_ROOT>/registry/naks/<nak.id>@<nak.version>.json`
4. In the written NAK Install Record:
   - `paths.root` MUST be the absolute path:
     - `<TARGET_ROOT>/naks/<nak.id>/<nak.version>`
   - `paths.resource_root` MUST be written as an absolute path under `paths.root` by resolving the pack `paths.resource_root` relative to the extracted pack root (defaults to `paths.root` if omitted or ".").
   - `paths.lib_dirs` entries MUST be written as absolute paths under `paths.root` by resolving pack `paths.lib_dirs` relative to the extracted pack root.
   - `environment` MUST be copied unchanged from the pack (these are NAK default environment values used at composition time per Precedence Rules).
   - If `[loader]` is present in the pack:
     - `loader.exec_path` MUST be written as an absolute path under `paths.root` by resolving pack `loader.exec_path` relative to the extracted pack root.
     - `loader.args_template` MUST be copied unchanged from the pack (templates resolved at composition time).
   - If `[loader]` is absent in the pack, the NAK Install Record MUST omit `[loader]`.
   - `execution.cwd` MUST be copied unchanged from the pack if present (template resolved at composition time).
5. All writes MUST be atomic (temp + fsync(file) + rename + fsync(dir)).

Materialization MUST be deterministic: given the same NAK pack bytes and the same `<TARGET_ROOT>`, the extracted directory contents and the non-timestamp fields of the written NAK Install Record MUST be identical.

### NAK Composition Boundary (Normative)

`nah contract show` / `compose_contract` MUST:

- Read only local state (manifest, App Install Record, NAK Install Record, Host Environment).
- Never fetch, install, update, or verify remote artifacts.
- Treat missing or invalid pinned NAK records as `nak_pin_invalid` and proceed according to warning policy; incompatibility of a pinned record MUST emit `nak_version_unsupported`.

---

## Non-Goals and Invariants

### What NAH Does NOT Do

NAH explicitly does NOT:

1. **Build software** - NAH is not a build system
2. **Provide developer toolchains** - build-time tooling is out of scope for contract composition
3. **Resolve dependencies** - No dependency graph management or solver
4. **Manage processes** - No process supervision or monitoring
5. **Implement sandboxing** - Only computes enforcement IDs
6. **Run as a daemon** - No background services
7. **Infer dependencies** - No binary analysis for libraries
8. **Package software** - Not a package manager
9. **Fetch or install from remotes at runtime** — NAH contract composition MUST NOT access the network or materialize NAKs during launch.
10. **Update applications** - No automatic updates

### Invariants That MUST Be Maintained

1. **Separation of Concerns**
   - Applications MUST NOT know host layout
   - Hosts MUST NOT modify app contracts
   - Host Environment MUST NOT contain app-specific data

2. **Immutability Boundaries**
   - App manifests are immutable after build
   - NAK state is host-controlled
   - Host Environment is mutable with audit trail

3. **Security Boundaries**
   - Verification happens at install time
   - Trust decisions are host-owned
   - Enforcement is host-implemented

4. **Portability Guarantees**
   - Apps built once, run anywhere with NAH
   - No host-specific paths in manifests
   - NAK versions are abstract, not paths

5. **Determinism**
   - Same inputs produce same launch contract
   - No implicit behavior or magic
   - All decisions are traceable

---

## Versioning

### Schema Evolution (Normative)

- Schema identifiers ending in `.v1` are additive only; breaking changes MUST use a new schema identifier.
- Implementations MUST ignore unknown keys/sections in JSON Host Environment and records.
- Warning identifiers are stable; new warnings MAY be added, but existing meanings MUST NOT change.
- CLI/API stability: v1.x MAY add commands/flags/fields but MUST NOT remove or rename existing ones.

## Conformance

### Conformance Schema

`nah.conformance.v1`

### NAH v1.0 Compliant Implementation (Normative)

An implementation is NAH v1.0 compliant if it satisfies ALL of the following:

- MUST implement the four artifacts (App Manifest, App Install Record, NAK Install Record, Host Environment) with the schemas defined in this spec.
- MUST implement deterministic NAK selection at install time and pinned NAK loading at composition time.
- MUST implement `compose_contract` as a pure, non-interactive function over inputs.
- MUST enforce path containment and placeholder expansion rules.
- MUST emit warnings and critical errors as specified.
- MUST ignore unknown JSON keys/sections.
- MUST support conformance testing with at least the following categories:
  - Unit tests for pure functions (TLV decode, SemVer requirement parsing/evaluation, NAK selection, variable expansion, path containment).
  - Integration tests for install record atomic write, registry scan, NAK install -> record write.
  - Golden tests for known inputs -> exact Launch Contract JSON/text output.
  - Fuzz tests for TLV decode, JSON parse, variable expansion.

Conformance criteria in v1.x MUST be additive only.

## Conclusion

NAH provides a production-grade, minimal-dependency system for native application hosting that enforces clean separation between application contracts and host configuration. Through its four-artifact model (App Manifest, App Install Record, NAK Install Record, and Host Environment), NAH ensures that applications remain portable while hosts maintain complete control over security, layout, and configuration.
