# Getting Started: App Developer

You're building an app that depends on an SDK. This guide covers embedding a manifest and packaging your app.

See [Core Concepts](concepts.md) for terminology.

## 1. Create an App Skeleton

```bash
nah app init myapp
cd myapp
```

This creates:
```
myapp/
├── main.cpp
├── bin/
├── lib/
└── share/
```

## 2. Embed a Manifest

In your C++ source:

```cpp
#include <nah/manifest.h>

NAH_APP_MANIFEST(
    NAH_FIELD_ID("com.yourcompany.myapp")
    NAH_FIELD_VERSION("1.0.0")
    NAH_FIELD_NAK_ID("com.example.sdk")
    NAH_FIELD_NAK_VERSION_REQ(">=2.0.0 <3.0.0")
    NAH_FIELD_ENTRYPOINT("bin/myapp")
    NAH_FIELD_LIB_DIR("lib")
)

int main(int argc, char* argv[]) {
    // Your app code
    return 0;
}
```

### Manifest Fields

| Field | Description |
|-------|-------------|
| `id` | Unique identifier (reverse domain notation) |
| `version` | Your app's SemVer version |
| `nak_id` | NAK your app depends on |
| `nak_version_req` | Version requirement (SemVer range) |
| `entrypoint` | Path to binary relative to app root |

### Version Requirements

| Syntax | Meaning |
|--------|---------|
| `1.2.3` | Exactly 1.2.3 |
| `>=1.2.0` | 1.2.0 or higher |
| `>=1.0.0 <2.0.0` | 1.x (space = AND) |
| `>=1.0.0 \|\| >=3.0.0` | 1.x or 3.0+ (pipe = OR) |

## 3. Build

```bash
cmake -B build
cmake --build build
# Ensure binary is at bin/myapp
```

## 4. Package

```bash
nah app pack . -o myapp-1.0.0.nap
```

## 5. Test

Install into a NAH root:

```bash
nah --root /path/to/nah app install myapp-1.0.0.nap
nah --root /path/to/nah doctor com.yourcompany.myapp
nah --root /path/to/nah contract show com.yourcompany.myapp
```

## Alternative: Standalone Manifest

Instead of embedding, use a separate `manifest.json`:

```json
{
  "app": {
    "id": "com.yourcompany.myapp",
    "version": "1.0.0",
    "nak_id": "com.example.sdk",
    "nak_version_req": ">=2.0.0 <3.0.0",
    "entrypoint": "bin/myapp",
    "lib_dirs": ["lib"]
  }
}
```

Generate the binary manifest:

```bash
nah manifest generate manifest.json -o manifest.nah
```

Include `manifest.nah` in your package root.

For non-native applications (JavaScript, Python, etc.), see [Getting Started: Bundle Apps](getting-started-bundle.md).

## Next Steps

- [CLI Reference](cli.md) for all commands
- [SPEC.md](../SPEC.md) for the full manifest format
