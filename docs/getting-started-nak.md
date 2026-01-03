# Getting Started: NAK Developer

Build an SDK or framework that apps depend on.

## Prerequisites

- NAH CLI installed (`nah` command available)

## 1. Create NAK Skeleton

```bash
nah nak init mysdk
cd mysdk
```

This creates:
```
mysdk/
├── META/
│   └── nak.toml    # NAK metadata
├── bin/            # Loader binary (optional)
├── lib/            # Shared libraries
└── resources/      # NAK resources
```

## 2. Edit NAK Metadata

Open `META/nak.toml`:

```toml
schema = "nah.nak.pack.v1"

[nak]
id = "com.yourcompany.mysdk"
version = "1.0.0"

[paths]
resource_root = "resources"
lib_dirs = ["lib"]

[environment]
# Variables set for all apps using this NAK
# MYSDK_VERSION = "1.0"

[execution]
cwd = "{NAH_APP_ROOT}"
```

## 3. Add Your Libraries

Place shared libraries in `lib/`:

```
lib/
├── libmysdk.so        # Linux
├── libmysdk.dylib     # macOS
└── libmysdk.dll       # Windows
```

## 4. Optional: Add a Loader

If your SDK needs to wrap app execution, add a loader:

```toml
[loader]
exec_path = "bin/mysdk-loader"
args_template = ["--app", "{NAH_APP_ENTRY}"]
```

The loader binary will be invoked instead of the app directly.

## 5. Package as NAK

```bash
nah nak pack . -o mysdk-1.0.0.nak
```

## 6. Test Installation

Install into a NAH root:

```bash
nah --root /path/to/nah nak install mysdk-1.0.0.nak
```

## 7. Verify

List installed NAKs:

```bash
nah --root /path/to/nah nak list
```

Show details:

```bash
nah --root /path/to/nah nak show com.yourcompany.mysdk
```

## NAK Metadata Reference

### Required Fields

| Field | Description |
|-------|-------------|
| `nak.id` | Unique identifier (reverse domain) |
| `nak.version` | SemVer version |
| `paths.lib_dirs` | Directories containing shared libraries |

### Optional Fields

| Field | Description |
|-------|-------------|
| `paths.resource_root` | Directory for NAK resources |
| `environment` | Environment variables to set |
| `loader.exec_path` | Path to loader binary |
| `loader.args_template` | Arguments for loader |
| `execution.cwd` | Working directory (default: `{NAH_APP_ROOT}`) |

### Placeholder Variables

Use these in `args_template` and `cwd`:

| Variable | Description |
|----------|-------------|
| `{NAH_APP_ID}` | App's ID |
| `{NAH_APP_VERSION}` | App's version |
| `{NAH_APP_ROOT}` | App's install directory |
| `{NAH_APP_ENTRY}` | App's entrypoint path |
| `{NAH_NAK_ROOT}` | NAK's install directory |

## Next Steps

- See `examples/sdk/` for a complete working example
- Read SPEC.md for the full NAK format
