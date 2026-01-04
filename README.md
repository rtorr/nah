# NAH - Native Application Host

[![CI](https://github.com/rtorr/nah/actions/workflows/ci.yml/badge.svg)](https://github.com/rtorr/nah/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-API-blue)](https://rtorr.github.io/nah/)

NAH standardizes how native applications are installed, inspected, and launched. It provides a deterministic contract between applications and hosts, ensuring portable app binaries while giving hosts full control over policy, layout, and enforcement.

## The Problem

Native platforms fail when launch behavior becomes an emergent property of scattered scripts, ad-hoc environment assumptions, and host-specific glue:

- Applications tied to specific filesystem layouts
- Runtime dependencies drift across machines
- Trust/provenance handled inconsistently
- Launch behavior cannot be audited
- Drift between what developers expect and what actually launches

## The Solution

NAH fixes this by making launch behavior a deterministic composition:

- **Applications** provide an immutable, host-agnostic declaration of intent and requirements
- **Hosts** provide mutable policy, bindings, and per-install state
- **Composition** produces one concrete result: a Launch Contract that can be executed and audited

## Key Concepts

| Term                | Description                                                                |
| ------------------- | -------------------------------------------------------------------------- |
| **NAK**             | Native App Kit - versioned SDK/framework bundle that apps target at launch |
| **NAP**             | Native App Package - application package targeting a specific NAK          |
| **App Manifest**    | Immutable declaration embedded in app binary or as `manifest.nah`          |
| **Launch Contract** | Final executable contract: binary, argv, cwd, environment, library paths   |

## Installation

### Pre-built Binaries

Download from [GitHub Releases](https://github.com/rtorr/nah/releases):

```bash
# Linux
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-linux-x64.tar.gz | tar xz
sudo mv nah /usr/local/bin/

# macOS (Apple Silicon)
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-macos-arm64.tar.gz | tar xz
sudo mv nah /usr/local/bin/

# macOS (Intel)
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-macos-x64.tar.gz | tar xz
sudo mv nah /usr/local/bin/
```

### Build from Source

```bash
git clone https://github.com/rtorr/nah.git
cd nah
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### Requirements

- CMake 3.21+
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- Optional: Ninja for faster builds
- Optional: Conan 2.x for examples with dependencies

## CLI Usage

```bash
# Initialize a new NAH root
nah profile init ./my-nah-root

# Install a NAK (SDK/framework)
nah --root ./my-nah-root nak install sdk-1.0.0.nak

# Install an application
nah --root ./my-nah-root app install myapp-1.0.0.nap

# List installed apps
nah --root ./my-nah-root app list

# Show launch contract for an app
nah --root ./my-nah-root contract show com.example.myapp

# Get contract as JSON (for scripting)
nah --root ./my-nah-root --json contract show com.example.myapp

# Diagnose issues with an app
nah --root ./my-nah-root doctor com.example.myapp

# Validate a configuration file
nah validate profile host-profile.toml
```

Run `nah --help` or `nah <command> --help` for full usage information. See [CLI Reference](docs/cli.md) for complete documentation.

## Using NAH as a Library

NAH can be integrated into your C++ project as a library for programmatic contract composition.

### CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    nah
    GIT_REPOSITORY https://github.com/rtorr/nah.git
    GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(nah)

target_link_libraries(your_target PRIVATE nahhost)
```

### Conan 2

Add to your `conanfile.txt`:

```ini
[requires]
nah/1.0.0

[generators]
CMakeDeps
CMakeToolchain
```

Or in `conanfile.py`:

```python
def requirements(self):
    self.requires("nah/1.0.0")
```

Then in CMake:

```cmake
find_package(nah REQUIRED)
target_link_libraries(your_target PRIVATE nah::nahhost)
```

Build with:

```bash
conan install . --build=missing
cmake -B build -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake
cmake --build build
```

### Library Components

| Target           | Description                              |
| ---------------- | ---------------------------------------- |
| `nahhost`        | Main library - contract composition      |
| `nah_manifest`   | Manifest parsing and TLV encoding        |
| `nah_contract`   | Contract composition and NAK selection   |
| `nah_config`     | Host profile and registry loading        |
| `nah_packaging`  | NAP/NAK package handling                 |
| `nah_platform`   | Platform abstraction (paths, I/O)        |

### Example Usage

```cpp
#include <nah/nahhost.hpp>
#include <nah/manifest.hpp>
#include <nah/semver.hpp>

// Parse a semantic version
auto version = nah::parse_version("1.2.3");

// Parse a version range (SemVer 2.0.0 syntax)
auto range = nah::parse_range(">=1.0.0 <2.0.0");

// Check if version satisfies range
bool ok = nah::satisfies(version, range);
```

## Examples

The `examples/` directory contains working demonstrations:

```bash
cd examples

# Build all NAKs and apps
./scripts/build_all.sh

# Set up a demo host
./scripts/setup_host.sh

# Run apps
./scripts/run_apps.sh
```

See [examples/README.md](examples/README.md) for details.

## Reference

### On-Disk Layout

```
/nah/
├── apps/
│   └── <id>-<version>/          # Installed app payloads
├── naks/
│   └── <nak_id>/<version>/      # Installed NAKs
├── host/
│   ├── profiles/
│   │   └── default.toml         # Host profile (policy/bindings)
│   └── profile.current          # Symlink to active profile
└── registry/
    ├── installs/                # App install records
    └── naks/                    # NAK install records
```

### App Manifest

Applications declare their requirements in a TLV binary manifest:

```cpp
#include <nah/manifest.h>

NAH_APP_MANIFEST(
    NAH_FIELD_ID("com.example.myapp")
    NAH_FIELD_VERSION("1.0.0")
    NAH_FIELD_NAK_ID("com.example.sdk")
    NAH_FIELD_NAK_VERSION_REQ(">=1.0.0 <2.0.0")
    NAH_FIELD_ENTRYPOINT("bin/myapp")
    NAH_FIELD_LIB_DIR("lib")
)
```

### Host Profile

Hosts control NAK selection, warning policy, and capability mapping:

```toml
schema = "nah.host.profile.v1"

[nak]
binding_mode = "canonical"
allow_versions = ["1.*", "2.*"]

[environment]
NAH_HOST_VERSION = "1.0"

[warnings]
nak_not_found = "error"
trust_state_failed = "error"

[capabilities]
"filesystem.read" = "sandbox.fs.readonly"
```

## Design Principles

1. **Minimal mechanism** - NAH composes and reports; hosts enforce
2. **Deterministic binding** - Selection occurs at install time and is pinned
3. **Auditable contracts** - Launch behavior is data, not scattered glue
4. **Portable payloads** - No host paths embedded in app declarations
5. **Policy evolves without rebuilds** - Hosts update bindings independently

## Development

### Building

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Running Tests

```bash
ctest --test-dir build --output-on-failure
```

### CMake Options

| Option                  | Default | Description                     |
| ----------------------- | ------- | ------------------------------- |
| `NAH_ENABLE_TESTS`      | ON      | Build test suite                |
| `NAH_ENABLE_WARNINGS`   | ON      | Enable strict compiler warnings |
| `NAH_ENABLE_SANITIZERS` | OFF     | Enable ASan/UBSan (GCC/Clang)   |

### Creating a Release

Use the release script to bump versions and create a tag:

```bash
./scripts/release.sh 1.1.0
```

This updates the VERSION file, commits, tags, and pushes. GitHub Actions then:

1. Builds binaries for Linux (x64), macOS (x64, ARM64), and Windows (x64)
2. Creates a GitHub Release with the binaries
3. Auto-generates release notes from commits

## Documentation

| Resource | Description |
|----------|-------------|
| [CLI Reference](docs/cli.md) | Complete command-line reference |
| [API Reference](https://rtorr.github.io/nah/api/) | Library documentation (Doxygen) |
| [Getting Started: App](docs/getting-started-app.md) | Build apps for NAH |
| [Getting Started: NAK](docs/getting-started-nak.md) | Build SDKs/frameworks |
| [Getting Started: Host](docs/getting-started-host.md) | Deploy NAH in production |
| [SPEC.md](SPEC.md) | Complete normative specification |

## License

MIT
