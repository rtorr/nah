# Getting Started: NAK Developer

You're building an SDK, runtime, or framework that apps depend on. This guide covers creating and packaging a NAK.

See [Core Concepts](concepts.md) for terminology.

## 1. Create a NAK Skeleton

```bash
nah init nak mysdk
cd mysdk
```

This creates:
```
mysdk/
├── nak.json          # NAK manifest
├── bin/              # Optional loader
├── lib/              # Shared libraries
└── resources/        # SDK resources
```

## 2. Edit NAK Metadata

Open `nak.json`:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nak.v1.json",
  "nak": {
    "identity": {
      "id": "com.yourcompany.mysdk",
      "version": "1.0.0"
    },
    "layout": {
      "resource_root": "resources",
      "lib_dirs": ["lib"]
    },
    "environment": {
      "MYSDK_VERSION": "1.0.0"
    },
    "execution": {
      "cwd": "{NAH_APP_ROOT}"
    }
  },
  "metadata": {
    "description": "My SDK",
    "author": "Your Company",
    "license": "Apache-2.0"
  }
}
```

### Required Fields

| Field | Description |
|-------|-------------|
| `nak.identity.id` | Unique identifier (reverse domain notation) |
| `nak.identity.version` | SemVer version |
| `nak.layout.lib_dirs` | Directories containing shared libraries |

### Optional Fields

| Field | Description |
|-------|-------------|
| `nak.layout.resource_root` | Directory for NAK resources |
| `nak.environment` | Environment variables to set |
| `nak.execution.cwd` | Working directory for apps |
| `metadata` | Description, author, license, homepage |

## 3. Add Libraries

Place shared libraries in `lib/`:

```
lib/
├── libmysdk.so        # Linux
├── libmysdk.dylib     # macOS
└── mysdk.dll          # Windows
```

## 4. Optional: Add a Loader

If your SDK needs to wrap app execution:

```json
{
  "loader": {
    "exec_path": "bin/mysdk-loader",
    "args_template": ["--app", "{NAH_APP_ENTRY}"]
  }
}
```

When present, the loader binary runs instead of the app directly.

### Placeholder Variables

Use in `args_template`, `cwd`, and `environment`. Three syntaxes are supported:
- `{NAME}` - NAH-style (canonical)
- `$NAME` - Shell-style  
- `${NAME}` - Shell-style with braces

| Variable | Value |
|----------|-------|
| `{NAH_APP_ID}` | App's ID |
| `{NAH_APP_VERSION}` | App's version |
| `{NAH_APP_ROOT}` | App's install directory |
| `{NAH_APP_ENTRY}` | App's entrypoint path |
| `{NAH_NAK_ROOT}` | NAK's install directory |

System environment variables like `$HOME` and `$PATH` are also available.

## 5. Package

```bash
nah nak pack . -o mysdk-1.0.0.nak
```

## 6. Test

Install into a NAH root:

```bash
nah --root /path/to/nah nak install mysdk-1.0.0.nak
nah --root /path/to/nah nak list
nah --root /path/to/nah nak show com.yourcompany.mysdk@1.0.0
```

## Next Steps

- [CLI Reference](cli.md) for all commands
- [SPEC.md](../SPEC.md) for the full NAK pack format
