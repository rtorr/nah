# Example NAH Host

A standalone host configuration demonstrating NAH integration with multiple NAKs and applications.

## Overview

This host installs:

| Type | ID | Version | Description |
|------|----|---------|-------------|
| NAK | `com.example.sdk` | 1.2.3 | Framework SDK for C/C++ apps |
| App | `com.example.app` | 1.0.0 | Basic C app (uses sdk) |
| App | `com.example.app_c` | 1.0.0 | C++ app with embedded manifest (uses sdk) |

## Usage

```bash
# Install packages into NAH root using nah host install
./setup.sh

# Or directly with the nah CLI:
nah host install .

# Clean install (removes existing NAH root first)
nah host install . --clean

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
├── host.json           # Host environment configuration
├── nah.json            # Host manifest - declares required packages (if using nah host install)
├── setup.sh            # Wrapper for nah host install
├── run.sh              # Run apps or show contracts
├── src/                # Host integration examples (C++)
└── nah_root/           # Generated NAH root (after setup)
```

## Host Environment (host.json)

Configures host-specific environment variables and paths:

```json
{
  "environment": {
    "NAH_HOST_NAME": "example-host"
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

## Host Manifest (nah.json)

Declares which packages to install:

```json
{
  "$schema": "nah.host.manifest.v1",
  "root": "./nah_root",
  "host": {
    "environment": {
      "NAH_HOST_NAME": "example-host"
    }
  },
  "install": [
    "../sdk/build/com.example.sdk-1.2.3.nak",
    "../apps/app/build/com.example.app-1.0.0.nap",
    "../apps/app_c/build/com.example.app_c-1.0.0.nap"
  ]
}
```

Package paths are relative to the host directory. Packages are self-describing (id/version extracted from the package itself).

## Host API Integration

The `src/` directory contains both C and C++ examples.

### C API (Stable ABI)

For hosts using C, FFI, or requiring ABI stability:

```c
#include <nah/nah.h>

/* Check ABI compatibility */
if (nah_abi_version() != NAH_ABI_VERSION) {
    fprintf(stderr, "ABI mismatch\n");
    return 1;
}

NahHost* host = nah_host_create("./nah_root");
NahContract* contract = nah_host_get_contract(host, "com.example.app", NULL);

printf("Binary: %s\n", nah_contract_binary(contract));
printf("CWD: %s\n", nah_contract_cwd(contract));

/* Environment as JSON (caller must free) */
char* env = nah_contract_environment_json(contract);
printf("Env: %s\n", env);
nah_free_string(env);

nah_contract_destroy(contract);
nah_host_destroy(host);
```

### C++ API

For hosts using C++:

```cpp
#include <nah/nahhost.hpp>

auto host = nah::NahHost::create("./nah_root");
auto apps = host->listApplications();
auto contract = host->getLaunchContract("com.example.app", "1.0.0");
```

### Build

```bash
mkdir build && cd build
cmake .. && make

# C++ demo
./host_api_demo ../nah_root

# C demo
./host_c_api_demo ../nah_root
```
