# NAH Manifest Format - The Simple Design

## Executive Summary

This document proposes a **radically simplified** NAH manifest and packaging architecture:

1. **Role-specific manifests** - `app.json`, `nak.json`, `host.json` (filename tells you what it is)
2. **No custom binary format** - Use standard tar.gz archives (`.nap` and `.nak` are just tarballs)
3. **Unified CLI** - `nah install <source>` auto-detects everything
4. **Zero cognitive overhead** - Everyone knows tar, everyone knows JSON

**Core Insight:** Format unification only matters for developer experience, CLI simplicity, and cognitive load. Runtime doesn't care—by the time code runs, everything is already unpacked and resolved.

***

## Design Philosophy

### What Matters at Runtime

At execution time (`nah run myapp`), the runtime needs:

* Path to executable: `/nah/apps/myapp-1.0.0/bin/app`
* Paths to NAK libraries: `/nah/naks/sdk-2.1.0/lib/libsdk.so`
* Environment variables
* Asset directories

**The original manifest format is irrelevant.** Everything is:

* ✅ Already unpacked to disk
* ✅ Dependencies resolved
* ✅ Paths recorded in install records
* ✅ Ready to execute

### What Matters at Build/Install Time

Format design optimizes for:

1. **Developer ergonomics** (authoring manifests)
2. **CLI simplicity** (implementing commands)
3. **Cognitive load** (understanding the system)
4. **Standard tooling** (using familiar tools)

Performance at build/install time is **not a concern**—disk is cheap, bandwidth is fast, and these operations happen rarely.

***

## Core Architecture

### Three Manifest Types

**NAH-specific filenames (match package extensions):**

| Role | Manifest File | Ownership | Mutability | Package Type |
|------|---------------|-----------|------------|--------------|
| App Developer | `nap.json` | Developer | Immutable | `.nap` |
| NAK Vendor | `nak.json` | Vendor | Immutable | `.nak` |
| System Admin | `nah.json` | Admin | Mutable | N/A |

**Design principle:** Manifest names match package extensions. A `.nap` file contains `nap.json`. Clear hierarchy: `nah` (system) → `nap` (app) → `nak` (SDK).

### Package Formats

**Standard archives (tar.gz):**

```
myapp-1.0.0.nap         (just a tar.gz)
├── nap.json            ← App manifest
├── bin/
│   └── myapp
└── assets/
    └── config.json

engine-2.1.0.nak        (just a tar.gz)
├── nak.json            ← NAK manifest
└── lib/
    └── libengine.so
```

**Design principle:** Use standard tooling everyone already knows.

### CLI Auto-Detection

```bash
# Works for everything
nah install ./myapp-1.0.0.nap      # Detects .nap extension
nah install ./engine-2.1.0.nak     # Detects .nak extension
nah install ./my-app-src/          # Reads nap.json, packs and installs
nah install ./my-host-config/      # Reads nah.json, bootstraps host
```

**Design principle:** One command that figures out what to do.

***

## Benefits Over Custom Binary Format (TLV)

### What TLV Would Give Us

1. **Fast seeking to sections** → Not needed (install time perf doesn't matter)
2. **Compact binary size** → Not needed (disk is cheap, bandwidth is fast)
3. **Type safety at binary level** → Not needed (JSON gives type safety)
4. **Embeddable in binaries** → Nice-to-have, but not critical

### What tar.gz Gives Us

1. ✅ **Massive implementation simplicity** (90% less code)
2. ✅ **Standard tooling** (`tar`, `gzip`, `libarchive`)
3. ✅ **Human-friendly** (developers can manually create packages)
4. ✅ **Debuggable** ("just unzip it and look")
5. ✅ **CI/CD friendly** (inspect without custom tools)
6. ✅ **Universal platform support** (tar works everywhere)

### Implementation Comparison

**With TLV:**

```cpp
// Custom TLV writer (200+ lines)
struct TLVWriter {
    void write_tag(uint16_t tag, const std::string& value);
    void write_header();
    uint32_t calculate_crc();
    // ... 20 more methods
};

// Custom TLV reader (200+ lines)
struct TLVReader {
    std::optional<std::string> read_tag(uint16_t tag);
    bool validate_header();
    bool verify_crc();
    // ... 20 more methods
};
```

**With tar.gz:**

```bash
# Pack
nah pack              # Just runs: tar -czf myapp-1.0.0.nap -C src/ .

# Install  
nah install pkg.nap   # Just runs: tar -xzf pkg.nap -C /nah/apps/myapp-1.0.0/
```

***

## Manifest Specifications

### 1. App Manifest (`nap.json`)

**Developer-owned, immutable, packaged in `.nap` files**

```json
{
  "$schema": "https://nah.rtorr.com/schemas/app.v1.json",
  "app": {
    "identity": {
      "id": "com.example.myapp",
      "version": "1.0.0",
      "nak_id": "com.example.sdk",
      "nak_version_req": "^1.2.0"
    },
    "execution": {
      "entrypoint": "bin/myapp",
      "args": []
    },
    "layout": {
      "lib_dirs": ["lib"],
      "asset_dirs": ["assets"]
    },
    "environment": {
      "APP_MODE": "production"
    },
    "permissions": {
      "filesystem": ["/data", "/tmp"],
      "network": ["0.0.0.0:8080"]
    }
  }
}
```

**Location in package:** `.nap` root (e.g., `myapp-1.0.0.nap/nap.json`)

**What it declares:**

* Identity (id, version, NAK dependency)
* How to execute (entrypoint, args)
* What it needs (directories, environment)
* What it's allowed to do (permissions)

### 2. NAK Manifest (`nak.json`)

**Vendor-owned, immutable, packaged in `.nak` files**

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nak.v1.json",
  "nak": {
    "identity": {
      "id": "com.example.sdk",
      "version": "1.2.3"
    },
    "paths": {
      "resource_root": "resources",
      "lib_dirs": ["lib", "lib/x64"]
    },
    "environment": {
      "SDK_MODE": "release"
    },
    "loader": {
      "exec_path": "bin/loader",
      "args_template": [
        "{NAH_APP_ROOT}",
        "{NAH_NAK_ROOT}"
      ]
    },
    "execution": {
      "cwd": "{NAH_APP_ROOT}"
    }
  }
}
```

**Location in package:** `.nak` root (e.g., `sdk-1.2.3.nak/nak.json`)

**What it declares:**

* Identity (id, version)
* Resource layout (where libs and resources are)
* Loader details (how to bootstrap the NAK)
* Environment needs

### 3. Host Configuration (`nah.json`)

**Admin-owned, mutable, lives at NAH root**

```json
{
  "$schema": "https://nah.rtorr.com/schemas/host.v1.json",
  "host": {
    "root": "/opt/nah",
    "name": "production-host",
    "environment": {
      "NAH_MODE": "production",
      "LOG_LEVEL": "info"
    },
    "paths": {
      "library_prepend": ["/usr/local/lib"],
      "library_append": ["/opt/vendor/lib"]
    },
    "overrides": {
      "allow_env_overrides": true
    }
  }
}
```

**Location:** `<NAH_ROOT>/host/nah.json`

**What it declares:**

* Host-wide configuration
* Environment variables for all apps
* Library search paths
* Policy settings

### 4. Host Bootstrap Manifest (`nah.json` with install)

**Used to set up a new host environment**

```json
{
  "$schema": "https://nah.rtorr.com/schemas/host.v1.json",
  "host": {
    "root": "./nah_root",
    "name": "dev-environment",
    "environment": {
      "NAH_MODE": "development"
    }
  },
  "install": [
    "packages/myapp-1.0.0.nap",
    "packages/sdk-1.2.3.nak"
  ]
}
```

**When `install[]` is present:** Acts as bootstrap manifest for `nah install ./host-config/`

**What it does:**

1. Creates NAH root at `host.root`
2. Writes operational `nah.json` (without `install[]`)
3. Installs all packages listed in `install[]`

### 5. App Install Record (Host Registry)

**Host-owned, mutable, created at install time**

```json
{
  "$schema": "https://nah.rtorr.com/schemas/app-record.v1.json",
  "app": {
    "id": "com.example.myapp",
    "version": "1.0.0"
  },
  "nak": {
    "id": "com.example.sdk",
    "version": "1.2.3",
    "record_ref": "../naks/com.example.sdk@1.2.3.json"
  },
  "paths": {
    "install_root": "/opt/nah/apps/com.example.myapp-1.0.0"
  },
  "install": {
    "instance_id": "uuid-1234",
    "timestamp": "2026-01-20T10:00:00Z",
    "source": "/tmp/myapp-1.0.0.nap"
  },
  "provenance": {
    "signature": "...",
    "cert_chain": "..."
  },
  "overrides": {
    "environment": {}
  }
}
```

**Location:** `<NAH_ROOT>/registry/apps/com.example.myapp@1.0.0.json`

### 6. NAK Install Record (Host Registry)

**Host-owned, mutable, created at install time**

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nak-record.v1.json",
  "nak": {
    "id": "com.example.sdk",
    "version": "1.2.3"
  },
  "paths": {
    "root": "/opt/nah/naks/com.example.sdk-1.2.3",
    "resource_root": "/opt/nah/naks/com.example.sdk-1.2.3/resources",
    "lib_dirs": [
      "/opt/nah/naks/com.example.sdk-1.2.3/lib"
    ]
  },
  "environment": {
    "SDK_MODE": "release"
  },
  "loader": {
    "exec_path": "/opt/nah/naks/com.example.sdk-1.2.3/bin/loader",
    "args_template": ["{NAH_APP_ROOT}", "{NAH_NAK_ROOT}"]
  },
  "install": {
    "timestamp": "2026-01-20T10:00:00Z",
    "source": "/tmp/sdk-1.2.3.nak"
  },
  "provenance": {
    "signature": "...",
    "cert_chain": "..."
  }
}
```

**Location:** `<NAH_ROOT>/registry/naks/com.example.sdk@1.2.3.json`

***

## Validation Against SPEC.md

### 1. Ownership Model (SPEC.md §Definitions and Ownership)

**SPEC Requirement:**

> Applications declare intent and requirements. Hosts declare bindings and enforcement. NAH composes them deterministically.

**Analysis:** ✅ **PRESERVED AND STRENGTHENED**

Ownership boundaries are **clearer** with role-specific files:

* `app.json` → Developer-owned (immutable, packaged in `.nap`)
* `nak.json` → Vendor-owned (immutable, packaged in `.nak`)
* `host.json` → Admin-owned (mutable, at NAH root)
* `*-record.json` → Host-owned (mutable, in registry)

**Verdict:** Ownership is more obvious because **the filename matches the package type and documents the role**.

### 2. Binary Manifest Format (SPEC.md §Binary Manifest Format)

**Current SPEC:**

> Embedded manifests use TLV for C linkability

**Analysis:** ⚠️ **REMOVED FOR SIMPLICITY**

**What we remove:**

* Binary TLV format for all use cases
* Embedded manifest capability (was nice-to-have)
* C-compatible manifest embedding

**Why this is acceptable:**

* Runtime doesn't care about manifest format (everything is already resolved by install time)
* Embedding was a demo feature, not critical to NAH's value proposition
* Apps can still include `app.json` as a resource file if needed for introspection
* Simplicity > theoretical embeddability

**Alternatives if needed later:**

* Simple binary format just for embedding (separate from package format)
* Embed JSON as string literal (works, slightly larger, totally fine)

**Verdict:** **Acceptable trade-off**. Package format doesn't need to be binary. If embedding becomes critical, add it separately.

### 3. Package Formats (SPEC.md §NAP Package Format, §NAK Pack Format)

**Current SPEC:**

* `.nap` packages are custom binary TLV
* `.nak` packages contain `META/nak.json`

**Proposed:**

* `.nap` packages are `tar.gz` archives with `nap.json` at root
* `.nak` packages are `tar.gz` archives with `nak.json` at root

**Migration:**

```bash
# Old format
myapp.nap (binary TLV)
sdk.nak/META/nak.json

# New format  
myapp-1.0.0.nap (tar.gz with nap.json at root)
sdk-1.2.3.nak (tar.gz with nak.json at root)
```

**Verdict:** ✅ **Simpler**. Standard archive format is easier to work with.

### 4. Host Configuration

**Current Confusion:**

* "Host Environment" vs "Host Setup" (two concepts, unclear boundary)

**Resolution:**

* Single `nah.json` format
* When used with `install[]`: Bootstrap manifest
* When used at NAH root (without `install[]`): Operational config

**Verdict:** ✅ **UNIFIED**. Same format, different use cases.

***

## CLI Integration

### Unified Install Command

```bash
nah install <source>
```

**Auto-detection logic:**

1. **Package files (by extension):**
   ```bash
   nah install ./myapp-1.0.0.nap   # Extension = .nap → app package
   nah install ./sdk-1.2.3.nak     # Extension = .nak → NAK package
   ```

2. **Directories (read manifest):**
   ```bash
   nah install ./myapp/            # Reads app.json → pack + install
   nah install ./my-sdk/           # Reads nak.json → pack + install
   nah install ./host-config/      # Reads host.json → bootstrap host
   ```

3. **Error handling:**
   ```bash
   nah install ./mystery/          # No manifest found → error
   ```

### Implementation

```python
def detect_and_install(source):
    # Extension-based detection
    if source.endswith('.nap'):
        return install_app_package(source)
    elif source.endswith('.nak'):
        return install_nak_package(source)
    
    # Directory-based detection
    elif is_directory(source):
        if exists(join(source, 'app.json')):
            return pack_and_install_app(source)
        elif exists(join(source, 'nak.json')):
            return pack_and_install_nak(source)
        elif exists(join(source, 'host.json')):
            return bootstrap_host(source)
        else:
            error("No manifest found (expected app.json, nak.json, or host.json)")
    
    else:
        error("Source must be .nap, .nak, or directory with manifest")
```

### Pack Command

```bash
nah pack                # Auto-detects manifest in current directory
nah pack ./myapp/       # Explicit source directory
```

**Implementation:**

```python
def pack(source='.'):
    if exists(join(source, 'app.json')):
        manifest = read_json(join(source, 'app.json'))
        app_id = manifest['app']['identity']['id']
        version = manifest['app']['identity']['version']
        output = f"{app_id}-{version}.nap"
        run(f"tar -czf {output} -C {source} .")
        return output
    
    elif exists(join(source, 'nak.json')):
        manifest = read_json(join(source, 'nak.json'))
        nak_id = manifest['nak']['identity']['id']
        version = manifest['nak']['identity']['version']
        output = f"{nak_id}-{version}.nak"
        run(f"tar -czf {output} -C {source} .")
        return output
    
    else:
        error("No manifest found")
```

**Actual pack is just tar:**

```bash
tar -czf myapp-1.0.0.nap -C ./myapp/ .
tar -czf sdk-1.2.3.nak -C ./sdk/ .
```

### List Command

```bash
nah list               # List all installed apps and NAKs
nah list apps          # List only apps
nah list naks          # List only NAKs
```

**Implementation:**

```python
def list_installed(filter='all'):
    nah_root = get_nah_root()
    
    if filter in ['all', 'apps']:
        app_records = glob(f"{nah_root}/registry/apps/*.json")
        for record_path in app_records:
            record = read_json(record_path)
            print(f"app: {record['app']['id']} @ {record['app']['version']}")
    
    if filter in ['all', 'naks']:
        nak_records = glob(f"{nah_root}/registry/naks/*.json")
        for record_path in nak_records:
            record = read_json(record_path)
            print(f"nak: {record['nak']['id']} @ {record['nak']['version']}")
```

### Show Command

```bash
nah show com.example.myapp           # Show app details
nah show com.example.sdk             # Show NAK details
```

### Examples

```bash
# Install from packages
nah install myapp-1.0.0.nap
nah install sdk-1.2.3.nak

# Install from source directories
nah install ./my-app-src/
nah install ./my-sdk-src/

# Bootstrap a new host
nah install ./production-host-config/

# Pack for distribution
cd ./my-app-src/
nah pack                              # Creates myapp-1.0.0.nap

# List installed
nah list
nah list apps
nah list naks

# Show details
nah show com.example.myapp
```

**Clean, unified CLI with no special cases.**

***

## Implementation Roadmap

**NO BACKWARD COMPATIBILITY. THIS IS THE NEW TRUTH.**

This is a clean-slate redesign at an early stage (v1.0.25). Old formats will not be supported.

### Implementation Timeline: 2-3 Weeks

This is a **deletion and simplification** effort, not a migration. The vast majority of work is removing code.

***

## What Changes in the Codebase

### Files to DELETE (Complete Removal)

**1. Binary Manifest Generation:**

* `tools/nah/commands/manifest.cpp` - **DELETE ENTIRELY** (215 lines)
  * `ManifestFieldType` enum
  * `write_field()` TLV writer
  * `write_array_field()` TLV writer
  * `cmd_generate()` binary manifest generator
  * All TLV encoding logic

**2. Binary Manifest Parsing in Install:**

* `tools/nah/commands/install.cpp` (lines 230-320) - **DELETE SECTION**
  * Binary manifest reader (`manifest.nah` parsing)
  * TLV decoding loop
  * Magic header validation
  * Field type switch statement
  * \~90 lines of TLV parsing code

**3. CMake Manifest Generation:**

* `examples/cmake/NahAppTemplate.cmake` (lines 121-135) - **DELETE SECTION**
  * `nah_generate_manifest()` function (calls `nah manifest generate`)
  * References to `.nah` binary manifest files

**4. Example Binary Manifests:**

* `examples/apps/app_c/src/main.cpp` (lines 25-77) - **DELETE SECTION**
  * `embedded_manifest[]` byte array
  * All embedded manifest demonstration code
  * Manifest inspection/debugging code

**Total Deletion:** ~400 lines of custom binary format code

### Files to MODIFY

**1. `tools/nah/commands/install.cpp`**

**Change: Manifest detection and reading**

Current code (lines 221-320):

```cpp
// Tries: nah.json, META/nak.json, META/app.json, manifest.nah (binary)
auto manifest_content = nah::fs::read_file(source_dir + "/nah.json");
if (!manifest_content) {
    manifest_content = nah::fs::read_file(source_dir + "/META/nak.json");
}
if (!manifest_content) {
    manifest_content = nah::fs::read_file(source_dir + "/META/app.json");
}
if (!manifest_content) {
    // 90 lines of binary manifest parsing
    auto binary_manifest = nah::fs::read_file(source_dir + "/manifest.nah");
    // ... TLV decoding ...
}
```

**Replace with:**

```cpp
// Try NAH-specific manifests (match package extensions)
auto manifest_content = nah::fs::read_file(source_dir + "/nap.json");
if (!manifest_content) {
    manifest_content = nah::fs::read_file(source_dir + "/nak.json");
}
if (!manifest_content) {
    manifest_content = nah::fs::read_file(source_dir + "/nah.json");
}
if (!manifest_content) {
    print_error("No manifest found (expected nap.json, nak.json, or nah.json)", opts.json);
    return 1;
}

// Parse JSON
try {
    manifest = nlohmann::json::parse(*manifest_content);
} catch (const std::exception& e) {
    print_error("Invalid manifest JSON: " + std::string(e.what()), opts.json);
    return 1;
}

// Detect type from structure
bool is_app = manifest.contains("app") && manifest["app"].is_object();
bool is_nak = manifest.contains("nak") && manifest["nak"].is_object();
bool is_host = manifest.contains("host") && manifest["host"].is_object();

// Extract identity fields based on type
if (is_app) {
    id = manifest["app"]["identity"]["id"];
    version = manifest["app"]["identity"]["version"];
    // ... app-specific install logic
} else if (is_nak) {
    id = manifest["nak"]["identity"]["id"];
    version = manifest["nak"]["identity"]["version"];
    // ... nak-specific install logic
} else if (is_host) {
    // ... host bootstrap logic
} else {
    print_error("Invalid manifest structure", opts.json);
    return 1;
}
```

**Impact:** Delete ~90 lines of TLV parsing, add ~40 lines of JSON structure detection. **Net: -50 lines, simpler.**

**2. `tools/nah/commands/install.cpp` - `detect_source_type()`**

Current code (lines 198-210):

```cpp
// Directory - check for host manifest
if (nah::fs::is_directory(source)) {
    auto manifest_path = source + "/nah.json";
    auto content = nah::fs::read_file(manifest_path);
    if (content) {
        try {
            auto j = nlohmann::json::parse(*content);
            if (j.contains("root")) {  // Heuristic for host
                return SourceType::Host;
            }
        } catch (...) {}
    }
    return SourceType::Directory;
}
```

**Replace with:**

```cpp
// Directory - check manifest type
if (nah::fs::is_directory(source)) {
    // Try each manifest file (names match package extensions)
    auto app_manifest = nah::fs::read_file(source + "/nap.json");
    if (app_manifest) return SourceType::Directory; // App source
    
    auto nak_manifest = nah::fs::read_file(source + "/nak.json");
    if (nak_manifest) return SourceType::Directory; // NAK source
    
    auto host_manifest = nah::fs::read_file(source + "/nah.json");
    if (host_manifest) return SourceType::Host; // Host config
    
    print_error("No manifest found in directory", opts.json);
    return SourceType::Unknown;
}
```

**Impact:** More explicit, easier to understand.

**3. `tools/nah/commands/pack.cpp`**

Current code (lines 25-58):

```cpp
// Read manifest
auto manifest_content = nah::fs::read_file(source_dir + "/nah.json");
// ... parse flat JSON
// ... detect type by heuristics (loaders presence, entrypoint presence)

std::string id = manifest.value("id", "");
std::string version = manifest.value("version", "");

bool is_nak = manifest.contains("loaders") || 
              (manifest.contains("lib_dirs") && !manifest.contains("entrypoint_path"));
```

**Replace with:**

```cpp
// Detect manifest type by filename (matches package extension)
std::string manifest_path;
std::string manifest_type;

if (nah::fs::file_exists(source_dir + "/nap.json")) {
    manifest_path = source_dir + "/nap.json";
    manifest_type = "nap";
} else if (nah::fs::file_exists(source_dir + "/nak.json")) {
    manifest_path = source_dir + "/nak.json";
    manifest_type = "nak";
} else {
    print_error("No manifest found (expected nap.json or nak.json)", opts.json);
    return 1;
}

auto manifest_content = nah::fs::read_file(manifest_path);
auto manifest = nlohmann::json::parse(*manifest_content);

// Extract identity based on type
std::string id, version;
if (manifest_type == "app") {
    id = manifest["app"]["identity"]["id"];
    version = manifest["app"]["identity"]["version"];
} else if (manifest_type == "nak") {
    id = manifest["nak"]["identity"]["id"];
    version = manifest["nak"]["identity"]["version"];
}

std::string ext = "." + manifest_type;  // .nap or .nak
std::string output_path = id + "-" + version + ext;

// Create tar.gz package
std::string cmd = "tar -czf " + output_path + " -C " + source_dir + " .";
int result = std::system(cmd.c_str());

if (result == 0) {
    if (opts.json) {
        nlohmann::json j;
        j["ok"] = true;
        j["package"] = output_path;
        output_json(j);
    } else {
        std::cout << "Created " << output_path << std::endl;
    }
} else {
    print_error("Failed to create package", opts.json);
    return 1;
}

return 0;
```

**Impact:** Complete implementation (was stubbed), uses system `tar` instead of custom format. ~30 lines.

**4. `tools/nah/commands/host.cpp`**

This command likely implements `nah host install`.

**Change:** This command should be **REMOVED** or **MERGED** into `nah install`.

If `host.cpp` only handles host-specific operations, merge that logic into `install.cpp` as a branch of the auto-detection flow.

**Impact:** -1 command file, simpler CLI surface.

**5. `examples/cmake/NahAppTemplate.cmake`**

Current packaging function (lines 137-180):

```cmake
function(nah_package_nap TARGET_NAME APP_ID APP_VERSION)
    # ...
    # Copy manifest.nah (binary)
    COMMAND ${CMAKE_COMMAND} -E copy ${MANIFEST_NAH} ${NAP_STAGING_DIR}/manifest.nah
    # ...
    # Package via tar
    COMMAND ${CMAKE_COMMAND} -E tar czf ${NAP_FILE} --format=gnutar .
    # ...
endfunction()
```

**Replace with:**

```cmake
function(nah_package_nap TARGET_NAME APP_ID APP_VERSION)
    set(NAP_STAGING_DIR "${CMAKE_BINARY_DIR}/${TARGET_NAME}_nap_staging")
    set(NAP_FILE "${CMAKE_BINARY_DIR}/${APP_ID}-${APP_VERSION}.nap")

    # Stage NAP (no binary manifest needed)
    add_custom_target(${TARGET_NAME}_stage_nap
        COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/assets
        
        # Copy binary
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_NAME}> ${NAP_STAGING_DIR}/bin/
        
        # Copy nap.json manifest
        COMMAND ${CMAKE_COMMAND} -E copy 
            ${CMAKE_CURRENT_SOURCE_DIR}/nap.json 
            ${NAP_STAGING_DIR}/nap.json
        
        DEPENDS ${TARGET_NAME}
        COMMENT "Staging NAP: ${APP_ID}@${APP_VERSION}"
    )
    
    # Copy assets if provided
    if(ARG_ASSETS_DIR AND EXISTS "${ARG_ASSETS_DIR}")
        add_custom_command(TARGET ${TARGET_NAME}_stage_nap POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${ARG_ASSETS_DIR} ${NAP_STAGING_DIR}/assets
        )
    endif()

    # Package via tar
    add_custom_target(${TARGET_NAME}_package_nap
        COMMAND ${CMAKE_COMMAND} -E tar czf ${NAP_FILE} --format=gnutar .
        WORKING_DIRECTORY ${NAP_STAGING_DIR}
        DEPENDS ${TARGET_NAME}_stage_nap
        COMMENT "Packaging NAP: ${NAP_FILE}"
    )

    # Convenience target
    if(NOT TARGET package_nap)
        add_custom_target(package_nap DEPENDS ${TARGET_NAME}_package_nap)
    else()
        add_dependencies(package_nap ${TARGET_NAME}_package_nap)
    endif()
endfunction()

# DELETE nah_generate_manifest() - no longer needed
```

**Impact:** Simpler, no binary manifest generation step.

**6. `examples/apps/app_c/CMakeLists.txt`**

Current:

* Generates binary manifest header via `bin2header.cmake`
* Embeds `embedded_manifest[]` in C++ code
* Includes binary manifest in staging

**Replace with:**

* Create `app.json` at root
* Copy `app.json` to staging directory
* No embedding needed

**Create `examples/apps/app_c/app.json`:**

```json
{
  "$schema": "https://nah.rtorr.com/schemas/app.v1.json",
  "app": {
    "identity": {
      "id": "com.example.app_c",
      "version": "1.0.0",
      "nak_id": "com.example.sdk",
      "nak_version_req": "^1.2.0"
    },
    "execution": {
      "entrypoint": "bin/app_c",
      "args": []
    },
    "layout": {
      "lib_dirs": ["lib"],
      "asset_dirs": ["assets"]
    }
  }
}
```

**Impact:** +1 JSON file, delete embedded manifest code, simpler build.

**7. `examples/sdk/META/nak.json.in` and `examples/conan-sdk/META/nak.json.in`**

**Rename and restructure:**

From: `META/nak.json.in`

```json
{
  "nak": {
    "id": "@NAK_ID@",
    "version": "@NAK_VERSION@"
  },
  "paths": { ... },
  "loaders": { ... }
}
```

To: `nak.json` (at root)

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nak.v1.json",
  "nak": {
    "identity": {
      "id": "@NAK_ID@",
      "version": "@NAK_VERSION@"
    },
    "paths": {
      "lib_dirs": ["lib"],
      "resource_root": "resources"
    },
    "loader": {
      "exec_path": "bin/engine-loader",
      "args_template": [
        "--app-entry", "{NAH_APP_ENTRY}",
        "--app-root", "{NAH_APP_ROOT}",
        "--app-id", "{NAH_APP_ID}",
        "--engine-root", "{NAH_NAK_ROOT}"
      ]
    },
    "execution": {
      "cwd": "{NAH_APP_ROOT}"
    },
    "environment": {
      "GAMEENGINE_VERSION": "@NAK_VERSION@",
      "GAMEENGINE_LOG_LEVEL": "info"
    }
  }
}
```

**Impact:** Move from `META/` to root, add structure nesting.

**8. `examples/host/nah.json`**

Likely already has the right structure (may need to rename from `host.json`). Verify it follows the new schema:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nah.v1.json",
  "host": {
    "root": "/opt/nah",
    "environment": { ... },
    "paths": { ... }
  }
}
```

**Impact:** Minimal or no changes needed.

**9. SPEC.md**

**Changes:**

* Remove all references to binary TLV format (lines 1527-1567)
* Remove "Binary Manifest Format" section entirely
* Update package format descriptions to tar.gz
* Remove embedded manifest references
* Update examples to use JSON manifests

**Impact:** Major documentation simplification.

**10. Documentation files:**

* `docs/concepts.md` - Remove TLV format section, update to JSON + tar.gz
* `docs/cli.md` - Remove `nah manifest generate` command, update `nah pack` docs
* `docs/getting-started-app.md` - Remove embedded manifest section
* `docs/getting-started-bundle.md` - Update manifest references
* `docs/troubleshooting.md` - Remove binary manifest debugging

**Impact:** Simpler docs focused on JSON manifests.

***

## CMake Integration (Improved, Not Lost)

**Critical: CMake integration gets BETTER, not removed.**

### What Changes

**Current Flow (Complex):**

```cmake
# 1. Write manifest as JSON string in CMakeLists.txt
set(MANIFEST_JSON "{\"app\": {...}}")

# 2. Generate binary manifest (requires nah CLI at build time)
nah_generate_manifest(my_app "${MANIFEST_JSON}")
  → Calls `nah manifest generate`
  → Produces binary manifest.nah

# 3. Package
nah_package_nap(my_app "com.example.app" "1.0.0"
    ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
  → Copies manifest.nah to staging
  → Runs tar
```

**Problems:**

* Requires `nah` CLI installed at build time
* Binary generation is unnecessary complexity
* Two-step process
* Manifest is embedded JSON string (hard to read/maintain)

**New Flow (Simpler):**

```cmake
# 1. Declare manifest with type-safe CMake API
nah_app_manifest(my_app
    ID "com.example.app"
    VERSION "1.0.0"
    NAK_ID "com.example.sdk"
    NAK_VERSION "^1.0.0"
    ENTRYPOINT "bin/my_app"
    ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# 2. Package (automatic)
nah_package(my_app)
  → Writes app.json directly
  → Stages files
  → Creates .nap with tar
```

**Benefits:**

* ✅ No CLI dependency at build time
* ✅ Type-safe CMake API (validate at configure time)
* ✅ Direct JSON generation (no intermediate steps)
* ✅ One function call
* ✅ Cleaner CMakeLists.txt

### Updated CMake Template

**File: `examples/cmake/NahAppTemplate.cmake`**

**New functions to provide:**

```cmake
################################################################################
# nah_app_manifest() - Generate app.json from CMake variables
################################################################################
function(nah_app_manifest TARGET_NAME)
    cmake_parse_arguments(ARG 
        "" 
        "ID;VERSION;NAK_ID;NAK_VERSION;ENTRYPOINT" 
        "LIB_DIRS;ASSET_DIRS;ENV_VARS;ARGS" 
        ${ARGN}
    )

    # Validate required fields
    if(NOT ARG_ID)
        message(FATAL_ERROR "nah_app_manifest: ID is required")
    endif()
    if(NOT ARG_VERSION)
        message(FATAL_ERROR "nah_app_manifest: VERSION is required")
    endif()
    if(NOT ARG_ENTRYPOINT)
        set(ARG_ENTRYPOINT "bin/${TARGET_NAME}")
    endif()

    # Build nap.json content
    set(MANIFEST_JSON "{\n")
    string(APPEND MANIFEST_JSON "  \"$schema\": \"https://nah.rtorr.com/schemas/nap.v1.json\",\n")
    string(APPEND MANIFEST_JSON "  \"app\": {\n")
    string(APPEND MANIFEST_JSON "    \"identity\": {\n")
    string(APPEND MANIFEST_JSON "      \"id\": \"${ARG_ID}\",\n")
    string(APPEND MANIFEST_JSON "      \"version\": \"${ARG_VERSION}\"")

    # Optional NAK dependency
    if(ARG_NAK_ID)
        string(APPEND MANIFEST_JSON ",\n")
        string(APPEND MANIFEST_JSON "      \"nak_id\": \"${ARG_NAK_ID}\",\n")
        string(APPEND MANIFEST_JSON "      \"nak_version_req\": \"${ARG_NAK_VERSION}\"")
    endif()

    string(APPEND MANIFEST_JSON "\n    },\n")
    
    # Execution section
    string(APPEND MANIFEST_JSON "    \"execution\": {\n")
    string(APPEND MANIFEST_JSON "      \"entrypoint\": \"${ARG_ENTRYPOINT}\"")
    
    if(ARG_ARGS)
        string(APPEND MANIFEST_JSON ",\n      \"args\": [")
        list(LENGTH ARG_ARGS args_count)
        math(EXPR args_last "${args_count} - 1")
        set(i 0)
        foreach(arg ${ARG_ARGS})
            string(APPEND MANIFEST_JSON "\"${arg}\"")
            if(i LESS args_last)
                string(APPEND MANIFEST_JSON ", ")
            endif()
            math(EXPR i "${i} + 1")
        endforeach()
        string(APPEND MANIFEST_JSON "]")
    endif()
    
    string(APPEND MANIFEST_JSON "\n    },\n")

    # Layout section
    string(APPEND MANIFEST_JSON "    \"layout\": {\n")
    
    if(ARG_LIB_DIRS)
        string(APPEND MANIFEST_JSON "      \"lib_dirs\": [")
        list(LENGTH ARG_LIB_DIRS lib_count)
        math(EXPR lib_last "${lib_count} - 1")
        set(i 0)
        foreach(dir ${ARG_LIB_DIRS})
            string(APPEND MANIFEST_JSON "\"${dir}\"")
            if(i LESS lib_last)
                string(APPEND MANIFEST_JSON ", ")
            endif()
            math(EXPR i "${i} + 1")
        endforeach()
        string(APPEND MANIFEST_JSON "],\n")
    else()
        string(APPEND MANIFEST_JSON "      \"lib_dirs\": [\"lib\"],\n")
    endif()

    if(ARG_ASSET_DIRS)
        string(APPEND MANIFEST_JSON "      \"asset_dirs\": [")
        list(LENGTH ARG_ASSET_DIRS asset_count)
        math(EXPR asset_last "${asset_count} - 1")
        set(i 0)
        foreach(dir ${ARG_ASSET_DIRS})
            string(APPEND MANIFEST_JSON "\"${dir}\"")
            if(i LESS asset_last)
                string(APPEND MANIFEST_JSON ", ")
            endif()
            math(EXPR i "${i} + 1")
        endforeach()
        string(APPEND MANIFEST_JSON "]\n")
    else()
        string(APPEND MANIFEST_JSON "      \"asset_dirs\": [\"assets\"]\n")
    endif()

    string(APPEND MANIFEST_JSON "    }\n")

    # Environment (optional)
    if(ARG_ENV_VARS)
        string(APPEND MANIFEST_JSON ",\n    \"environment\": {\n")
        list(LENGTH ARG_ENV_VARS env_count)
        math(EXPR env_last "${env_count} - 1")
        set(i 0)
        foreach(env_var ${ARG_ENV_VARS})
            string(REGEX MATCH "^([^=]+)=(.*)$" _ ${env_var})
            set(env_key ${CMAKE_MATCH_1})
            set(env_val ${CMAKE_MATCH_2})
            string(APPEND MANIFEST_JSON "      \"${env_key}\": \"${env_val}\"")
            if(i LESS env_last)
                string(APPEND MANIFEST_JSON ",")
            endif()
            string(APPEND MANIFEST_JSON "\n")
            math(EXPR i "${i} + 1")
        endforeach()
        string(APPEND MANIFEST_JSON "    }\n")
    endif()

    string(APPEND MANIFEST_JSON "  }\n")
    string(APPEND MANIFEST_JSON "}\n")

    # Write nap.json to build directory
    set(MANIFEST_FILE "${CMAKE_BINARY_DIR}/${TARGET_NAME}_nap.json")
    file(WRITE ${MANIFEST_FILE} "${MANIFEST_JSON}")
    
    # Store metadata for packaging
    set(${TARGET_NAME}_MANIFEST_FILE ${MANIFEST_FILE} PARENT_SCOPE)
    set(${TARGET_NAME}_APP_ID ${ARG_ID} PARENT_SCOPE)
    set(${TARGET_NAME}_APP_VERSION ${ARG_VERSION} PARENT_SCOPE)
    
    message(STATUS "Generated nap.json: ${MANIFEST_FILE}")
endfunction()

################################################################################
# nah_nak_manifest() - Generate nak.json from CMake variables
################################################################################
function(nah_nak_manifest TARGET_NAME)
    cmake_parse_arguments(ARG 
        "" 
        "ID;VERSION;LOADER;RESOURCE_ROOT" 
        "LIB_DIRS;ENV_VARS;LOADER_ARGS" 
        ${ARGN}
    )

    # Validate required fields
    if(NOT ARG_ID)
        message(FATAL_ERROR "nah_nak_manifest: ID is required")
    endif()
    if(NOT ARG_VERSION)
        message(FATAL_ERROR "nah_nak_manifest: VERSION is required")
    endif()

    # Build nak.json content
    set(MANIFEST_JSON "{\n")
    string(APPEND MANIFEST_JSON "  \"$schema\": \"https://nah.rtorr.com/schemas/nak.v1.json\",\n")
    string(APPEND MANIFEST_JSON "  \"nak\": {\n")
    string(APPEND MANIFEST_JSON "    \"identity\": {\n")
    string(APPEND MANIFEST_JSON "      \"id\": \"${ARG_ID}\",\n")
    string(APPEND MANIFEST_JSON "      \"version\": \"${ARG_VERSION}\"\n")
    string(APPEND MANIFEST_JSON "    },\n")
    
    # Paths section
    string(APPEND MANIFEST_JSON "    \"paths\": {\n")
    
    if(ARG_LIB_DIRS)
        string(APPEND MANIFEST_JSON "      \"lib_dirs\": [")
        list(LENGTH ARG_LIB_DIRS lib_count)
        math(EXPR lib_last "${lib_count} - 1")
        set(i 0)
        foreach(dir ${ARG_LIB_DIRS})
            string(APPEND MANIFEST_JSON "\"${dir}\"")
            if(i LESS lib_last)
                string(APPEND MANIFEST_JSON ", ")
            endif()
            math(EXPR i "${i} + 1")
        endforeach()
        string(APPEND MANIFEST_JSON "]")
    else()
        string(APPEND MANIFEST_JSON "      \"lib_dirs\": [\"lib\"]")
    endif()

    if(ARG_RESOURCE_ROOT)
        string(APPEND MANIFEST_JSON ",\n      \"resource_root\": \"${ARG_RESOURCE_ROOT}\"")
    endif()

    string(APPEND MANIFEST_JSON "\n    }")

    # Loader (optional)
    if(ARG_LOADER)
        string(APPEND MANIFEST_JSON ",\n    \"loader\": {\n")
        string(APPEND MANIFEST_JSON "      \"exec_path\": \"${ARG_LOADER}\"")
        
        if(ARG_LOADER_ARGS)
            string(APPEND MANIFEST_JSON ",\n      \"args_template\": [")
            list(LENGTH ARG_LOADER_ARGS arg_count)
            math(EXPR arg_last "${arg_count} - 1")
            set(i 0)
            foreach(arg ${ARG_LOADER_ARGS})
                string(APPEND MANIFEST_JSON "\"${arg}\"")
                if(i LESS arg_last)
                    string(APPEND MANIFEST_JSON ", ")
                endif()
                math(EXPR i "${i} + 1")
            endforeach()
            string(APPEND MANIFEST_JSON "]")
        endif()
        
        string(APPEND MANIFEST_JSON "\n    }")
    endif()

    # Environment (optional)
    if(ARG_ENV_VARS)
        string(APPEND MANIFEST_JSON ",\n    \"environment\": {\n")
        list(LENGTH ARG_ENV_VARS env_count)
        math(EXPR env_last "${env_count} - 1")
        set(i 0)
        foreach(env_var ${ARG_ENV_VARS})
            string(REGEX MATCH "^([^=]+)=(.*)$" _ ${env_var})
            set(env_key ${CMAKE_MATCH_1})
            set(env_val ${CMAKE_MATCH_2})
            string(APPEND MANIFEST_JSON "      \"${env_key}\": \"${env_val}\"")
            if(i LESS env_last)
                string(APPEND MANIFEST_JSON ",")
            endif()
            string(APPEND MANIFEST_JSON "\n")
            math(EXPR i "${i} + 1")
        endforeach()
        string(APPEND MANIFEST_JSON "    }")
    endif()

    string(APPEND MANIFEST_JSON "\n  }\n")
    string(APPEND MANIFEST_JSON "}\n")

    # Write nak.json
    set(MANIFEST_FILE "${CMAKE_BINARY_DIR}/${TARGET_NAME}_nak.json")
    file(WRITE ${MANIFEST_FILE} "${MANIFEST_JSON}")
    
    # Store metadata
    set(${TARGET_NAME}_MANIFEST_FILE ${MANIFEST_FILE} PARENT_SCOPE)
    set(${TARGET_NAME}_NAK_ID ${ARG_ID} PARENT_SCOPE)
    set(${TARGET_NAME}_NAK_VERSION ${ARG_VERSION} PARENT_SCOPE)
    
    message(STATUS "Generated nak.json: ${MANIFEST_FILE}")
endfunction()

################################################################################
# nah_package() - Stage and package app or NAK
################################################################################
function(nah_package TARGET_NAME)
    cmake_parse_arguments(ARG 
        "" 
        "ASSETS_DIR;TYPE" 
        "" 
        ${ARGN}
    )

    # Detect type from manifest (package type matches manifest name)
    if(DEFINED ${TARGET_NAME}_APP_ID)
        set(PACKAGE_TYPE "nap")  # Package type = manifest name
        set(PACKAGE_ID ${${TARGET_NAME}_APP_ID})
        set(PACKAGE_VERSION ${${TARGET_NAME}_APP_VERSION})
        set(PACKAGE_EXT ".nap")
    elseif(DEFINED ${TARGET_NAME}_NAK_ID)
        set(PACKAGE_TYPE "nak")  # Package type = manifest name
        set(PACKAGE_ID ${${TARGET_NAME}_NAK_ID})
        set(PACKAGE_VERSION ${${TARGET_NAME}_NAK_VERSION})
        set(PACKAGE_EXT ".nak")
    else()
        message(FATAL_ERROR "nah_package: ${TARGET_NAME} must have manifest (call nah_app_manifest or nah_nak_manifest first)")
    endif()

    set(STAGING_DIR "${CMAKE_BINARY_DIR}/${TARGET_NAME}_staging")
    set(PACKAGE_FILE "${CMAKE_BINARY_DIR}/${PACKAGE_ID}-${PACKAGE_VERSION}${PACKAGE_EXT}")

    # Staging target
    add_custom_target(${TARGET_NAME}_stage
        # Create directories
        COMMAND ${CMAKE_COMMAND} -E make_directory ${STAGING_DIR}/bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${STAGING_DIR}/lib
        COMMAND ${CMAKE_COMMAND} -E make_directory ${STAGING_DIR}/assets
        
        # Copy manifest (nap.json or nak.json - matches package extension)
        COMMAND ${CMAKE_COMMAND} -E copy 
            ${${TARGET_NAME}_MANIFEST_FILE}
            ${STAGING_DIR}/${PACKAGE_TYPE}.json
        
        # Copy binary
        COMMAND ${CMAKE_COMMAND} -E copy 
            $<TARGET_FILE:${TARGET_NAME}>
            ${STAGING_DIR}/bin/
        
        DEPENDS ${TARGET_NAME}
        COMMENT "Staging ${PACKAGE_TYPE}: ${PACKAGE_ID}@${PACKAGE_VERSION}"
    )

    # Copy assets if provided
    if(ARG_ASSETS_DIR AND EXISTS "${ARG_ASSETS_DIR}")
        add_custom_command(TARGET ${TARGET_NAME}_stage POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory 
                ${ARG_ASSETS_DIR}
                ${STAGING_DIR}/assets
            COMMENT "Copying assets from ${ARG_ASSETS_DIR}"
        )
    endif()

    # Packaging target
    add_custom_target(${TARGET_NAME}_package
        COMMAND ${CMAKE_COMMAND} -E tar czf ${PACKAGE_FILE} --format=gnutar .
        WORKING_DIRECTORY ${STAGING_DIR}
        DEPENDS ${TARGET_NAME}_stage
        COMMENT "Creating package: ${PACKAGE_FILE}"
        BYPRODUCTS ${PACKAGE_FILE}
    )

    # Convenience: add to global package target
    if(NOT TARGET package_all)
        add_custom_target(package_all)
    endif()
    add_dependencies(package_all ${TARGET_NAME}_package)

    message(STATUS "Package target: ${TARGET_NAME}_package → ${PACKAGE_FILE}")
endfunction()

################################################################################
# Convenience wrapper: nah_app() - Declare + package in one call
################################################################################
function(nah_app TARGET_NAME)
    cmake_parse_arguments(ARG 
        "" 
        "ID;VERSION;NAK;NAK_VERSION;ENTRYPOINT;ASSETS" 
        "" 
        ${ARGN}
    )

    # Generate manifest
    nah_app_manifest(${TARGET_NAME}
        ID ${ARG_ID}
        VERSION ${ARG_VERSION}
        NAK_ID ${ARG_NAK}
        NAK_VERSION ${ARG_NAK_VERSION}
        ENTRYPOINT ${ARG_ENTRYPOINT}
    )

    # Package
    nah_package(${TARGET_NAME}
        ASSETS_DIR ${ARG_ASSETS}
    )
endfunction()

################################################################################
# Convenience wrapper: nah_nak() - Declare + package in one call
################################################################################
function(nah_nak TARGET_NAME)
    cmake_parse_arguments(ARG 
        "" 
        "ID;VERSION;LOADER" 
        "LOADER_ARGS" 
        ${ARGN}
    )

    # Generate manifest
    nah_nak_manifest(${TARGET_NAME}
        ID ${ARG_ID}
        VERSION ${ARG_VERSION}
        LOADER ${ARG_LOADER}
        LOADER_ARGS ${ARG_LOADER_ARGS}
    )

    # Package
    nah_package(${TARGET_NAME})
endfunction()
```

### Example Usage

**Before (current complex approach):**

```cmake
cmake_minimum_required(VERSION 3.14)
project(MyApp)

add_executable(my_app src/main.cpp)

# Manually write JSON string
set(MANIFEST_JSON "{
  \"app\": {
    \"identity\": {
      \"id\": \"com.example.myapp\",
      \"version\": \"1.0.0\",
      \"nak_id\": \"com.example.sdk\",
      \"nak_version_req\": \"^1.0.0\"
    },
    \"execution\": {
      \"entrypoint\": \"bin/my_app\"
    },
    \"layout\": {
      \"lib_dirs\": [\"lib\"],
      \"asset_dirs\": [\"assets\"]
    }
  }
}")

# Generate binary manifest (requires nah CLI)
nah_generate_manifest(my_app "${MANIFEST_JSON}")

# Package
nah_package_nap(my_app "com.example.myapp" "1.0.0"
    ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
```

**After (new simple approach):**

```cmake
cmake_minimum_required(VERSION 3.14)
project(MyApp)

include(cmake/NahAppTemplate.cmake)

add_executable(my_app src/main.cpp)

# Option 1: Explicit manifest + package
nah_app_manifest(my_app
    ID "com.example.myapp"
    VERSION "1.0.0"
    NAK_ID "com.example.sdk"
    NAK_VERSION "^1.0.0"
    ENTRYPOINT "bin/my_app"
)

nah_package(my_app
    ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# Option 2: Shorthand (even simpler)
nah_app(my_app
    ID "com.example.myapp"
    VERSION "1.0.0"
    NAK "com.example.sdk"
    NAK_VERSION "^1.0.0"
    ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# Build and package
# cmake --build . --target my_app_package
```

**For NAKs:**

```cmake
add_library(my_sdk SHARED src/sdk.cpp)

nah_nak_manifest(my_sdk
    ID "com.example.sdk"
    VERSION "1.0.0"
    LOADER "bin/sdk-loader"
    LOADER_ARGS 
        "--app-root" "{NAH_APP_ROOT}"
        "--sdk-root" "{NAH_NAK_ROOT}"
)

nah_package(my_sdk)

# Or shorthand:
nah_nak(my_sdk
    ID "com.example.sdk"
    VERSION "1.0.0"
    LOADER "bin/sdk-loader"
)
```

### What We Keep

✅ **Full CMake integration** - Apps and NAKs can be built/packaged from CMake\
✅ **Convenience functions** - Easy API for manifest generation\
✅ **Automatic packaging** - Single target to build `.nap`/`.nak` files\
✅ **Project metadata reuse** - Pull from CMake variables (`PROJECT_VERSION`, etc.)

### What We Remove

❌ `nah manifest generate` CLI command (no longer needed)\
❌ Binary manifest generation step\
❌ Build-time CLI dependency\
❌ Intermediate `.nah` files

### What We Improve

✨ **No CLI dependency** - Pure CMake, works offline\
✨ **Type-safe API** - CMake validates at configure time\
✨ **Cleaner code** - Structured arguments instead of JSON strings\
✨ **Direct generation** - CMake writes JSON directly\
✨ **Better errors** - CMake can catch issues early

### Migration for CMake Users

**Old CMakeLists.txt:**

1. Replace `nah_generate_manifest()` with `nah_app_manifest()` or `nah_nak_manifest()`
2. Replace `nah_package_nap()` with `nah_package()`
3. Or use shorthand: `nah_app()` / `nah_nak()`
4. Remove binary manifest references
5. Note: Generates `nap.json` (not `app.json`) to match `.nap` extension

**Typical change:**

```diff
- nah_generate_manifest(my_app "${MANIFEST_JSON}")
- nah_package_nap(my_app "com.example.myapp" "1.0.0")
+ nah_app(my_app
+     ID "com.example.myapp"
+     VERSION "1.0.0"
+     ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets"
+ )
```

**Result: Simpler CMakeLists.txt, no external dependencies, same functionality.**

### Files to CREATE

**1. JSON Schema Files (Highly Recommended)**

Create schema files for IDE validation and autocomplete.

**Location:** `docs/schemas/` (deployed via GitHub Pages to https://nah.rtorr.com/schemas/)

**Files to create:**

* `docs/schemas/nap.v1.json` - JSON Schema for `nap.json` (app manifests)
* `docs/schemas/nak.v1.json` - JSON Schema for `nak.json` (NAK manifests)
* `docs/schemas/nah.v1.json` - JSON Schema for `nah.json` (host configuration)
* `docs/schemas/app-record.v1.json` - JSON Schema for app install records
* `docs/schemas/nak-record.v1.json` - JSON Schema for NAK install records

**Deployment:** GitHub Pages deploys `docs/` automatically, making schemas available at:

* `https://nah.rtorr.com/schemas/nap.v1.json`
* `https://nah.rtorr.com/schemas/nak.v1.json`
* `https://nah.rtorr.com/schemas/nah.v1.json`

**Benefits:**

* IDE autocomplete and validation (VSCode, IntelliJ, etc.)
* Real-time error checking while editing manifests
* Self-documenting format with `$schema` references
* Automatic deployment with each release

**Impact:** ~500 lines total (straightforward JSON Schema, can be generated from examples).

**2. Example `nap.json` files**

For each example app, create a `nap.json`:

* `examples/apps/app/nap.json`
* `examples/apps/app_c/nap.json`
* `examples/apps/game-app/nap.json`
* `examples/apps/script-app/nap.json`

**Impact:** +4 files, ~30 lines each = ~120 lines total.

**3. Example `nak.json` files**

Move and restructure:

* `examples/sdk/nak.json` (was `META/nak.json.in`)
* `examples/conan-sdk/nak.json` (was `META/nak.json.in`)

**Impact:** Restructure existing files.

***

## Summary of Changes

### Code Deletion

* `tools/nah/commands/manifest.cpp` - **DELETE** (215 lines)
* Binary TLV parsing in `install.cpp` - **DELETE** (90 lines)
* Embedded manifest examples - **DELETE** (50 lines)
* CMake manifest generation - **DELETE** (40 lines)

**Total deleted: ~400 lines**

### Code Addition

* JSON manifest detection in `install.cpp` - **ADD** (40 lines)
* System tar integration in `pack.cpp` - **ADD** (30 lines)
* JSON schemas (optional) - **ADD** (~500 lines, one-time)

**Total added: ~70 lines core + 500 lines schemas (optional)**

### Net Result

**Core implementation: -330 lines (deletion > addition)**

This is a **massive simplification** that removes custom binary format code and replaces it with standard tools.

### Migration Path

**THERE IS NO MIGRATION. THIS IS A BREAKING CHANGE IN v1.1.0.**

This is the new format, period. No backward compatibility, no phased rollout.

**Why this is acceptable:**

* NAH is at v1.0.25 (early adoption stage)
* User base is tiny or non-existent
* Clean slate is better than years of technical debt
* The new format is dramatically simpler

**What users must do:**

1. Rewrite manifests from embedded/binary to JSON
2. Rename manifest files: `app.json` → `nap.json`, `host.json` → `nah.json`
3. Update CMakeLists.txt (simpler than before)
4. Rebuild packages with new `nah pack`
5. Old packages will NOT work

**Documentation will include:**

* Clear "BREAKING CHANGE" notice in v1.1.0 release notes
* Migration guide (manual rewrite examples)
* Side-by-side comparison (old → new)
* Updated examples for all formats

### Implementation: All in v1.1.0

**Timeline: 2-3 weeks total**

**Tasks:**

1. **Core changes** (Week 1)
   * Delete `tools/nah/commands/manifest.cpp` entirely
   * Delete binary manifest parsing from `install.cpp` (lines 230-320)
   * Implement JSON-only detection in `install.cpp`
   * Implement tar-based `pack.cpp` (was stubbed)
   * Merge or delete `host.cpp` into `install.cpp`
   * Test: `nah install`, `nah pack`, `nah list`

2. **Examples & templates** (Week 2)
   * Update `examples/cmake/NahAppTemplate.cmake` with new functions
   * Create `nap.json` for all example apps (matches `.nap` extension)
   * Verify `nak.json` for all example NAKs (already correct)
   * Rename `host.json` → `nah.json` in host examples
   * Delete embedded manifest code from `app_c`
   * Update all example CMakeLists.txt
   * Test: Build all examples

3. **Documentation** (Week 2-3)
   * Remove binary format section from SPEC.md (lines 1527-1567)
   * Update all docs: concepts, CLI, getting-started, troubleshooting
   * Create JSON schemas (optional but valuable)
   * Write v1.1.0 release notes with migration guide
   * Update README with new examples
   * Update CHANGELOG.md

**Ship v1.1.0 with all changes at once. No intermediate versions.**

***

## Technical Details

### Archive Format

**Structure of `.nap` file:**

```
myapp-1.0.0.nap (gzipped tar archive)
│
├── app.json              # Manifest at root
├── bin/
│   └── myapp             # Executable
├── lib/
│   └── libhelper.so      # Optional libraries
└── assets/
    ├── config.json
    └── icon.png
```

**Structure of `.nak` file:**

```
sdk-1.2.3.nak (gzipped tar archive)
│
├── nak.json              # Manifest at root
├── bin/
│   └── loader            # NAK loader
├── lib/
│   └── libsdk.so         # NAK library
└── resources/
    └── defaults.json
```

**File permissions:**

* Preserved by tar
* Executables keep +x bit
* No special handling needed

### Compression

**Use gzip by default:**

* Good compression ratio
* Fast decompression
* Universal support

**Could support other formats later:**

* `.nap.xz` (better compression, slower)
* `.nap.zst` (balanced)

**Keep it simple for v1:** Just gzip.

### Manifest Detection Algorithm

```python
def detect_manifest_type(directory):
    """
    Returns ('app', 'nak', 'host', or None).
    """
    if exists(join(directory, 'app.json')):
        return 'app'
    elif exists(join(directory, 'nak.json')):
        return 'nak'
    elif exists(join(directory, 'host.json')):
        return 'host'
    else:
        return None
```

### Package Integrity

**For signed packages:**

* Include `signature.json` at root
* Verify before extraction

**Structure:**

```
myapp-1.0.0.nap
├── app.json
├── signature.json        # Signature over tar contents
└── ... (app files)
```

**Signature verification:**

```bash
# Extract signature
tar -xzf myapp.nap signature.json

# Verify signature against tar contents
nah verify myapp.nap
```

***

## Why This Is The Best Design

### 1. Cognitive Load: Lowest Possible

**Role-specific files:**

* App developer sees: `app.json`
* NAK vendor sees: `nak.json`
* Sysadmin sees: `host.json`

**No confusion:**

* Filename documents purpose
* No type discriminator to remember
* Focused documentation per role

### 2. CLI Simplicity: Maximum

**One command:**

```bash
nah install <anything>    # Just works
```

**Auto-detection is trivial:**

* Check file extension (.nap, .nak)
* Check for manifest file (app.json, nak.json, host.json)
* Do the right thing

### 3. Implementation Simplicity: Extreme

**No custom binary format:**

* ❌ No TLV writer (200+ lines deleted)
* ❌ No TLV reader (200+ lines deleted)
* ❌ No CRC calculation
* ❌ No binary format versioning
* ✅ Just use tar

**Standard tooling:**

* Every language has tar libraries
* Every OS has tar command
* Compression handled by gzip
* Zero custom code

### 4. Developer Experience: Excellent

**Manual packaging:**

```bash
# Developer can create package by hand
cd my-app/
tar -czf ../myapp-1.0.0.nap .
```

**Inspection:**

```bash
# Anyone can inspect a package
tar -tzf myapp.nap              # List contents
tar -xzf myapp.nap app.json     # Extract manifest
cat app.json | jq .             # Read manifest
```

**Debugging:**

```bash
# Extract and examine
tar -xzf myapp.nap -C /tmp/debug/
cd /tmp/debug/
# Poke around, test, debug
```

### 5. CI/CD Integration: Seamless

**Standard tools work:**

```yaml
# GitHub Actions
- name: Inspect package
  run: |
    tar -tzf myapp.nap
    tar -xzf myapp.nap app.json
    jq '.app.identity.version' app.json
```

**No special NAH tools needed for basic operations.**

### 6. Cross-Platform: Guaranteed

**Tar works everywhere:**

* Linux ✅
* macOS ✅
* Windows ✅ (via WSL, Git Bash, or native tar)
* BSD ✅

**Gzip is universal.**

***

## Comparison Matrix

| Aspect | Custom TLV Format | Standard tar.gz |
|--------|-------------------|-----------------|
| **Implementation** | 400+ lines custom code | ~50 lines (wrapper) |
| **Debugging** | Need custom tools | Standard Unix tools |
| **Manual creation** | Nearly impossible | `tar -czf pkg.nap .` |
| **Inspection** | Custom reader needed | `tar -tzf pkg.nap` |
| **CI/CD integration** | Requires NAH tools | Standard scripting |
| **Cross-platform** | Must port custom code | Tar everywhere |
| **File permissions** | Must handle explicitly | Preserved by tar |
| **Compression** | Must implement | gzip (standard) |
| **Incremental extraction** | Can optimize | Standard tar |
| **Security (signing)** | Custom implementation | Standard tools |
| **Embedding in binaries** | Natural fit | String literal or separate format |
| **Runtime performance** | N/A (not used at runtime) | N/A (not used at runtime) |
| **Install time performance** | Negligible difference | Negligible difference |
| **Package size** | Slightly smaller | Slightly larger (acceptable) |

**Winner:** tar.gz on almost every metric that matters.

***

## Migration From Current Design

***

## Recommendation

✅ **ADOPT THIS DESIGN IN v1.1.0 (BREAKING CHANGE)**

### Why This Is The Right Decision

1. **Cognitive load: Minimal** - Role-specific files, standard formats
2. **CLI simplicity: Maximum** - One install command, auto-detects
3. **Implementation: Simplest possible** - Delete custom code, use tar
4. **Developer experience: Excellent** - Manual packaging, easy debugging
5. **CI/CD: Seamless** - Standard tools everywhere
6. **Cross-platform: Guaranteed** - Tar works everywhere
7. **Future-proof: Yes** - Can add features without changing core format

### What We Trade

**Lost:**

* Custom binary format mystique
* Embedded manifest capability (was a demo feature)

**Gained:**

* 90% less implementation code
* Standard tooling everywhere
* Human-friendly workflows
* Easier debugging
* Simpler mental model
* No build-time CLI dependency for CMake users

### The Bottom Line

This design achieves the **absolute minimum complexity** while meeting all requirements:

* ✅ Packages bundle files
* ✅ Manifests declare requirements
* ✅ CLI auto-detects everything
* ✅ Works cross-platform
* ✅ Developer-friendly
* ✅ Debuggable
* ✅ CI/CD friendly
* ✅ Standard tooling
* ✅ CMake integration improved

**There is no simpler design that meets these requirements.**

NAH becomes:

* Self-documenting (role-specific files)
* Implementation-simple (standard archives)
* User-friendly (one install command)
* Debuggable (standard tools)

**This is the principled, minimal design. Ship it in v1.1.0 as a clean break.**
