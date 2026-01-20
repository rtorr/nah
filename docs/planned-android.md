# Android Support (Planned)

NAH **prepares** Android builds. At build time, NAH resolves SDK dependencies and stages the correct native libraries so they can be bundled into the APK. Once prepared, Android uses its normal mechanisms - the APK contains everything, and there's no NAH at runtime.

This means developers get the NAH experience (version resolution, contract satisfaction, reproducible builds) while Android gets a standard APK with bundled native code.

## Build-Time Flow

1. SDK developer packages native libs as a NAK
2. App developer declares SDK dependency in manifest
3. Build system uses NAH to:
   - Resolve correct SDK version
   - Copy `.so` files into APK's `jniLibs/`
   - Embed manifest for auditing
4. APK ships with everything bundled

```
Build Machine                          APK Output
┌─────────────────────┐               ┌─────────────────────┐
│ NAH Root            │               │ app.apk             │
│ └── naks/           │               │ ├── lib/            │
│     └── com.sdk/    │  ──build──▶   │ │   ├── arm64-v8a/  │
│         └── 2.0.0/  │               │ │   │   ├── libapp.so
│             └── lib/│               │ │   │   └── libsdk.so
│                     │               │ │   └── armeabi-v7a/│
└─────────────────────┘               │ └── assets/         │
                                      └─────────────────────┘
```

## NAK Contents

NAKs contain native libraries organized by Android ABI:

```
com.vendor.sdk-2.0.0.nak
├── META/nak.json
├── lib/
│   ├── arm64-v8a/
│   │   └── libsdk.so
│   └── armeabi-v7a/
│       └── libsdk.so
├── assets/
└── config/
```

## Build Integration

Gradle plugin or CMake integration would:

```groovy
// build.gradle
nah {
    root = "/path/to/nah"
    // Reads manifest from native code, resolves SDK, copies libs
}
```

Or via command line in CI:

```bash
# Resolve and copy SDK libs into jniLibs
nah --root ./nah-root app pack ./src -o build/app.nap
cp build/staged/lib/* app/src/main/jniLibs/
```

## What Gets Embedded

The APK contains:
- App's native code (`libapp.so`)
- SDK's native code (`libsdk.so`) copied from NAK
- Embedded manifest (for auditing: "this APK was built with SDK v2.0.3")

## Runtime

At runtime, there is no NAH. Android loads all `.so` files from the APK using its standard mechanism. The app just works - exactly as if the developer had manually copied the libraries into place.

NAH's job is done at build time. It prepared Android with the right dependencies, and Android takes it from there.

## What the Android Integrator Decides

NAH prepares the build. The integrator decides:

- How NAKs are distributed to build machines
- Build system integration (Gradle, CMake, Bazel)
- Which ABIs to support

## Status

Not yet implemented.
