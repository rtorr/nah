# Getting Started: Host Integrator

You run apps and SDKs from different vendors. This guide covers setting up a NAH root and deploying packages.

See [Core Concepts](concepts.md) for terminology.

## 1. Initialize a NAH Root

```bash
nah profile init /opt/myplatform
```

This creates:
```
/opt/myplatform/
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

## 2. Configure the Host Profile

Edit `host/profiles/default.toml`:

```toml
schema = "nah.host.profile.v1"

[nak]
binding_mode = "canonical"
# allow_versions = ["2.*"]
# deny_versions = ["*-beta*"]

[environment]
DEPLOYMENT_ENV = "production"

[warnings]
nak_not_found = "error"
nak_version_unsupported = "error"
```

### Binding Modes

| Mode | Behavior |
|------|----------|
| `canonical` | Select highest version satisfying the app's requirement |
| `mapped` | Use explicit version mappings in `nak.map` |

### Warning Actions

| Action | Behavior |
|--------|----------|
| `warn` | Log and continue |
| `ignore` | Suppress |
| `error` | Fail |

## 3. Install NAKs

```bash
nah --root /opt/myplatform nak install vendor-sdk-2.1.0.nak
nah --root /opt/myplatform nak list
```

Multiple versions can coexist:
```bash
nah --root /opt/myplatform nak install vendor-sdk-1.0.0.nak
nah --root /opt/myplatform nak install vendor-sdk-2.1.0.nak
```

## 4. Install Apps

```bash
nah --root /opt/myplatform app install myapp-1.0.0.nap
nah --root /opt/myplatform app list
```

At install time, NAH selects a compatible NAK and pins it.

## 5. Verify

```bash
nah --root /opt/myplatform doctor com.example.myapp
```

Clean output means the app is ready.

## 6. View the Launch Contract

```bash
nah --root /opt/myplatform contract show com.example.myapp
```

Output shows:
- Binary path
- Library paths
- Environment variables
- Working directory

For scripting:
```bash
nah --root /opt/myplatform --json contract show com.example.myapp
```

## 7. Multiple Profiles

Create profiles for different environments:

```bash
cp host/profiles/default.toml host/profiles/staging.toml
```

Switch the active profile:
```bash
nah --root /opt/myplatform profile set staging
```

Or use a specific profile for one command:
```bash
nah --root /opt/myplatform --profile staging contract show com.example.myapp
```

## Next Steps

- [CLI Reference](cli.md) for all commands
- [SPEC.md](../SPEC.md) for the full profile format
