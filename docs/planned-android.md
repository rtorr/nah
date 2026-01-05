# Android Support (Planned)

NAH is a **query system** - it tells you **where** things are and **how** to launch them. The host platform does the actual work.

On Android, NAH would provide the same contract it provides on Linux or macOS. The Android integrator (platform team, OEM, or app developer) decides how to use that contract.

## What NAH Provides

Given an app with an embedded manifest, NAH returns:

```json
{
  "nak": {
    "id": "com.vendor.sdk",
    "version": "2.0.0",
    "root": "/data/nah/naks/com.vendor.sdk/2.0.0"
  },
  "environment": {
    "NAH_NAK_ROOT": "/data/nah/naks/com.vendor.sdk/2.0.0",
    "ASSET_PATH": "/data/nah/naks/com.vendor.sdk/2.0.0/assets"
  }
}
```

The app uses these paths however it needs.

## What the Android Integrator Decides

NAH doesn't prescribe Android-specific details. The integrator decides:

- Where NAKs are stored (`/data/nah/`, external storage, etc.)
- How NAKs are distributed (APK, ADB, download)
- Security model (permissions, signing, sandboxing)
- Service architecture (Binder, content provider, embedded)
- Cleanup policy for unused NAK versions

## NAK Contents

NAKs contain native libraries and assets - the same as on Linux/macOS:

```
com.vendor.sdk-2.0.0.nak
├── META/nak.toml
├── lib/
│   ├── arm64-v8a/
│   │   └── libsdk.so
│   └── armeabi-v7a/
│       └── libsdk.so
├── assets/
└── config/
```

The APK contains the app's native code, which links against the SDK libraries at the paths NAH provides.

## Example Usage

```kotlin
// Query NAH for the contract
val contract = NahClient(context).getContract()

// Pass paths to native code
nativeInit(contract.nak.root, contract.environment["ASSET_PATH"])
```

## Testing via ADB

The existing CLI works on Android via ADB:

```bash
adb push nah /data/local/bin/
adb shell /data/local/bin/nah --root /data/local/nah nak install my-sdk.nak
adb shell /data/local/bin/nah --root /data/local/nah contract show com.example.myapp --json
```

## Status

Not yet implemented.
