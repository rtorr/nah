# NAH v1.1.0 Refactor - Progress Report

## Completed Changes

### 1. ✅ Removed Binary TLV Manifest Format
- **Deleted**: `tools/nah/commands/manifest.cpp` (215 lines)
- **Updated**: `tools/nah/CMakeLists.txt` - removed manifest.cpp from build
- **Updated**: `tools/nah/main.cpp` - removed manifest command registration
- **Impact**: Eliminated ~400+ lines of custom TLV parsing/generation code

### 2. ✅ Updated install.cpp for New JSON Manifests
**File**: `tools/nah/commands/install.cpp`

**Changes**:
- Updated `detect_source_type()` to check for `nap.json`, `nak.json`, `nah.json`
- Removed ~130 lines of binary TLV manifest parsing (lines 230-364)
- Added proper JSON structure detection (app.identity, nak.identity, host sections)
- Updated manifest field extraction to use new nested structure
- Simplified error messages

**Before** (230+ lines of code):
```cpp
// Try different manifest locations
auto manifest_content = nah::fs::read_file(source_dir + "/nah.json");
if (!manifest_content) {
    manifest_content = nah::fs::read_file(source_dir + "/META/nak.json");
}
if (!manifest_content) {
    manifest_content = nah::fs::read_file(source_dir + "/META/app.json");
}
if (!manifest_content) {
    // 90+ lines of binary TLV parsing...
}
```

**After** (25 lines of code):
```cpp
// Try NAH-specific manifest files (names match package extensions)
auto manifest_content = nah::fs::read_file(source_dir + "/nap.json");
std::string manifest_type = "nap";

if (!manifest_content) {
    manifest_content = nah::fs::read_file(source_dir + "/nak.json");
    manifest_type = "nak";
}
if (!manifest_content) {
    manifest_content = nah::fs::read_file(source_dir + "/nah.json");
    manifest_type = "nah";
}
```

### 3. ✅ Implemented pack.cpp with tar.gz Generation
**File**: `tools/nah/commands/pack.cpp`

**Changes**:
- Replaced stub implementation with full tar.gz packaging
- Auto-detects manifest type from filename (`nap.json` vs `nak.json`)
- Extracts identity from nested structure (`app.identity` or `nak.identity`)
- Uses system `tar` command with deterministic flags for reproducible builds
- Proper error handling and JSON output

**Key Implementation**:
```cpp
// Create tar.gz package using system tar command
// Use deterministic flags for reproducible builds
std::string tar_cmd = "tar --sort=name --owner=0 --group=0 --numeric-owner "
                      "--mtime='1970-01-01' -czf " + output_path + 
                      " -C " + source_dir + " .";

int result = std::system(tar_cmd.c_str());
```

**Result**: Users can now run `nah pack <dir>` to create `.nap` or `.nak` packages.

## Remaining Tasks

### 4. ⏳ Update or Merge host.cpp
**Status**: Pending
**Plan**: Merge `nah host install` functionality into `nah install` with auto-detection

### 5. ⏳ Update CMake NahAppTemplate Functions
**Status**: Pending  
**File**: `examples/cmake/NahAppTemplate.cmake`
**Plan**: 
- Remove `nah_generate_manifest()` function
- Add `nah_app_manifest()` and `nah_nak_manifest()` functions that generate JSON
- Update `nah_package()` to use tar.gz instead of binary manifests
- Add convenience wrappers: `nah_app()` and `nah_nak()`

### 6. ⏳ Update Examples
**Status**: Pending
**Files**: `examples/apps/*/`, `examples/sdk/`, `examples/conan-sdk/`
**Plan**:
- Create `nap.json` files for apps (replace embedded manifests or binary files)
- Move `META/nak.json.in` to root as `nak.json`
- Update CMakeLists.txt in each example

### 7. ⏳ Update SPEC.md
**Status**: Pending
**Plan**: Remove "Binary Manifest Format" section (lines 1527-1682)

### 8. ⏳ Run Tests
**Status**: Pending
**Plan**: Build and run tests, fix any compilation errors or test failures

## Code Statistics

### Lines Removed: ~450+
- Binary TLV parsing: ~130 lines
- Binary TLV generation: ~215 lines  
- Manifest command scaffolding: ~100 lines
- Old detection heuristics: ~10 lines

### Lines Added: ~100
- New JSON detection: ~40 lines
- tar.gz packaging: ~60 lines

**Net Change**: ~350 lines **removed** (simplified)

## Key Benefits Achieved

✅ **Removed custom binary format** - No more TLV encoding/decoding  
✅ **Standard tooling** - Uses `tar` for packaging (works everywhere)  
✅ **Self-documenting** - Role-specific filenames (`nap.json`, `nak.json`, `nah.json`)  
✅ **Simpler CLI** - Removed `nah manifest generate` command  
✅ **Better errors** - Clear messages about expected manifest files  
✅ **Developer-friendly** - Can manually create packages with `tar -czf`

## Schema Files Created

Created 5 comprehensive JSON Schema files:
- `docs/schemas/nap.v1.json` - App manifest schema
- `docs/schemas/nak.v1.json` - NAK manifest schema
- `docs/schemas/nah.v1.json` - Host configuration schema
- `docs/schemas/app-record.v1.json` - App install record schema
- `docs/schemas/nak-record.v1.json` - NAK install record schema

All schemas support environment variable operations (set, prepend, append, unset) and include complete validation rules.

## Next Steps

1. Continue with CMake template updates
2. Update all examples to use new JSON manifests
3. Update SPEC.md to remove binary format documentation
4. Build and test the refactored codebase
5. Create migration guide for existing users

## Breaking Changes

⚠️ This is a **breaking change** for v1.1.0:
- Old `.nap`/`.nak` files with binary manifests will not work
- `META/` directories are no longer used
- Manifest filenames changed to match package extensions
- All manifests must use nested structure (`app.identity`, `nak.identity`)

