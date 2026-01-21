# Example NAH Host

A standalone host configuration demonstrating NAH integration with multiple NAKs and applications.

## Overview

This host installs:

| Type | ID | Version | Description |
|------|----|---------|-------------|
| NAK | `com.example.sdk` | 1.2.3 | Framework SDK for C/C++ apps |
| App | `com.example.app` | 1.0.0 | Basic C app (uses sdk) |
| App | `com.example.app_c` | 1.0.0 | C++ app (uses sdk) |
| App | `com.example.script-app` | 1.0.0 | Script-only app |

## Usage

```bash
# Install packages into NAH root
cd examples
./scripts/setup_host.sh

# Clean install (removes existing NAH root first)
./scripts/setup_host.sh --clean

# Run all apps
./scripts/run_apps.sh

# Run specific app
../build/tools/nah/nah --root demo_nah_root run com.example.app
```

> **Note:** Packages must be built before running setup.
> Use `../scripts/build_all.sh` to build all NAKs and apps first.

## Structure

```
host/
├── nah.json            # Host configuration
└── README.md           # This file
```

## Host Configuration (nah.json)

Configures host-specific environment variables and paths:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nah.v1.json",
  "host": {
    "root": "/opt/nah",
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
    },
    "install": [
      "../sdk/build/com.example.sdk-1.2.3.nak",
      "../apps/app/build/com.example.app-1.0.0.nap",
      "../apps/app_c/build/com.example.app_c-1.0.0.nap",
      "../apps/script-app/build/com.example.script-app-1.0.0.nap"
    ]
  }
}
```

Package paths are relative to the host directory. Packages are self-describing (id/version extracted from the package itself).
