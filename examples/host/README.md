# Example NAH Host

A standalone host configuration demonstrating NAH integration with multiple NAKs and applications.

## Overview

This host requires:

| Type | ID | Version | Description |
|------|----|---------|-------------|
| NAK | `com.example.sdk` | 1.2.3 | Framework SDK for C/C++ apps |
| NAK | `com.example.gameengine` | 1.0.0 | Game engine with graphics/network/crypto |
| App | `com.example.app` | 1.0.0 | Basic C app (uses sdk) |
| App | `com.example.app_c` | 1.0.0 | C++ app with embedded manifest (uses sdk) |
| App | `com.example.mygame` | 1.0.0 | Game app (uses gameengine) |

## Usage

```bash
# Install pre-built packages into NAH root
./setup.sh

# Run all apps
./run.sh

# Show launch contracts
./run.sh --contract

# Run specific app
./run.sh com.example.app
```

> **Note:** Packages must be built before running setup.
> Use `../scripts/build_all.sh` to build all NAKs and apps first.

## Structure

```
host/
├── host.toml           # Host manifest - declares required packages
├── setup.sh            # Install packages into NAH root
├── run.sh              # Run apps or show contracts
├── profiles/           # Host profiles (policy/bindings)
│   ├── default.toml
│   ├── production.toml
│   └── canary.toml
├── src/                # Host integration examples (C++)
└── nah_root/           # Generated NAH root (after setup)
```

## Host Manifest (host.toml)

Declares which packages to install:

```toml
[host]
name = "example-host"
nah_root = "./nah_root"
default_profile = "default"

[[naks]]
id = "com.example.sdk"
version = "1.2.3"
package = "../sdk/build/com.example.sdk-1.2.3.nak"

[[apps]]
id = "com.example.app"
version = "1.0.0"
package = "../apps/app/build/com.example.app-1.0.0.nap"
nak = "com.example.sdk"
```

## Profiles

Profiles control NAK selection and policy:

| Profile | Mode | Description |
|---------|------|-------------|
| default.toml | canonical | Use highest compatible NAK version |
| production.toml | mapped | Pin specific NAK versions |
| canary.toml | canonical | Testing with relaxed warnings |

Switch profiles:
```bash
./run.sh --profile production
```

## Host API Integration

The `src/` directory contains C++ examples:

```cpp
#include <nah/nahhost.hpp>

auto host = nah::NahHost::create("./nah_root");
auto apps = host->listApplications();
auto contract = host->getLaunchContract("com.example.app", "1.0.0");
```

Build:
```bash
mkdir build && cd build
cmake .. && make
./host_api_demo ../nah_root
```
