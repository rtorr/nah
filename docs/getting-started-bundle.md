# Getting Started: Bundle App Developer

You're building an application using JavaScript, Python, or another interpreted language that runs inside a NAK runtime. This guide covers creating manifests and packaging bundle applications.

See [Core Concepts](concepts.md) for terminology.

## Overview

Bundle applications differ from native applications:

| Aspect | Native App | Bundle App |
|--------|------------|------------|
| Language | C, C++, Rust, etc. | JavaScript, Python, etc. |
| Manifest storage | Embedded in binary | `manifest.nah` file |
| Manifest creation | C++ macro at compile time | `nah manifest generate` |
| Entrypoint | Native executable | Script or bundle file |
| Execution | Direct or via loader | NAK loader invokes runtime |

The NAK provides the runtime that executes your bundle. Common examples:
- React Native runtime for JS bundles
- Python interpreter NAK for Python apps
- Node.js NAK for server-side JavaScript

## 1. Create Manifest Input File

Create a `manifest.json` file describing your application:

```json
{
  "$schema": "nah.manifest.input.v2",
  "app": {
    "id": "com.yourcompany.myapp",
    "version": "1.0.0",
    "nak_id": "com.example.js-runtime",
    "nak_version_req": ">=2.0.0 <3.0.0",
    "entrypoint": "dist/bundle.js",
    "description": "My JavaScript Application",
    "author": "Your Name",
    "license": "MIT",
    "asset_dirs": ["assets"],
    "environment": {
      "NODE_ENV": "production"
    }
  }
}
```

### Required Fields

| Field | Description | Example |
|-------|-------------|---------|
| `$schema` | Schema version | `nah.manifest.input.v2` |
| `app.id` | Unique identifier | `com.yourcompany.myapp` |
| `app.version` | SemVer version | `1.0.0` |
| `app.nak_id` | Runtime NAK ID | `com.example.js-runtime` |
| `app.nak_version_req` | Version requirement | `>=2.0.0 <3.0.0` |
| `app.entrypoint` | Relative path to entry | `dist/bundle.js` |

### Optional Fields

| Field | Description |
|-------|-------------|
| `description` | Human-readable description |
| `author` | Author name or organization |
| `license` | SPDX license identifier |
| `homepage` | Project URL |
| `entrypoint_args` | Arguments passed to entrypoint |
| `lib_dirs` | Library directories (relative) |
| `asset_dirs` | Asset directories (relative) |
| `exports` | Named asset exports |
| `environment` | Default environment variables |
| `permissions` | Capability declarations |

### Path Requirements

All paths must be:
- **Relative** - No leading `/`
- **Contained** - No `..` path components
- **Forward-slash** - Use `/` even on Windows

Valid: `dist/bundle.js`, `assets/images`
Invalid: `/dist/bundle.js`, `../shared/lib`

## 2. Generate Binary Manifest

Convert the JSON to a binary TLV manifest:

```bash
nah manifest generate manifest.json -o manifest.nah
```

Verify the generated manifest:

```bash
nah manifest show manifest.nah
```

Output:
```
ID: com.yourcompany.myapp
Version: 1.0.0
NAK ID: com.example.js-runtime
NAK Version Req: >=2.0.0 <3.0.0
Entrypoint: dist/bundle.js
```

## 3. Build Your Bundle

Build your application using your standard toolchain:

```bash
# Example: React Native
npx react-native bundle --platform ios --entry-file index.js --bundle-output dist/bundle.js

# Example: Webpack
npx webpack --mode production

# Example: Python (just copy sources)
cp -r src dist
```

## 4. Create Package Structure

Organize your files for packaging:

```
package/
├── manifest.nah          # Generated manifest
├── dist/
│   └── bundle.js         # Your application entry
├── assets/               # Optional assets
│   ├── images/
│   └── config.json
└── META/                  # Optional metadata
    └── package.json
```

## 5. Package as NAP

Create the distributable package:

```bash
nah app pack package -o myapp-1.0.0.nap
```

## 6. Test Installation

Install and verify in a NAH root:

```bash
# Initialize a test NAH root
nah profile init ./test-nah

# Install the runtime NAK (get from your SDK provider)
nah --root ./test-nah nak install js-runtime-2.0.0.nak

# Install your app
nah --root ./test-nah app install myapp-1.0.0.nap

# Verify installation
nah --root ./test-nah app verify com.yourcompany.myapp

# View the launch contract
nah --root ./test-nah contract show com.yourcompany.myapp
```

## Complete Example: React Native App

### manifest.json

```json
{
  "$schema": "nah.manifest.input.v2",
  "app": {
    "id": "com.example.rnapp",
    "version": "2.1.0",
    "nak_id": "com.mycompany.rn-runtime",
    "nak_version_req": ">=3.0.0",
    "entrypoint": "bundle.jsbundle",
    "description": "Example React Native Application",
    "author": "Mobile Team",
    "asset_dirs": ["assets"],
    "exports": [
      {
        "id": "splash",
        "path": "assets/splash.png",
        "type": "image/png"
      }
    ],
    "environment": {
      "NODE_ENV": "production",
      "RN_DEBUG": "false"
    }
  }
}
```

### Build Script

```bash
#!/bin/bash
set -e

# Build the JS bundle
npx react-native bundle \
  --platform ios \
  --dev false \
  --entry-file index.js \
  --bundle-output package/bundle.jsbundle \
  --assets-dest package/assets

# Generate manifest
nah manifest generate manifest.json -o package/manifest.nah

# Create package
nah app pack package -o dist/rnapp-2.1.0.nap

echo "Package created: dist/rnapp-2.1.0.nap"
```

## Asset Exports

Export named assets for programmatic access:

```json
{
  "exports": [
    {
      "id": "app-icon",
      "path": "assets/icon.png",
      "type": "image/png"
    },
    {
      "id": "config",
      "path": "config/app.json",
      "type": "application/json"
    }
  ]
}
```

The NAK runtime can query these by ID rather than hardcoding paths.

## Permissions

Bundle apps typically don't declare permissions because the NAK runtime defines the sandbox. Only declare permissions if your runtime supports per-app capability escalation:

```json
{
  "permissions": {
    "filesystem": ["read:app://assets/*"],
    "network": ["connect:https://api.example.com:443"]
  }
}
```

Permission format:
- Filesystem: `<operation>:<selector>` (operations: `read`, `write`, `execute`)
- Network: `<operation>:<selector>` (operations: `connect`, `listen`, `bind`)

## Security Model

Bundle apps run inside the NAK runtime's sandbox:

1. **NAK is the trust boundary** - The runtime controls what the bundle can access
2. **Bundle permissions are advisory** - The runtime enforces its own sandbox
3. **Package signature covers everything** - Tampering is detected at install time
4. **Host profile governs the NAK** - Host policy controls what the runtime can do

This mirrors mobile platforms: React Native JS runs inside a native container that defines the security boundary.

## Troubleshooting

### "missing or invalid schema"

Ensure your manifest.json has the correct schema:
```json
{
  "$schema": "nah.manifest.input.v2"
}
```

### "entrypoint must be a relative path"

Remove leading slashes from paths:
```json
// Wrong
"entrypoint": "/dist/bundle.js"

// Correct
"entrypoint": "dist/bundle.js"
```

### "path must not contain '..'"

Use only forward paths within the package:
```json
// Wrong
"lib_dirs": ["../shared/lib"]

// Correct - copy shared libs into package
"lib_dirs": ["lib"]
```

### NAK not found

Ensure the NAK is installed before the app:
```bash
nah --root ./my-nah nak install runtime-2.0.0.nak
nah --root ./my-nah app install myapp-1.0.0.nap
```

## Next Steps

- [CLI Reference](cli.md) - All available commands
- [Core Concepts](concepts.md) - NAH terminology
- [Getting Started: NAK Developer](getting-started-nak.md) - Building a runtime NAK
- [SPEC.md](../SPEC.md) - Full specification
