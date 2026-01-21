# How NAH Works

This document explains the internals of NAH's launch contract system.

## The Core Problem

Native applications need configuration to run:
- Which binary to execute
- What library paths to set
- Which environment variables
- What working directory

Traditionally, this information lives in:
- Documentation that drifts from reality
- Install scripts that diverge between environments
- Tribal knowledge that doesn't scale

NAH solves this by making configuration **declarative** and **composable**.

## Three Inputs, One Output

NAH composes three inputs into a launch contract:

```
┌─────────────┐  ┌─────────────┐  ┌─────────────┐
│   Manifest  │  │     NAK     │  │    Host     │
│   (App)     │  │   (SDK)     │  │ Environment │
└──────┬──────┘  └──────┬──────┘  └──────┬──────┘
       │                │                │
       └────────────────┼────────────────┘
                        ▼
               ┌─────────────────┐
               │ Launch Contract │
               │   (Output)      │
               └─────────────────┘
```

### 1. Manifest (from App)

The manifest declares what the app needs:

```json
{
  "app": {
    "identity": {
      "id": "com.example.myapp",
      "version": "1.0.0",
      "nak_id": "com.vendor.sdk",
      "nak_version_req": ">=2.0.0 <3.0.0"
    },
    "execution": {
      "entrypoint": "bin/myapp"
    }
  }
}
```

### 2. NAK Record (from SDK)

The NAK record declares what the SDK provides:

```json
{
  "nak": { "id": "com.vendor.sdk", "version": "2.1.0" },
  "paths": {
    "root": "/nah/naks/com.vendor.sdk/2.1.0",
    "lib_dirs": ["lib"]
  },
  "environment": {
    "SDK_DATA_PATH": "{NAK_RESOURCE_ROOT}/data"
  }
}
```

### 3. Host Environment (from Host)

The host environment provides host-specific configuration:

```json
{
  "environment": {
    "LOG_LEVEL": "info",
    "DEPLOYMENT_ENV": "production"
  },
  "paths": {
    "library_prepend": [],
    "library_append": []
  }
}
```

### Output: Launch Contract

NAH composes these into a deterministic contract:

```json
{
  "app": { "id": "com.example.myapp", "version": "1.0.0" },
  "nak": { "id": "com.vendor.sdk", "version": "2.1.0" },
  "execution": {
    "binary": "/nah/apps/com.example.myapp-1.0.0/bin/myapp",
    "cwd": "/nah/apps/com.example.myapp-1.0.0",
    "library_paths": ["/nah/naks/com.vendor.sdk/2.1.0/lib"]
  },
  "environment": {
    "NAH_APP_ID": "com.example.myapp",
    "NAH_NAK_ROOT": "/nah/naks/com.vendor.sdk/2.1.0",
    "SDK_DATA_PATH": "/nah/naks/com.vendor.sdk/2.1.0/resources/data",
    "LOG_LEVEL": "info"
  }
}
```

## Install-Time Pinning

A key design decision: NAK version is resolved at **install time**, not launch time.

```
Install Time                    Launch Time
────────────────────────────────────────────────
                                
App v1.0.0 installed            App v1.0.0 runs
    ↓                               ↓
NAK requirement: ^2.0.0         NAK used: 2.1.0
    ↓                           (from install record)
Available: 2.0.0, 2.1.0, 2.2.0
    ↓
Selected: 2.1.0 (highest match)
    ↓
Pinned in install record
```

Benefits:
- **Predictable**: Same NAK every time the app runs
- **Independent updates**: Install new NAK without affecting running apps
- **Rollback friendly**: Pin to older NAK by reinstalling app

## Directory Layout

```
/nah/                           # NAH root
├── host/
│   └── nah.json               # Host environment configuration
├── apps/
│   └── com.example.myapp-1.0.0/
│       ├── nap.json           # App manifest
│       ├── bin/myapp          # Application binary
│       ├── lib/               # Libraries
│       └── assets/            # Assets
├── naks/
│   └── com.vendor.sdk/
│       └── 2.1.0/
│           ├── nak.json       # NAK manifest
│           ├── lib/           # SDK libraries
│           └── resources/     # SDK resources
└── registry/
    ├── apps/
    │   └── com.example.myapp@1.0.0.json
    └── naks/
        └── com.vendor.sdk@2.1.0.json
```

## Contract Composition Steps

1. **Load app install record** - Find the app in the registry
2. **Read manifest** - Get app requirements from manifest
3. **Load NAK record** - Use pinned NAK version from install record
4. **Load host environment** - Apply host configuration
5. **Resolve paths** - Convert relative paths to absolute
6. **Substitute placeholders** - Replace `{NAH_*}` with values
7. **Merge environment** - Layer host env → NAK → manifest → install overrides → standard vars
8. **Emit contract** - Output the final launch parameters

## Placeholder Substitution

NAK records and host environments can use placeholders. Three syntaxes are supported:

- `{NAME}` - NAH-style (canonical)
- `$NAME` - Shell-style
- `${NAME}` - Shell-style with braces

### NAH Standard Variables

| Placeholder | Value |
|-------------|-------|
| `{NAH_APP_ROOT}` | App installation directory |
| `{NAH_APP_ID}` | App identifier |
| `{NAH_APP_VERSION}` | App version |
| `{NAH_NAK_ROOT}` | NAK installation directory |
| `{NAK_RESOURCE_ROOT}` | NAK resources directory |

### System Environment Fallback

If a placeholder is not found in the NAH environment, the system environment is checked. This allows referencing variables like `$HOME` or `$PATH`:

```json
"environment": {
  "CACHE_DIR": "$HOME/.cache/myapp",
  "SEARCH_PATH": "$PATH:{NAH_APP_ROOT}/bin"
}
```

### Example

```json
"environment": {
  "DATA_PATH": "{NAH_APP_ROOT}/data",
  "SDK_CONFIG": "{NAK_RESOURCE_ROOT}/config.json"
}
```

## Environment Precedence

When the same variable is set in multiple places (lowest to highest priority):

```
1. Host environment (fill-only defaults)
2. NAK record (fill-only defaults)
3. Manifest (fill-only defaults)
4. Install record overrides (overwrite)
5. NAH standard variables (overwrite)
6. Process environment overrides (overwrite, if permitted)
```

Higher numbers have higher priority (overwrite lower numbers).

## Trace Mode

Use `nah show` or `nah run` with `--json` to see contract details:

```bash
nah show com.example.app --json
```

The output shows:
- App and NAK identity
- Execution parameters (entrypoint, working directory)
- Environment variables
- Library paths
- Install locations

## Determinism

Given the same inputs (manifest, NAK, profile), NAH produces the same contract.

This enables:
- **Auditing**: Inspect contract before execution
- **Testing**: Verify contract in CI
- **Caching**: Contract can be cached if inputs haven't changed
- **Reproducibility**: Same result across machines

## What NAH Doesn't Do

- **Execute applications** - NAH outputs a contract; you execute it
- **Manage processes** - No supervision, restart, or monitoring
- **Handle networking** - No service discovery or load balancing
- **Package management** - No dependency resolution or downloads

NAH is intentionally minimal. It answers one question: "How do I launch this app?"
