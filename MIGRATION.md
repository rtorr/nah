# NAH v2.0 Migration Guide

## Overview

NAH v2.0 is a **major breaking release** that eliminates custom binary formats in favor of pure JSON manifests and standard tar.gz packaging. This is an architectural simplification with **no migration path** from v1.x.

**If you are upgrading from v1.x, you must rebuild all packages.**

## What Changed in v2.0

### 1. Manifest Format: Binary TLV → Pure JSON

**v1.x:**

* Binary TLV manifest format (custom encoding)
* `manifest.nah` files
* Embedded manifests in binaries via `NAH_APP_MANIFEST()` macro
* `nah manifest generate` command to create binary manifests

**v2.0:**

* Pure JSON manifests throughout
* `nap.json` for apps, `nak.json` for NAKs, `nah.json` for host config
* No binary format, no embedded manifests
* Manifests at package root (not in `META/`)

**Migration:**

* Delete all `.nah` binary manifest files
* Create JSON manifests using the new schema
* Remove `nah manifest generate` from build scripts

### 2. Package Format: Custom Binary → Standard tar.gz

**v1.x:**

* Custom `.nap` binary format with embedded TLV manifest
* Mixed formats (binary + JSON)

**v2.0:**

* Standard `tar.gz` archives for both `.nap` and `.nak`
* JSON manifest at root of archive
* Use standard tools (`tar`, `gzip`) for inspection

**Migration:**

* Rebuild all packages with `nah pack`
* Old `.nap`/`.nak` files will not work

### 3. Manifest Structure: Flat → Nested

**v1.x (flat):**

```json
{
  "id": "com.example.app",
  "version": "1.0.0",
  "entrypoint": "bin/app",
  "nak_id": "com.example.sdk",
  "nak_version_req": ">=1.0.0"
}
```

**v2.0 (nested):**

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v1.json",
  "app": {
    "identity": {
      "id": "com.example.app",
      "version": "1.0.0",
      "nak_id": "com.example.sdk",
      "nak_version_req": ">=1.0.0"
    },
    "execution": {
      "entrypoint": "bin/app"
    },
    "layout": {
      "lib_dirs": ["lib"],
      "asset_dirs": ["assets"]
    }
  }
}
```

**Migration:**

* Use JSON Schema: `https://nah.rtorr.com/schemas/nap.v1.json`
* Restructure manifests into nested sections
* See [docs/schemas/README.md](docs/schemas/README.md) for full schema documentation

### 4. CMake Integration: Manifest Generation → Direct JSON

**v1.x:**

```cmake
nah_generate_manifest(myapp
    ID "com.example.app"
    VERSION "1.0.0"
    ENTRYPOINT "bin/app"
)

nah_package_nap(myapp)
```

**v2.0:**

```cmake
# Convenience wrapper
nah_app(myapp
    ID "com.example.app"
    VERSION "1.0.0"
    NAK "com.example.sdk"
    NAK_VERSION ">=1.0.0"
    ENTRYPOINT "bin/app"
    ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# Creates target: myapp_package
# Build with: make nah_package
```

Or use separate functions for more control:

```cmake
nah_app_manifest(myapp ...)  # Generates nap.json
nah_package(myapp ...)        # Creates tar.gz
```

**Migration:**

* Remove `nah_generate_manifest()` calls
* Replace with `nah_app()` or `nah_nak()` convenience functions
* Update build targets from `package_nap` to `nah_package`
* See [examples/cmake/NahAppTemplate.cmake](examples/cmake/NahAppTemplate.cmake)

### 5. CLI Changes: Simplified Commands

**v1.x:**

```bash
nah manifest generate manifest.json -o manifest.nah
nah app install myapp.nap
nah nak install sdk.nak
nah host install ./host_manifest
nah status app@version
nah profile list
```

**v2.0:**

```bash
nah install myapp.nap        # Unified install for apps and NAKs
nah install sdk.nak
nah list                     # Show installed packages
nah run com.example.app      # Run an app
nah show com.example.app     # Show details
nah pack ./app_dir           # Create package

# Removed commands:
# - nah manifest generate (no binary manifests)
# - nah host install (merged into nah install)
# - nah status (use nah show)
# - nah profile * (profiles removed)
```

**Migration:**

* Replace `nah manifest generate` with direct JSON creation
* Use `nah install` for all package types
* Use `nah show` instead of `nah status`
* No more profile commands (see below)

### 6. Host Configuration: Profiles → Single nah.json

**v1.x:**

* Multiple profiles in `<nah_root>/host/profiles/`
* `profile.current` symlink
* Profile switching with `nah profile set`

**v2.0:**

* Single `<nah_root>/host/nah.json` file
* No profile concept
* Simpler, single configuration

**v1.x host.json:**

```json
{
  "root": "/opt/nah",
  "host": { "environment": { "KEY": "value" } }
}
```

**v2.0 nah.json:**

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nah.v1.json",
  "host": {
    "root": "/opt/nah",
    "environment": {
      "DEPLOYMENT_ENV": "production"
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

**Migration:**

* Delete `<nah_root>/host/profiles/` directory
* Delete `<nah_root>/host/profile.current` symlink
* Rename `host.json` to `nah.json`
* Update structure to match schema
* Remove `--profile` flags from scripts

### 7. Environment Variables: Simple → Operations

**v1.x:**

```json
{
  "environment": {
    "PATH": "/custom/path",
    "MY_VAR": "value"
  }
}
```

**v2.0 (supports operations):**

```json
{
  "environment": {
    "MY_VAR": "value",
    "PATH": {
      "op": "prepend",
      "value": "/custom/path",
      "separator": ":"
    },
    "REMOVED_VAR": {
      "op": "unset"
    }
  }
}
```

Operations: `set`, `prepend`, `append`, `unset`

### 8. Library API: Same Core, New JSON Parser

**No breaking changes to core composition API.**

The library API remains largely compatible, but JSON parsing is enhanced:

```cpp
// v1.x and v2.0 - Same API
#define NAH_HOST_IMPLEMENTATION
#include <nah/nah_host.h>

auto host = nah::host::NahHost::create(nah_root);
auto result = host->getLaunchContract("com.example.app");
```

The JSON parser now handles both:

* v2.0 nested format (preferred)
* v1.x flat format (backward compatible for parsed JSON only)

**Note:** Binary manifests are NOT supported. Only JSON manifests work.

## Complete Migration Checklist

### For Application Developers

* \[ ] Create `nap.json` with nested structure
* \[ ] Update CMakeLists.txt to use `nah_app()` function
* \[ ] Change build target from `package_nap` to `nah_package`
* \[ ] Remove any `nah manifest generate` commands
* \[ ] Rebuild packages with `make nah_package`
* \[ ] Test installation with `nah install myapp-1.0.0.nap`

### For NAK/SDK Developers

* \[ ] Create `nak.json` at package root (not in `META/`)
* \[ ] Update CMakeLists.txt to use `nah_nak()` function
* \[ ] Change build target to `nah_package`
* \[ ] Rebuild packages
* \[ ] Update distribution documentation

### For Host Administrators

* \[ ] Backup existing NAH root
* \[ ] Remove all old `.nap` and `.nak` packages
* \[ ] Delete `<nah_root>/host/profiles/` directory
* \[ ] Rename `host.json` to `nah.json`
* \[ ] Update `nah.json` structure to match v2.0 schema
* \[ ] Remove `--profile` flags from automation scripts
* \[ ] Reinstall all packages from v2.0 builds

### For Library Users

* \[ ] Update includes (if using low-level APIs)
* \[ ] No changes needed if using `NahHost` class
* \[ ] Test with new JSON manifest format

## Examples

See the [examples/](examples/) directory for complete working examples:

* **[examples/apps/app/](examples/apps/app/)** - Simple C app with CMake
* **[examples/apps/script-app/](examples/apps/script-app/)** - Script-only app (no binary)
* **[examples/sdk/](examples/sdk/)** - Framework SDK (NAK)
* **[examples/host/](examples/host/)** - Host configuration

## Schema Documentation

Full JSON Schemas are available at:

* https://nah.rtorr.com/schemas/nap.v1.json (App manifest)
* https://nah.rtorr.com/schemas/nak.v1.json (NAK manifest)
* https://nah.rtorr.com/schemas/nah.v1.json (Host configuration)

See [docs/schemas/README.md](docs/schemas/README.md) for detailed documentation.

## Breaking Changes Summary

| Feature | v1.x | v2.0 |
|---------|------|------|
| Manifest Format | Binary TLV | JSON only |
| Package Format | Custom binary | Standard tar.gz |
| Manifest Location | `manifest.nah` or embedded | `nap.json`/`nak.json` at root |
| Host Config | `host.json` with profiles | `nah.json` single config |
| CLI Commands | `nah manifest`, `nah status`, `nah profile` | Removed |
| CMake Target | `package_nap`/`package_nak` | `nah_package` |
| Migration Path | N/A | **No migration - rebuild required** |

## Support

For questions about migration:

* Open an issue: https://github.com/rtorr/nah/issues
* See examples: https://github.com/rtorr/nah/tree/main/examples
* Read schemas: https://nah.rtorr.com/schemas/
