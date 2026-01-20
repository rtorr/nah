# NAH Examples

Working examples demonstrating NAH applications, SDKs, and host integration.

## Structure

```
examples/
├── scripts/                # Build, setup, and run scripts
│   ├── common.sh          # Shared functions (internal)
│   ├── build_all.sh       # Build all NAKs and apps
│   ├── setup_host.sh      # Install NAKs and apps into demo host
│   ├── run_apps.sh        # Run apps via NAH
│   └── clean_all.sh       # Clean build artifacts
├── cmake/
│   └── NahAppTemplate.cmake  # Shared CMake patterns for apps
├── sdk/                    # Framework SDK (NAK)
├── conan-sdk/              # Game Engine SDK (NAK with Conan deps)
├── apps/
│   ├── app/               # C app with standalone manifest
│   ├── app_c/             # C++ app with embedded manifest
│   └── game-app/          # Game targeting conan-sdk NAK
├── host/
│   └── host.json          # Host environment configuration
└── demo_nah_root/         # Generated demo NAH root (after setup)
```

## Quick Start

```bash
cd examples

# Build everything (NAKs + apps)
./scripts/build_all.sh

# Set up a demo NAH host
./scripts/setup_host.sh

# List installed packages
nah --root demo_nah_root list

# Check an app's status
nah --root demo_nah_root show com.example.app

# Run apps via NAH
./scripts/run_apps.sh

# Or just show launch contracts
./scripts/run_apps.sh --contract
```

## Using the New CLI

The NAH CLI auto-detects package types and simplifies common operations:

```bash
# Install a package (auto-detects .nap/.nak)
nah install sdk/build/com.example.sdk-1.2.3.nak
nah install apps/app/build/com.example.app-1.0.0.nap

# List everything
nah list                    # Shows both apps and NAKs
nah list --apps             # Apps only
nah list --naks             # NAKs only

# Check status and diagnose issues
nah status                  # Overview of root
nah status com.example.app  # App contract details
nah status com.example.app --trace  # With provenance

# Create a new project
nah init app ./my-new-app
nah init nak ./my-new-sdk
nah init root ./my-nah-root

# Pack a directory (auto-detects type)
nah pack ./my-new-app -o my-app.nap
```

## Scripts

| Script | Purpose |
|--------|---------|
| `build_all.sh` | Builds all NAKs and apps |
| `setup_host.sh` | Creates demo_nah_root with NAKs and apps installed |
| `run_apps.sh` | Runs installed apps or shows their contracts |
| `clean_all.sh` | Removes all build artifacts |

Options:
- `--clean` - Clean before building/setup
- `--root <path>` - Use different NAH root
- `SKIP_CONAN=1` - Skip conan-sdk build

## Examples Overview

### NAKs (Native App Kits)

**sdk/** - Simple NAK built with CMake only
```bash
cd sdk && mkdir build && cd build
cmake .. && make && make package_nak
# Output: com.example.sdk-1.2.3.nak
```

**conan-sdk/** - NAK with Conan-managed dependencies (zlib, openssl, curl, spdlog)
```bash
cd conan-sdk
conan install . --output-folder=build --build=missing \
    --deployer=full_deploy --deployer-folder=build/deploy
cmake --preset conan-release
cmake --build build/build/Release --target package_nak
# Output: com.example.gameengine-1.0.0.nak
```

### Apps (Native App Packages)

**app/** - Basic C application with standalone manifest
- Demonstrates: SDK usage, resource loading, NAH environment detection
- Uses: `NahAppTemplate.cmake` for common build patterns

**app_c/** - C++ application with embedded manifest
- Demonstrates: Manifest embedded in binary section
- Unique: Single-file distribution without separate manifest.nah

**game-app/** - Game targeting the conan-sdk NAK
- Demonstrates: Complex SDK with many dependencies
- Requires: conan-sdk to be built first

## Two Manifest Approaches

### 1. Standalone Manifest (app/)

Manifest is a separate `manifest.nah` file:

```cmake
nah_generate_manifest(myapp "${MANIFEST_CONTENT}")
nah_package_nap(myapp ${APP_ID} ${APP_VERSION})
```

### 2. Embedded Manifest (app_c/)

Manifest is embedded in binary section:

```cpp
NAH_MANIFEST_SECTION
static const uint8_t manifest[] = { ... };
```

## Version Requirements

Apps declare NAK version requirements using [SemVer 2.0.0](https://semver.org/spec/v2.0.0.html) range syntax:

| Format | Example | Meaning |
|--------|---------|---------|
| Exact | `1.2.0` | Exactly version 1.2.0 |
| Greater/Equal | `>=1.2.0` | Version 1.2.0 or higher |
| Range | `>=1.2.0 <2.0.0` | Version 1.2.0 up to (not including) 2.0.0 |
| OR | `>=1.0.0 <2.0.0 \|\| >=3.0.0` | 1.x versions or 3.0.0+ |

Comparators: `=`, `<`, `<=`, `>`, `>=`. Space-separated comparators are AND'd together. `||` separates OR alternatives.

## Building in Isolation

Apps and NAKs can be built independently:

```bash
# App built in isolation (production)
cmake .. \
    -DNAH_CLI=/usr/local/bin/nah \
    -Dframework_SDK_INCLUDE_DIR=/path/to/sdk/include \
    -Dframework_SDK_LIB_DIR=/path/to/sdk/lib

# Within examples tree (auto-detects paths)
cmake ..
```

## Multi-NAK Host

Hosts can have multiple NAKs installed. Each app targets a specific NAK:

```
demo_nah_root/
├── host/
│   └── host.json                    # Host environment
├── naks/
│   ├── com.example.sdk/1.2.3/
│   └── com.example.gameengine/1.0.0/
├── apps/
│   ├── com.example.app-1.0.0/       # Uses sdk
│   └── com.example.mygame-1.0.0/    # Uses gameengine
└── registry/
    ├── apps/
    │   └── com.example.app@1.0.0.json
    └── naks/
        └── com.example.sdk@1.2.3.json
```

## Troubleshooting

**NAH CLI not found**: Install NAH or set `NAH_CLI` environment variable.

**Build errors**: Ensure NAH is built first (`cd .. && cmake --build build`).

**Conan errors**: Install Conan 2.x (`pip install conan`).
