# Example NAH Host

A standalone host configuration demonstrating NAH integration with multiple NAKs and applications.

## Overview

This host manages:

| Type | ID | Version | Description |
|------|----|---------|-------------|
| NAK | `com.example.sdk` | 1.2.3 | Framework SDK for C/C++ apps |
| NAK | `com.example.gameengine` | 1.0.0 | Game engine with graphics/network/crypto |
| App | `com.example.app` | 1.0.0 | Basic C app (uses sdk) |
| App | `com.example.app_c` | 1.0.0 | C++ app with embedded manifest (uses sdk) |
| App | `com.example.mygame` | 1.0.0 | Game app (uses gameengine) |

## Quick Start

```bash
# Set up the host (builds and installs NAKs + apps)
./setup.sh

# Run all apps
./run.sh

# Show launch contracts
./run.sh --contract

# Run specific app
./run.sh com.example.app
```

## Structure

```
host/
├── host.toml           # Host manifest - declares NAKs and apps
├── setup.sh            # Build and install dependencies
├── run.sh              # Run apps or show contracts
├── profiles/           # Host profiles (policy/bindings)
│   ├── default.toml    # Development profile
│   ├── production.toml # Mapped NAK bindings
│   └── canary.toml     # Testing profile
├── src/                # Host integration examples (C++)
│   ├── host_api_demo.cpp
│   └── contract_inspector.cpp
└── nah_root/           # Generated NAH root (after setup)
    ├── apps/           # Installed app payloads
    ├── naks/           # Installed NAKs
    ├── host/profiles/  # Active profiles
    └── registry/       # Install records
```

## Host Manifest (host.toml)

The `host.toml` file declares all NAKs and apps this host requires:

```toml
[host]
name = "example-host"
nah_root = "./nah_root"
default_profile = "default"

[[naks]]
id = "com.example.sdk"
version = "1.2.3"
source = "../sdk"

[[apps]]
id = "com.example.app"
version = "1.0.0"
source = "../apps/app"
nak = "com.example.sdk"
```

## Profiles

Profiles control NAK selection and policy:

### default.toml (Development)
```toml
[nak]
binding_mode = "canonical"  # Use highest compatible version
```

### production.toml (Mapped)
```toml
[nak]
binding_mode = "mapped"

[nak.map]
"1.2" = "com.example.sdk@1.2.3.toml"  # Pin specific versions
```

Switch profiles:
```bash
./run.sh --profile production
```

## Host API Integration

The `src/` directory contains examples of using the NAH C++ API:

```cpp
#include <nah/nahhost.hpp>

auto host = nah::NahHost::create("/path/to/nah_root");

// List installed apps
auto apps = host->listApplications();

// Get launch contract
auto result = host->getLaunchContract("com.example.app", "1.0.0");
if (result.isOk()) {
    auto contract = result.value().contract;
    // contract.execution.binary, contract.execution.arguments, etc.
}
```

Build the examples:
```bash
mkdir build && cd build
cmake .. -DNAH_BUILD_DIR=/path/to/nah/build
make
./host_api_demo ./nah_root
```

## Adding New Apps

1. Add entry to `host.toml`:
   ```toml
   [[apps]]
   id = "com.example.newapp"
   version = "1.0.0"
   source = "/path/to/app"
   nak = "com.example.sdk"
   ```

2. Re-run setup:
   ```bash
   ./setup.sh
   ```

## Adding New NAKs

1. Add entry to `host.toml`:
   ```toml
   [[naks]]
   id = "com.example.newnak"
   version = "2.0.0"
   source = "/path/to/nak"
   ```

2. Update profiles if using mapped mode

3. Re-run setup:
   ```bash
   ./setup.sh --clean
   ```

## Troubleshooting

**Setup fails with "NAH CLI not found"**
- Build NAH first: `cd ../.. && cmake --build build`
- Or set `NAH_CLI=/path/to/nah`

**App won't launch (NAK not found)**
- Check the app's NAK is listed in `host.toml`
- Run `./setup.sh --clean` to reinstall

**Conan NAKs skipped**
- Install Conan 2: `pip install conan`
- Or add `optional = true` to skip gracefully
