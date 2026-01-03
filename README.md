# NAH - Native Application Host

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

| Term | Description |
|------|-------------|
| **NAK** | Native App Kit - versioned SDK/framework bundle that apps target at launch |
| **NAP** | Native App Package - application package targeting a specific NAK |
| **App Manifest** | Immutable declaration embedded in app binary or as `manifest.nah` |
| **Launch Contract** | Final executable contract: binary, argv, cwd, environment, library paths |

## Installation

### Build from Source

```bash
mkdir build && cd build
cmake ..
cmake --build .
sudo cmake --install .
```

### Requirements

- CMake 3.20+
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- Optional: Conan 2.x for examples with dependencies

## CLI Usage

```bash
# Show launch contract for an installed app
nah contract show --root /nah --app com.example.myapp

# Install a NAK
nah nak install --root /nah mynak.nak

# Install an app
nah app install --root /nah myapp.nap

# Launch an app
nah app launch --root /nah com.example.myapp

# Validate configuration
nah validate --root /nah
```

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
    NAH_FIELD_NAK_VERSION_REQ("^1.0.0")
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

## Specification

See [SPEC.md](SPEC.md) for the complete normative specification.

## License

MIT
