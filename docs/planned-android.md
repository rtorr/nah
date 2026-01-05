# Android Support (Planned)

This document describes how NAH could work with Android as the host platform.

## Overview

NAH is a **query system** - it answers "where is everything and how should it be launched?" The host platform (Android, Linux, etc.) does the actual library loading and process spawning. NAH just provides the contract.

This separation makes Android support straightforward: NAH tells Android **what** and **where**, Android handles **how**.

## What NAH Provides on Android

1. **Shared SDK assets** - Multiple apps share one copy of engine assets, shaders, configs
2. **Version management** - Different apps pin different SDK versions
3. **Configuration** - Host controls paths and policies
4. **Auditing** - Know exactly what's installed and which version each app uses

## What NAH Does NOT Do

- Load native libraries (Android's linker does this)
- Spawn processes (Android's runtime does this)
- Manage APK installation (Play Store / package manager does this)

## Architecture

```
┌─────────────────────────────────────────────┐
│  Android App (com.example.myapp)            │
│  ┌─────────────────────────────────────┐    │
│  │ Embedded manifest.nah               │    │
│  │   nak_id: com.vendor.sdk            │    │
│  │   nak_version_req: ^2.0.0           │    │
│  └─────────────────────────────────────┘    │
│                    │                        │
│                    ▼                        │
│         NAH Client Library (AAR)            │
│         queries NAH Service via Binder      │
└─────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────┐
│  NAH Service (privileged system app)        │
│                                             │
│  /data/nah/                                 │
│  ├── naks/                                  │
│  │   └── com.vendor.sdk/                    │
│  │       ├── 2.0.0/                         │
│  │       │   ├── assets/                    │
│  │       │   ├── config/                    │
│  │       │   └── shaders/                   │
│  │       └── 2.1.0/                         │
│  ├── host/                                  │
│  │   └── profiles/                          │
│  │       └── default.toml                   │
│  └── registry/                              │
│       └── naks/                             │
│                                             │
│  Returns launch contract to requesting app  │
└─────────────────────────────────────────────┘
```

## Contract Usage

An Android app queries NAH and receives:

```json
{
  "app": {
    "id": "com.example.mygame",
    "version": "1.0.0",
    "root": "/data/data/com.example.mygame"
  },
  "nak": {
    "id": "com.vendor.sdk",
    "version": "2.0.0",
    "root": "/data/nah/naks/com.vendor.sdk/2.0.0"
  },
  "environment": {
    "NAH_NAK_ROOT": "/data/nah/naks/com.vendor.sdk/2.0.0",
    "SHADER_PATH": "/data/nah/naks/com.vendor.sdk/2.0.0/shaders",
    "ASSET_PATH": "/data/nah/naks/com.vendor.sdk/2.0.0/assets"
  }
}
```

The app then uses these paths however it needs - passing them to its native code, loading assets, reading configs.

## NAK Contents on Android

NAKs for Android would typically contain **data**, not native code:

```
com.vendor.gameengine-2.0.0.nak
├── META/
│   └── nak.toml
├── assets/
│   ├── models/
│   ├── textures/
│   └── audio/
├── shaders/
│   ├── vulkan/
│   └── gles/
└── config/
    └── engine.toml
```

Native libraries stay in the APK where Android expects them.

## Implementation Requirements

### 1. NAH Service App

A privileged Android app/service that:
- Manages `/data/nah/` directory
- Exposes Binder interface for contract queries
- Handles NAK installation (via APK, ADB, or download)
- Enforces host policies

### 2. NAH Client Library (AAR)

A lightweight Android library that apps include:
- Binds to NAH Service
- Reads embedded manifest from APK
- Queries contract
- Returns paths to app

### 3. NAK Distribution

Options:
- **APK-based**: NAKs packaged as APKs, installed via Play Store
- **ADB sideload**: For development/enterprise
- **Download**: NAH Service fetches from URL

### 4. Permissions

Android manifest permissions:
- `com.nah.permission.QUERY_CONTRACT` - Apps can query their own contract
- `com.nah.permission.INSTALL_NAK` - Privileged apps can install NAKs
- `com.nah.permission.MANAGE_HOST` - System apps can modify host profiles

## Example Usage (Kotlin)

```kotlin
class MyGameActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Query NAH for launch contract
        val nah = NahClient(this)
        val contract = nah.getContract()
        
        if (contract != null) {
            // Pass SDK paths to native code
            nativeInit(
                contract.nak.root,
                contract.environment["SHADER_PATH"],
                contract.environment["ASSET_PATH"]
            )
        }
    }
    
    external fun nativeInit(nakRoot: String, shaderPath: String?, assetPath: String?)
}
```

## Development/Testing

For testing without a full Android port:

```bash
# On rooted Android or via ADB
adb shell mkdir -p /data/local/nah
adb push nah /data/local/bin/
adb push my-sdk.nak /data/local/tmp/

# Install NAK
adb shell /data/local/bin/nah --root /data/local/nah nak install /data/local/tmp/my-sdk.nak

# Query contract
adb shell /data/local/bin/nah --root /data/local/nah contract show com.example.myapp --json
```

## Open Questions

1. **Storage location**: `/data/nah/` requires system privileges. Alternative: shared external storage with content provider access?

2. **NAK updates**: How to update a NAK when apps are using it? Versioning helps (old version stays until no apps need it).

3. **Disk space**: Shared NAKs save space, but who manages cleanup of unused versions?

4. **Security**: How to verify NAK integrity on Android? APK signing? Separate signature verification?

## Status

Not yet implemented. This document captures the design direction for future work.
