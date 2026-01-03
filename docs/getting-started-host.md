# Getting Started: Host Integrator

Deploy NAH in a production environment.

## Prerequisites

- NAH CLI installed (`nah` command available)
- NAK packs (`.nak`) for your SDKs
- App packages (`.nap`) to deploy

## 1. Create NAH Root

```bash
nah profile init /nah
```

Or use a custom location:

```bash
nah profile init /opt/mycompany/nah
```

This creates:
```
/nah/
├── host/
│   ├── profiles/
│   │   └── default.toml
│   └── profile.current -> profiles/default.toml
├── apps/
├── naks/
└── registry/
    ├── installs/
    └── naks/
```

## 2. Configure Host Profile

Edit `/nah/host/profiles/default.toml`:

```toml
schema = "nah.host.profile.v1"

[nak]
binding_mode = "canonical"
# allow_versions = ["1.*", "2.*"]
# deny_versions = ["*-beta*"]

[environment]
NAH_HOST_VERSION = "1.0"
NAH_ENVIRONMENT = "production"

[warnings]
# Make these errors in production
nak_not_found = "error"
trust_state_unknown = "error"

[capabilities]
# Map app capabilities to host enforcement
"filesystem.read" = "sandbox.readonly"
"filesystem.write" = "sandbox.readwrite"
"network.connect" = "network.outbound"
```

## 3. Install NAKs

```bash
nah --root /nah nak install mysdk-1.0.0.nak
```

Verify:

```bash
nah --root /nah nak list
```

## 4. Install Apps

```bash
nah --root /nah app install myapp-1.0.0.nap
```

Verify:

```bash
nah --root /nah app list
```

## 5. Validate

Run diagnostics:

```bash
nah --root /nah doctor com.yourcompany.myapp
```

A clean output means the app is ready to launch.

## 6. Generate Launch Contract

```bash
nah --root /nah contract show com.yourcompany.myapp
```

This outputs:
- Binary path to execute
- Command line arguments
- Library paths
- Environment variables
- Working directory

### JSON Output

For programmatic use:

```bash
nah --root /nah contract show com.yourcompany.myapp --json
```

## 7. Launch the App

Use the contract to launch:

```bash
# Get the binary and set up environment
eval $(nah --root /nah contract show myapp --json | jq -r '
  "export " + (.environment | to_entries | map(.key + "=\"" + .value + "\"") | join(" ")) + 
  "; " + .execution.binary + " " + (.execution.arguments | join(" "))
')
```

Or integrate with your process manager (systemd, Docker, etc.).

## Multiple Profiles

Create additional profiles for different environments:

```bash
cp /nah/host/profiles/default.toml /nah/host/profiles/staging.toml
# Edit staging.toml
```

Switch profiles:

```bash
nah --root /nah profile set staging
```

Or use a specific profile:

```bash
nah --root /nah --profile staging contract show myapp
```

## Host Profile Reference

### Binding Modes

| Mode | Description |
|------|-------------|
| `canonical` | Select highest version satisfying requirement |
| `mapped` | Use explicit version mapping |

### Warning Actions

| Action | Description |
|--------|-------------|
| `warn` | Log warning, continue |
| `ignore` | Suppress warning |
| `error` | Fail with error |

### Key Warnings

| Warning | Description |
|---------|-------------|
| `nak_not_found` | No NAK matches requirement |
| `nak_version_unsupported` | NAK version denied by policy |
| `trust_state_unknown` | App trust not evaluated |
| `capability_missing` | Capability not mapped |

## Next Steps

- See `examples/host/` for profile examples
- Read SPEC.md for the full profile format
