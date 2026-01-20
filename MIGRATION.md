# NAH Header-Only Library Migration Guide

## Overview

The header-only refactor represents a complete architectural redesign, moving from a traditional C++ library to a header-only library design. This is a **breaking change** with no direct migration path.

## Major Changes

### 1. Architecture
- **Old**: Traditional C++ library with separate compilation units
- **New**: Header-only library with pure functional core

### 2. API Changes

#### Composition API
```cpp
// Old (v1.0)
auto result = nah::compose(app_decl, profile, install_record, inventory, enable_trace);

// New (v2.0)
nah::core::CompositionOptions opts;
opts.enable_trace = enable_trace;
auto result = nah::core::nah_compose(app_decl, profile, install_record, inventory, opts);
```

#### Error Handling
```cpp
// Old (v1.0)
if (!result.ok) {
    std::cerr << result.error_message << std::endl;
}

// New (v2.0)
if (!result.ok) {
    std::cerr << result.critical_error_context << std::endl;
}
```

#### Namespaces
- `nah::` → `nah::core::`
- `nah::manifest::` → `nah::json::`
- `nah::platform::` → `nah::fs::`
- `nah::exec::` (new namespace)
- `nah::overrides::` (new namespace)

### 3. Header Files

#### Removed Headers
All headers in `include/nah/` have been replaced:
- `capabilities.hpp` → functionality in `nah_core.h`
- `compose.hpp` → `nah_core.h`
- `contract.hpp` → `nah_core.h`
- `manifest.hpp` → `nah_json.h`
- `platform.hpp` → `nah_fs.h`
- `packaging.hpp` → removed (CLI-only functionality)

#### New Headers
- `nah_core.h` - Core composition engine
- `nah_json.h` - JSON parsing and serialization
- `nah_fs.h` - Filesystem operations
- `nah_exec.h` - Process execution
- `nah_overrides.h` - Environment overrides

### 4. CLI Changes

The CLI has been completely reorganized with a modular command structure:

```bash
# Old (v1.0)
nah install app@version
nah run app@version

# New (v2.0) - Same interface, different implementation
nah install app@version
nah run app@version
```

### 5. Build System

#### CMake Changes
```cmake
# Old (v1.0)
find_package(NAH REQUIRED)
target_link_libraries(myapp NAH::nah)

# New (v2.0) - Header-only
find_package(NAH REQUIRED)
target_link_libraries(myapp NAH::nah)  # Interface library only
```

#### Dependencies
- No longer uses system libraries
- All dependencies via FetchContent or Conan
- zlib fetched automatically for tar/gzip support

## Migration Steps

### For Library Users

1. **Update includes**:
   ```cpp
   // Replace old headers
   #include <nah/compose.hpp>
   #include <nah/contract.hpp>

   // With new headers
   #include <nah/nah_core.h>
   ```

2. **Update namespace usage**:
   ```cpp
   // Old
   nah::compose(...)

   // New
   nah::core::nah_compose(...)
   ```

3. **Update error handling**:
   ```cpp
   // Check for critical_error_context instead of error_message
   if (!result.ok) {
       handle_error(result.critical_error_context);
   }
   ```

### For CLI Users

The CLI interface remains largely the same, but the underlying implementation has changed:

- All commands now use the header-only library
- Binary manifest parsing is fully supported
- tar/gzip archives are handled natively

### For Contributors

1. **New project structure**:
   ```
   include/nah/        # Header-only library
   tools/nah/          # CLI implementation
     commands/         # Modular command files
     main.cpp         # Entry point
   ```

2. **Testing**:
   - Tests reduced from 219 to 57 comprehensive tests
   - Focus on integration over unit testing
   - All tests updated for new API

## Breaking Changes Summary

1. **No backward compatibility** - Complete API redesign
2. **Header-only** - No more compiled library
3. **Pure functional core** - No I/O in composition engine
4. **New error types** - Result<T> pattern throughout
5. **Namespace reorganization** - More logical grouping

## Profile Removal (v2.1)

Version 2.1 removes the multi-profile system in favor of a simpler single `host.json` configuration.

### What Changed

1. **Profiles removed**: No more `<nah_root>/host/profiles/` directory or `profile.current` symlink
2. **Single host.json**: All host configuration is now in `<nah_root>/host/host.json`
3. **CLI changes**:
   - Removed `--profile` global flag
   - Removed `nah profile` command family (list, set, init, validate)
4. **Simplified composition**: No more profile-based warning policy upgrades or NAK binding modes

### Old Profile Format (removed)

```json
{
  "$schema": "nah.host.profile.v2",
  "nak": {
    "binding_mode": "canonical",
    "allow_versions": ["3.*"]
  },
  "environment": {
    "DEPLOYMENT_ENV": "production"
  },
  "warnings": {
    "nak_not_found": "error"
  },
  "capabilities": {
    "filesystem.read": "apparmor.profile.readonly"
  }
}
```

### New Host Environment Format

```json
{
  "environment": {
    "DEPLOYMENT_ENV": "production",
    "NAH_HOST_NAME": "myhost"
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

### Migration Steps

1. **Remove profiles directory**:
   ```bash
   rm -rf <nah_root>/host/profiles
   rm -f <nah_root>/host/profile.current
   ```

2. **Create host.json** from your default profile:
   ```bash
   # Extract environment from old profile
   cat <nah_root>/host/profiles/default.json | jq '{
     environment: .environment,
     paths: { library_prepend: [], library_append: [] },
     overrides: { allow_env_overrides: true, allowed_env_keys: [] }
   }' > <nah_root>/host/host.json
   ```

3. **Update scripts**: Remove `--profile` flags from any automation scripts.

4. **Update host manifests**: Change `host.json` format for `nah host install`:
   ```json
   {
     "$schema": "nah.host.manifest.v1",
     "root": "./nah_root",
     "host": {
       "environment": { "KEY": "value" }
     },
     "install": ["app.nap", "sdk.nak"]
   }
   ```

### Removed Features

- **NAK binding modes**: `canonical` and `mapped` modes are gone. NAK selection uses the highest compatible version at install time.
- **Warning policy**: All warnings now default to "warn". No ability to upgrade warnings to errors via configuration.
- **Capability mapping**: The `capabilities` section for mapping permissions to enforcement IDs is removed.

### API Changes

```cpp
// Old (v2.0)
auto profile = host->getActiveProfile();
host->setActiveProfile("production");
auto profiles = host->listProfiles();

// New (v2.1)
auto host_env = host->getHostEnvironment();
// No profile switching - single configuration
```

## Support

For questions about migration, please open an issue on the GitHub repository.