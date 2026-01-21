# Getting Started: Host Integrator

You run apps and SDKs from different vendors. This guide covers setting up a NAH host and deploying packages.

See [Core Concepts](concepts.md) for terminology.

## Quick Start

Initialize a NAH host directory:

```bash
nah init host ./myhost
cd myhost
```

This creates:

```
myhost/
├── nah.json          # Host configuration
├── apps/             # Apps install here
├── naks/             # NAKs install here
└── registry/         # Install records
    ├── apps/
    └── naks/
```

Install packages:

```bash
nah install vendor-sdk-2.1.0.nak
nah install myapp-1.0.0.nap
```

List what's installed:

```bash
nah list
```

Run an app:

```bash
nah run com.example.myapp
```

## Host Configuration

Edit `nah.json` to configure the host environment:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nah.v1.json",
  "host": {
    "root": "/opt/myplatform",
    "environment": {
      "DEPLOYMENT_ENV": "production",
      "NAH_HOST_NAME": "myplatform"
    },
    "paths": {
      "library_prepend": [],
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

### Configuration Fields

| Field | Description |
|-------|-------------|
| `host.root` | Absolute path to NAH root (optional, uses cwd if omitted) |
| `host.environment` | Environment variables to inject into all apps |
| `host.paths.library_prepend` | Library paths added before app/NAK paths |
| `host.paths.library_append` | Library paths added after app/NAK paths |
| `host.overrides.allow_env_overrides` | Allow `NAH_OVERRIDE_ENVIRONMENT` |
| `host.overrides.allowed_env_keys` | If non-empty, only these keys can be overridden |
| `host.install` | Packages to auto-install (optional) |

## Managing Packages

### Install NAKs

```bash
nah install vendor-sdk-2.1.0.nak
nah list --naks
```

Multiple versions can coexist:

```bash
nah install vendor-sdk-1.0.0.nak
nah install vendor-sdk-2.1.0.nak
```

### Install Apps

```bash
nah install myapp-1.0.0.nap
nah list --apps
```

At install time, NAH selects a compatible NAK and pins it.

### Uninstall

```bash
nah uninstall com.example.myapp
nah uninstall com.example.sdk@2.1.0
```

## Viewing Package Details

```bash
nah show com.example.myapp
```

Output shows:

* Identity (ID, version)
* NAK dependency
* Entrypoint
* Layout (lib dirs, asset dirs)
* Install location

For scripting:

```bash
nah show com.example.myapp --json
```

## Running Applications

```bash
nah run com.example.myapp
```

NAH automatically:

1. Loads the app manifest
2. Resolves the NAK dependency
3. Composes the launch contract (paths, environment)
4. Executes the application

## Using NAH\_ROOT

Set `NAH_ROOT` to avoid repeating `--root`:

```bash
export NAH_ROOT=/opt/myplatform
nah install myapp.nap
nah list
nah run com.example.myapp
```

## Declarative Host Setup

For reproducible deployments, use a declarative host manifest:

**nah.json:**

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nah.v1.json",
  "host": {
    "root": "./nah_root",
    "environment": {
      "DEPLOYMENT_ENV": "production"
    },
    "install": [
      "packages/vendor-sdk-2.1.0.nak",
      "packages/myapp-1.0.0.nap",
      "packages/another-app-1.5.0.nap"
    ]
  }
}
```

Then install everything:

```bash
nah install myapp.nap  # Will also install NAK dependencies
```

Package paths in `install` are resolved relative to the `nah.json` file location.

## Next Steps

* [CLI Reference](cli.md) for all commands
* [SPEC.md](../SPEC.md) for the full specification
