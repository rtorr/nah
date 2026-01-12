# Getting Started: App Developer

You're building an app that runs under NAH. This guide covers creating, building, and packaging your app.

See [Core Concepts](concepts.md) for terminology.

## 1. Create an App Skeleton

```bash
nah init app myapp
cd myapp
```

This creates:
```
myapp/
├── main.cpp        # Simple app template
├── manifest.json   # App manifest (JSON format)
├── README.md
├── bin/            # Put compiled binary here
├── lib/            # Optional libraries
└── share/          # Optional assets
```

## 2. The Generated Template

The generated `main.cpp` is a simple standalone app:

```cpp
#include <iostream>
#include <cstdlib>

int main() {
    // NAH environment variables are set by the host at launch
    const char* app_id = std::getenv("NAH_APP_ID");
    const char* app_version = std::getenv("NAH_APP_VERSION");
    const char* app_root = std::getenv("NAH_APP_ROOT");
    
    std::cout << "Hello from " << (app_id ? app_id : "unknown") << std::endl;
    if (app_version) std::cout << "Version: " << app_version << std::endl;
    if (app_root) std::cout << "Root: " << app_root << std::endl;
    
    return 0;
}
```

The generated `manifest.json`:

```json
{
  "app": {
    "id": "com.example.myapp",
    "version": "1.0.0",
    "entrypoint": "bin/myapp"
  }
}
```

## 3. Build

```bash
mkdir -p bin
g++ -o bin/myapp main.cpp
```

## 4. Package

```bash
nah pack .
# Creates: myapp.nap (or com.example.myapp-1.0.0.nap)
```

The `nah pack` command automatically converts `manifest.json` to binary format.

## 5. Install and Run

```bash
nah install myapp.nap
# First run: Creates ~/.nah (default) if it doesn't exist

nah list
# Shows: com.example.myapp@1.0.0

nah status com.example.myapp
# Shows contract details
```

## Adding a NAK Dependency

If your app depends on an SDK (NAK), add `nak_id` to your manifest:

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

### Version Requirements

| Syntax | Meaning |
|--------|---------|
| `1.2.3` | Exactly 1.2.3 |
| `>=1.2.0` | 1.2.0 or higher |
| `>=1.0.0 <2.0.0` | 1.x (space = AND) |
| `>=1.0.0 \|\| >=3.0.0` | 1.x or 3.0+ (pipe = OR) |
| `*` | Any version |

## Alternative: Embedded Manifest

For native apps, you can embed the manifest in the binary instead of using a separate file:

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

This requires linking against libnahhost.

## Manifest Fields Reference

| Field | Required | Description |
|-------|----------|-------------|
| `id` | Yes | Unique identifier (reverse domain notation) |
| `version` | Yes | Your app's SemVer version |
| `entrypoint` | Yes | Path to binary relative to app root |
| `nak_id` | No | NAK your app depends on (omit for standalone apps) |
| `nak_version_req` | No | Version requirement (SemVer range) |
| `lib_dirs` | No | Library directories to add to library path |
| `description` | No | Human-readable description |
| `author` | No | Author name or email |

For non-native applications (JavaScript, Python, etc.), see [Getting Started: Bundle Apps](getting-started-bundle.md).

## Next Steps

- [CLI Reference](cli.md) for all commands
- [SPEC.md](../SPEC.md) for the full manifest format
