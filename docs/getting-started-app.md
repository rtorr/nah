# Getting Started: App Developer

You're building an app that runs under NAH. This guide covers creating, building, and packaging your app - whether it's a native C++ app, a JavaScript bundle, a Python script, or any other type of application.

See [Core Concepts](concepts.md) for terminology.

## 1. Create an App Skeleton

```bash
nah init app myapp
cd myapp
```

This creates:
```
myapp/
├── nap.json       # App manifest (JSON format)
├── main.cpp       # Simple app template
├── README.md
├── bin/           # Put compiled binary here
├── lib/           # Optional libraries
└── assets/        # Optional assets
```

## 2. Understanding the Manifest

The generated `nap.json`:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v1.json",
  "app": {
    "identity": {
      "id": "com.example.myapp",
      "version": "1.0.0"
    },
    "execution": {
      "entrypoint": "bin/myapp"
    }
  },
  "metadata": {
    "description": "Example application",
    "author": "Your Name",
    "license": "MIT"
  }
}
```

### Key Fields

| Field | Description | Example |
|-------|-------------|---------|
| `app.identity.id` | Unique identifier (reverse domain notation) | `com.yourcompany.myapp` |
| `app.identity.version` | Your app's SemVer version | `1.0.0` |
| `app.execution.entrypoint` | Path to binary/script relative to app root | `bin/myapp` or `index.js` |
| `app.execution.loader` | Preferred loader from NAK (optional hint) | `"service"` or `"debug"` |
| `app.identity.nak_id` | NAK your app depends on (optional) | `com.example.sdk` |
| `app.identity.nak_version_req` | Version requirement (SemVer range) | `>=2.0.0 <3.0.0` |
| `app.layout.lib_dirs` | Library directories to add to library path | `["lib"]` |
| `app.layout.asset_dirs` | Asset directories | `["assets"]` |
| `app.environment` | Environment variables | `{"MY_VAR": "value"}` |

**Note on `app.execution.loader`:** If your NAK provides multiple loaders (e.g., `default`, `service`, `debug`), you can specify which one your app prefers. This is optional and can be overridden at install time (`nah install --loader X`) or runtime (`nah run --loader X`).

The `$schema` field enables validation and IDE autocompletion. See [docs/schemas/README.md](schemas/README.md) for the complete schema documentation.

## 3. Build Your App

### Native C++ App

```bash
mkdir -p bin
g++ -o bin/myapp main.cpp
```

### JavaScript Bundle

```bash
npm install
npm run build  # Creates dist/bundle.js
```

Update `nap.json` entrypoint:
```json
{
  "app": {
    "execution": {
      "entrypoint": "dist/bundle.js"
    }
  }
}
```

### Python Script

```bash
# No build step needed
```

Update `nap.json` entrypoint:
```json
{
  "app": {
    "execution": {
      "entrypoint": "main.py"
    }
  }
}
```

## 4. Package

```bash
nah pack .
# Creates: com.example.myapp-1.0.0.nap
```

The `.nap` package is a standard tar.gz archive containing your app and the `nap.json` manifest at the root.

## 5. Install and Run

```bash
nah install com.example.myapp-1.0.0.nap
# First run: Creates ~/.nah (default) if it doesn't exist

nah list
# Shows: com.example.myapp@1.0.0

nah show com.example.myapp
# Shows details

nah run com.example.myapp
# Runs your app
```

## Adding a NAK Dependency

If your app depends on an SDK (NAK), add the NAK information to your manifest:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v1.json",
  "app": {
    "identity": {
      "id": "com.yourcompany.myapp",
      "version": "1.0.0",
      "nak_id": "com.example.sdk",
      "nak_version_req": ">=2.0.0 <3.0.0"
    },
    "execution": {
      "entrypoint": "bin/myapp"
    },
    "layout": {
      "lib_dirs": ["lib"]
    }
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

## Bundle Applications (JavaScript, Python, etc.)

Bundle applications work identically to native apps in NAH v2.0. The key difference is that the `entrypoint` points to a script or bundle file, and the app typically depends on a NAK that provides the runtime:

### JavaScript Example

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v1.json",
  "app": {
    "identity": {
      "id": "com.example.myapp",
      "version": "1.0.0",
      "nak_id": "com.example.js-runtime",
      "nak_version_req": ">=2.0.0 <3.0.0"
    },
    "execution": {
      "entrypoint": "dist/bundle.js"
    },
    "layout": {
      "asset_dirs": ["assets"]
    },
    "environment": {
      "NODE_ENV": "production"
    }
  }
}
```

The NAK's loader executes your bundle using the appropriate runtime.

### Python Example

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v1.json",
  "app": {
    "identity": {
      "id": "com.example.pyapp",
      "version": "1.0.0",
      "nak_id": "org.python",
      "nak_version_req": "3.9.*"
    },
    "execution": {
      "entrypoint": "main.py"
    }
  }
}
```

### Background Service Example (with loader preference)

If your NAK provides multiple loaders (e.g., CLI vs service modes):

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v1.json",
  "app": {
    "identity": {
      "id": "com.example.daemon",
      "version": "1.0.0",
      "nak_id": "com.example.runtime",
      "nak_version_req": "^2.0.0"
    },
    "execution": {
      "entrypoint": "bin/service",
      "loader": "service"
    }
  }
}
```

**Loader Priority:**
1. `nah run --loader X` (highest - runtime override)
2. `nah install --loader X` (install-time pinning)
3. `app.execution.loader` (app's preference/hint)
4. NAK's default loader (fallback)

## Environment Variables

Your app receives these environment variables at runtime:

| Variable | Description | Example |
|----------|-------------|---------|
| `NAH_APP_ID` | Your app's ID | `com.example.myapp` |
| `NAH_APP_VERSION` | Your app's version | `1.0.0` |
| `NAH_APP_ROOT` | App install directory | `/nah/apps/com.example.myapp-1.0.0` |
| `NAH_NAK_ID` | Resolved NAK ID (if any) | `com.example.sdk` |
| `NAH_NAK_VERSION` | Resolved NAK version | `2.1.0` |
| `NAH_NAK_ROOT` | NAK install directory | `/nah/naks/com.example.sdk/2.1.0` |

Example usage:

```cpp
#include <iostream>
#include <cstdlib>

int main() {
    const char* app_id = std::getenv("NAH_APP_ID");
    const char* app_root = std::getenv("NAH_APP_ROOT");
    
    std::cout << "Running: " << (app_id ? app_id : "unknown") << std::endl;
    std::cout << "From: " << (app_root ? app_root : "unknown") << std::endl;
    return 0;
}
```

## CMake Integration

For C++ projects, use the NAH CMake helpers:

```cmake
cmake_minimum_required(VERSION 3.14)
project(myapp)

include(path/to/NahAppTemplate.cmake)

add_executable(myapp src/main.cpp)

nah_app(myapp
    ID "com.example.myapp"
    VERSION "1.0.0"
    NAK "com.example.sdk"
    NAK_VERSION ">=1.0.0 <2.0.0"
    ENTRYPOINT "bin/myapp"
    ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# Build package with: make nah_package
```

See [examples/apps/](../../examples/apps/) for complete working examples.

## Next Steps

- [CLI Reference](cli.md) for all commands
- [Getting Started: NAK](getting-started-nak.md) to package an SDK
- [Schema Documentation](schemas/README.md) for complete manifest reference
- [SPEC.md](../SPEC.md) for the full specification
