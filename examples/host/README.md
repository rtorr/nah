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
├── host.json           # Host manifest - declares required packages
├── setup.sh            # Install packages into NAH root
├── run.sh              # Run apps or show contracts
├── profiles/           # Host profiles (policy/bindings)
│   ├── default.json
│   ├── production.json
│   └── canary.json
├── src/                # Host integration examples (C++)
└── nah_root/           # Generated NAH root (after setup)
```

## Host Manifest (host.json)

Declares which packages to install:

```json
{
  "host": {
    "name": "example-host",
    "nah_root": "./nah_root",
    "default_profile": "default"
  },
  "naks": [
    {
      "id": "com.example.sdk",
      "version": "1.2.3",
      "package": "../sdk/build/com.example.sdk-1.2.3.nak"
    }
  ],
  "apps": [
    {
      "id": "com.example.app",
      "version": "1.0.0",
      "package": "../apps/app/build/com.example.app-1.0.0.nap",
      "nak": "com.example.sdk"
    }
  ]
}
```

## Profiles

Profiles control NAK selection and policy:

| Profile | Mode | Description |
|---------|------|-------------|
| default.json | canonical | Use highest compatible NAK version |
| production.json | mapped | Pin specific NAK versions |
| canary.json | canonical | Testing with relaxed warnings |

Switch profiles:
```bash
./run.sh --profile production
```

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
NahContract* contract = nah_host_get_contract(host, "com.example.app", NULL, NULL);

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
