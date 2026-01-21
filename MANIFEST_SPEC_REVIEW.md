# NAH Manifest Formats - Complete Specification Review

This document provides the complete, literal specifications for all NAH manifest types.

***

## 1. APP MANIFEST (Binary TLV Format)

### Purpose

Application-authored, immutable contract declaring identity, layout, and NAK requirements.

### Storage

* **Embedded**: In binary via `NAH_APP_MANIFEST()` macro
* **Standalone**: As `manifest.nah` file in app payload
* **Owner**: Application developer
* **Mutability**: Immutable after build

### Binary Structure

```cpp
struct ManifestHeader {
    uint32_t magic;      // ASCII "NAHM" (0x4D48414E little-endian)
    uint16_t version;    // Format version (1)
    uint16_t reserved;   // Reserved for future use
    uint32_t total_size; // Total manifest size (max 64 KiB)
    uint32_t crc32;      // IEEE CRC-32 checksum
    // Followed by TLV entries
};
```

### Tag Definitions

```cpp
enum class ManifestTag : uint16_t {
    END = 0,                    // Optional terminator
    SCHEMA_VERSION = 1,         // uint16 value 1
    
    // Identity (REQUIRED)
    ID = 10,                    // UTF-8 string (e.g., "com.example.app")
    VERSION = 11,               // UTF-8 string (e.g., "1.0.0")
    NAK_ID = 12,                // UTF-8 string (e.g., "com.example.sdk")
    NAK_VERSION_REQ = 13,       // UTF-8 string (e.g., "^1.2.0")
    
    // Execution (REQUIRED)
    ENTRYPOINT_PATH = 20,       // UTF-8 relative path
    ENTRYPOINT_ARG = 21,        // UTF-8 string (repeatable)
    
    // Environment
    ENV_VAR = 30,               // UTF-8 "KEY=VALUE" (repeatable)
    
    // Layout
    LIB_DIR = 40,               // UTF-8 relative path (repeatable)
    ASSET_DIR = 41,             // UTF-8 relative path (repeatable)
    ASSET_EXPORT = 42,          // UTF-8 "id:path[:type]" (repeatable)
    
    // Permissions
    PERMISSION_FILESYSTEM = 50, // UTF-8 string (repeatable)
    PERMISSION_NETWORK = 51,    // UTF-8 string (repeatable)
    
    // Metadata (optional)
    DESCRIPTION = 60,           // UTF-8 string
    AUTHOR = 61,                // UTF-8 string
    LICENSE = 62,               // UTF-8 string
    HOMEPAGE = 63,              // UTF-8 string
};
```

### TLV Encoding Rules

* **Format**: `[tag: uint16_le][length: uint16_le][value: length bytes]`
* **Order**: Tags MUST appear in ascending order
* **Strings**: UTF-8 without NUL terminator
* **Limits**:
  * Max total size: 64 KiB
  * Max entries: 512
  * Max string value: 4096 bytes
  * Max repeated tags: 128 occurrences each

### Required Fields

1. `ID` - Application identifier
2. `VERSION` - Application version (SemVer)
3. `NAK_ID` - Required NAK identifier
4. `NAK_VERSION_REQ` - NAK version requirement (SemVer range)
5. `ENTRYPOINT_PATH` - Relative path to executable

### Path Rules

* All paths MUST be relative
* No symlinks allowed in resolution
* MUST resolve under app install root
* Absolute paths are rejected with `invalid_manifest`

### Example JSON Representation (for tooling)

```json
{
  "type": "app",
  "id": "com.example.myapp",
  "version": "1.0.0",
  "nak_id": "com.example.sdk",
  "nak_version_req": "^1.2.0",
  "entrypoint": "bin/app",
  "lib_dirs": ["lib"],
  "asset_dirs": ["assets"],
  "environment": {
    "LOG_LEVEL": "info"
  },
  "permissions": {
    "filesystem": ["read:host://user-documents/*"],
    "network": ["connect:https://api.example.com:443"]
  },
  "metadata": {
    "description": "Example application",
    "author": "Developer Name",
    "license": "MIT",
    "homepage": "https://example.com"
  }
}
```

***

## 2. APP INSTALL RECORD (JSON)

### Purpose

Host-owned record of a specific installed app instance, including pinned NAK and provenance.

### Storage

* **Location**: `<nah_root>/registry/apps/<id>@<version>.json`
* **Owner**: Host
* **Mutability**: Mutable (by host tooling only)
* **Format**: JSON text

### Complete Schema

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
    "record_ref": "com.example.nak@3.0.2.json",
    "selection_reason": "latest_compatible"
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
    "arguments": {
      "prepend": [],
      "append": []
    },
    "paths": {
      "library_prepend": []
    }
  }
}
```

### Required Fields

* `install.instance_id` - Unique install instance UUID
* `app.id` - Application identifier (audit snapshot)
* `app.version` - Application version (audit snapshot)
* `paths.install_root` - Absolute path to app root

### Immutable Fields (after creation)

* `app.id`
* `app.version`
* `app.nak_id`
* `app.nak_version_req`
* `install.instance_id`
* `nak.record_ref` (once set)

***

## 3. NAK INSTALL RECORD (JSON)

### Purpose

Host-owned record of an installed NAK pack defining loader wiring and runtime environment.

### Storage

* **Location**: `<nah_root>/registry/naks/<nak_id>@<version>.json`
* **Owner**: Host
* **Mutability**: Mutable (by host tooling only)
* **Format**: JSON text

### Complete Schema

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
  "environment": {
    "NAH_NAK_VERSION": "3.0.2",
    "NAH_NAK_MODE": "production"
  },
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
  "provenance": {
    "package_hash": "sha256:...",
    "installed_at": "2024-01-15T09:00:00Z",
    "installed_by": "admin",
    "source": "nak-package.nak"
  }
}
```

### Required Fields

* `nak.id` - NAK identifier
* `nak.version` - NAK version
* `paths.root` - Absolute NAK root path

### Optional Sections

* `loader` - OPTIONAL. If absent, app entrypoint is used directly as binary
* `execution` - OPTIONAL. If absent, cwd defaults to app root
* `environment` - OPTIONAL. Defaults to empty map

### Path Rules

* `paths.root` - MUST be absolute
* `paths.resource_root` - MUST be absolute, under root (defaults to root if empty)
* `paths.lib_dirs[]` - MUST be absolute, under root
* `loader.exec_path` - MUST be absolute, under root (if loader present)

### Template Placeholders

Available in `loader.args_template` and `execution.cwd`:

* `{NAH_APP_ROOT}` - App installation root
* `{NAH_APP_ENTRY}` - App entrypoint path
* `{NAH_APP_ID}` - App identifier
* `{NAH_APP_VERSION}` - App version
* `{NAH_NAK_ROOT}` - NAK root path
* `{NAH_NAK_RESOURCE_ROOT}` - NAK resource root
* Any environment variable from effective\_environment

***

## 4. HOST ENVIRONMENT (JSON)

### Purpose

Host-owned configuration providing environment variables, library paths, and override policy.

### Storage

* **Location**: `<nah_root>/host/host.json`
* **Owner**: Framework team / host integrator
* **Mutability**: Mutable, auditable
* **Format**: JSON text

### Complete Schema

```json
{
  "environment": {
    "NAH_HOST_VERSION": "1.0",
    "NAH_HOST_MODE": "production",
    "NAH_CLUSTER": "us-west-2",
    "PATH": {
      "op": "prepend",
      "value": "/opt/host/bin",
      "separator": ":"
    }
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

### Environment Operations

Values can be:

1. **Simple string**: `"MY_VAR": "value"`
2. **Operation object**:
   ```json
   {
     "op": "set|prepend|append|unset",
     "value": "string",
     "separator": ":"
   }
   ```

### Operations

| Operation | Description | Fields |
|-----------|-------------|--------|
| `set` | Replace/set the value (default) | `value` (required) |
| `prepend` | Prepend value with separator | `value` (required), `separator` (default `:`) |
| `append` | Append value with separator | `value` (required), `separator` (default `:`) |
| `unset` | Remove the variable | none |

### Required Fields

None - all fields are optional. Empty object is valid.

### Built-in Empty Default

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

***

## 5. HOST SETUP MANIFEST (Proposed)

### Purpose

Declarative manifest for setting up a complete host environment from scratch.

### Storage

* **Location**: User-provided directory with `nah.json`
* **Owner**: Host administrator
* **Mutability**: User-controlled
* **Format**: JSON text

### Proposed Schema

```json
{
  "type": "host",
  "root": "./nah_root",
  "host": {
    "name": "my-host",
    "version": "1.0.0",
    "description": "Production host for example apps",
    "environment": {
      "NAH_HOST_NAME": "my-host"
    },
    "paths": {
      "library_prepend": [],
      "library_append": []
    },
    "overrides": {
      "allow_env_overrides": true,
      "allowed_env_keys": []
    }
  },
  "install": [
    "path/to/app1.nap",
    "path/to/app2.nap",
    "path/to/sdk.nak"
  ]
}
```

### Usage

```bash
nah install ./host-manifest-dir/
# Auto-detects type:"host" in nah.json
# Creates NAH root at specified location
# Copies/creates host.json
# Installs all packages listed in install[]
```

***

## UNIFIED APPROACH PROPOSAL

All manifests use `nah.json` with a `type` discriminator:

```json
{
  "type": "app" | "nak" | "host",
  // Type-specific fields...
}
```

### Benefits

1. **Single install command**: `nah install <path>` detects type automatically
2. **Consistent tooling**: Validation, generation work the same
3. **Clear semantics**: Type field makes intent explicit
4. **Backward compatible**: Binary format stays for embedded manifests
5. **Spec compliant**: Maintains all SPEC.md requirements

### Detection Logic

```
if path ends with .nap:
    install as app package
elif path ends with .nak:
    install as NAK package
elif path is directory with nah.json:
    read nah.json.type
    if type == "app": pack then install as app
    if type == "nak": pack then install as NAK
    if type == "host": setup host environment
else:
    error: cannot detect type
```

***

## COMPARISON TABLE

| Aspect | App Manifest | App Install Record | NAK Record | Host Environment | Host Setup |
|--------|-------------|-------------------|------------|------------------|------------|
| **Format** | Binary TLV | JSON | JSON | JSON | JSON (proposed) |
| **Owner** | App developer | Host | Host | Host admin | Host admin |
| **Mutable** | No | Yes | Yes | Yes | Yes |
| **Location** | Embedded/standalone | Registry | Registry | Host dir | User dir |
| **Type Field** | N/A | N/A | N/A | N/A | `"host"` |
| **Auto-detect** | File extension | File location | File location | File location | Type + location |
| **Primary Use** | Declare app intent | Track install state | Define NAK wiring | Configure host | Bootstrap host |

***

## RECOMMENDATION

**Add unified `type` field to JSON manifests** to enable:

* Single `nah install <source>` command for everything
* Clear, self-documenting manifest files
* Extensible for future types
* Maintains binary format for embedded manifests

This is the principled architectural approach that makes the CLI consistent and intuitive.
