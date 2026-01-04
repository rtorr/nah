# NAH - Native Application Host

[![CI](https://github.com/rtorr/nah/actions/workflows/ci.yml/badge.svg)](https://github.com/rtorr/nah/actions/workflows/ci.yml)

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

Run `nah --help` or `nah <command> --help` for full usage information.

## On-Disk Layout

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

## App Manifest

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

## Host Profile

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

Releases are automated via GitHub Actions. To create a new release:

```bash
# Create and push a version tag
git tag v1.0.0
git push origin v1.0.0
```

This triggers the release workflow which:

1. Builds binaries for Linux (x64), macOS (x64, ARM64), and Windows (x64)
2. Creates a GitHub Release with the binaries
3. Auto-generates release notes from commits

**Version format:**

- `v1.0.0` - stable release
- `v1.0.0-beta.1` - pre-release (marked as such on GitHub)

## Specification

See [SPEC.md](SPEC.md) for the complete normative specification.

## License

MIT
