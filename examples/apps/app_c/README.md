# Application C - C++ App Example

This is a simple C++ application that demonstrates using the NAH framework with the Framework SDK.

## Overview

This example shows:
- Using the new NAH v1.1.0 JSON manifest format (`nap.json`)
- Depending on a NAK (com.example.sdk)
- Standard CMake build patterns with `nah_app()` convenience function
- Simple application structure with assets

## Manifest Format

Uses the modern JSON manifest format packaged at the root of the `.nap` file:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v1.json",
  "app": {
    "identity": {
      "id": "com.example.app_c",
      "version": "1.0.0",
      "nak_id": "com.example.sdk",
      "nak_version_req": ">=1.0.0 <2.0.0"
    },
    "execution": {
      "entrypoint": "bin/app_c"
    }
  }
}
```

## Building

```bash
mkdir build && cd build
cmake ..
make
make nah_package
```

This creates `com.example.app_c-1.0.0.nap` as a standard tar.gz archive.

## Running

```bash
# Install
nah install com.example.app_c-1.0.0.nap

# Run
nah run com.example.app_c
```

## Structure

```
app_c/
├── CMakeLists.txt      # Build configuration
├── src/
│   └── main.cpp        # Application entry point
└── assets/
    └── config.json     # Application assets
```

