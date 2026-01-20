# Getting Started: Host Integrator

You run apps and SDKs from different vendors. This guide covers setting up a NAH root and deploying packages.

See [Core Concepts](concepts.md) for terminology.

## Quick Start with Host Manifest

The fastest way to set up a host is with `nah host install`:

```bash
# Create a host directory with manifest
mkdir myhost

# Create nah.json (host manifest)
cat > myhost/nah.json << 'EOF'
{
  "$schema": "nah.host.manifest.v1",
  "root": "./nah_root",
  "host": {
    "environment": {
      "DEPLOYMENT_ENV": "production"
    }
  },
  "install": [
    "../path/to/vendor-sdk-2.1.0.nak",
    "../path/to/myapp-1.0.0.nap"
  ]
}
EOF

# Install everything
nah host install myhost
```

This creates the NAH root, sets up host configuration, and installs all packages in one command.

## Manual Setup

For more control, you can set up a host step by step.

### 1. Initialize a NAH Root

```bash
nah init root /opt/myplatform
```

This creates:
```
/opt/myplatform/
├── host/
│   └── host.json
├── apps/
├── naks/
└── registry/
    ├── apps/
    └── naks/
```

### 2. Configure the Host Environment

Edit `host/host.json`:

```json
{
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
  }
}
```

### Host Environment Fields

| Field | Description |
|-------|-------------|
| `environment` | Environment variables to inject into all apps |
| `paths.library_prepend` | Library paths added before app/NAK paths |
| `paths.library_append` | Library paths added after app/NAK paths |
| `overrides.allow_env_overrides` | Allow `NAH_OVERRIDE_ENVIRONMENT` |
| `overrides.allowed_env_keys` | If non-empty, only these keys can be overridden |

### 3. Install NAKs

```bash
nah --root /opt/myplatform install vendor-sdk-2.1.0.nak
nah --root /opt/myplatform list --naks
```

Multiple versions can coexist:
```bash
nah --root /opt/myplatform install vendor-sdk-1.0.0.nak
nah --root /opt/myplatform install vendor-sdk-2.1.0.nak
```

### 4. Install Apps

```bash
nah --root /opt/myplatform install myapp-1.0.0.nap
nah --root /opt/myplatform list --apps
```

At install time, NAH selects a compatible NAK and pins it.

### 5. Verify

```bash
nah --root /opt/myplatform status com.example.myapp
```

Clean output means the app is ready.

### 6. View the Launch Contract

```bash
nah --root /opt/myplatform status com.example.myapp
```

Output shows:
- Binary path
- Library paths
- Environment variables
- Working directory

For scripting:
```bash
nah --root /opt/myplatform status com.example.myapp --json
```

With provenance trace:
```bash
nah --root /opt/myplatform status com.example.myapp --trace
```

## Next Steps

- [CLI Reference](cli.md) for all commands
- [SPEC.md](../SPEC.md) for the full specification
