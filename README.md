# NAH - Native Application Host

[![CI](https://github.com/rtorr/nah/actions/workflows/ci.yml/badge.svg)](https://github.com/rtorr/nah/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-API-blue)](https://nah.rtorr.com/)

You've built your app. It works on your machine. Now you need to ship it.

What environment variables need to be set? What library paths? What SDK version does it actually need? How does the host know how to launch it correctly?

Today this lives in README files that get outdated, shell scripts that diverge per environment, and tribal knowledge. The result: "works on my machine," finger-pointing between teams, and hours lost to deployment debugging.

**NAH makes apps self-describing.** The app carries its launch contract - what it needs to run. The host reads the contract and composes the correct environment. No guessing, no drift, no ambiguity.

## How It Works

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Application   │     │      Host       │     │  Launch Contract│
│                 │     │                 │     │                 │
│ "I need SDK 2.x │ ──► │ "I have SDK 2.1 │ ──► │ binary: /app/bin│
│  and these libs"│     │  installed here"│     │ env: LD_PATH=...│
│                 │     │                 │     │ cwd: /app       │
└─────────────────┘     └─────────────────┘     └─────────────────┘
     Manifest              Profile               Executable Spec
    (immutable)            (mutable)              (auditable)
```

The app declares intent. The host provides policy. NAH composes them into a concrete, auditable launch specification.

## Quick Example

```bash
# Host: set up the environment
nah profile init ./my-deployment
nah --root ./my-deployment nak install sdk-2.1.0.nak

# Deploy an app
nah --root ./my-deployment app install myapp-1.0.0.nap

# See exactly how the app will launch
nah --root ./my-deployment contract show com.example.myapp
```

Output:
```
Application: com.example.myapp v1.0.0
NAK: com.example.sdk v2.1.0
Binary: /my-deployment/apps/com.example.myapp-1.0.0/bin/myapp
CWD: /my-deployment/apps/com.example.myapp-1.0.0

Library Paths (LD_LIBRARY_PATH):
  /my-deployment/naks/com.example.sdk/2.1.0/lib

Environment:
  NAH_APP_ID=com.example.myapp
  NAH_APP_VERSION=1.0.0
  NAH_NAK_ROOT=/my-deployment/naks/com.example.sdk/2.1.0
```

No shell scripts. No guessing. The contract is the truth.

## Key Concepts

| Term | Description |
|------|-------------|
| **Manifest** | Immutable declaration embedded in the app - what it needs to run |
| **NAK** | Native App Kit - versioned SDK/runtime bundle that apps depend on |
| **Profile** | Host configuration - where things are, what's allowed, policy overrides |
| **Contract** | The composed result - exactly how to launch the app |

## Installation

### Pre-built Binaries

```bash
# Linux
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-linux-x64.tar.gz | tar xz
sudo mv nah /usr/local/bin/

# macOS
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-macos-arm64.tar.gz | tar xz
sudo mv nah /usr/local/bin/
```

### Build from Source

```bash
git clone https://github.com/rtorr/nah.git
cd nah
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

Requirements: CMake 3.21+, C++17 compiler

## For App Developers

Embed a manifest in your application:

```cpp
#include <nah/manifest.h>

NAH_APP_MANIFEST(
    NAH_FIELD_ID("com.example.myapp")
    NAH_FIELD_VERSION("1.0.0")
    NAH_FIELD_NAK_ID("com.example.sdk")
    NAH_FIELD_NAK_VERSION_REQ(">=2.0.0 <3.0.0")
    NAH_FIELD_ENTRYPOINT("bin/myapp")
    NAH_FIELD_LIB_DIR("lib")
)
```

Package and ship:

```bash
nah app pack ./my-app -o myapp-1.0.0.nap
```

Your app now carries its requirements. No external documentation needed.

## For Platform/Ops Teams

Set up a host environment:

```bash
nah profile init /opt/nah
```

Configure policy in `/opt/nah/host/profiles/default.toml`:

```toml
schema = "nah.host.profile.v1"

[nak]
binding_mode = "canonical"
allow_versions = ["2.*"]  # Only allow SDK 2.x

[warnings]
nak_not_found = "error"   # Fail fast if SDK missing

[environment]
DEPLOYMENT_ENV = "production"
```

Deploy apps:

```bash
nah --root /opt/nah nak install sdk-2.1.0.nak
nah --root /opt/nah app install myapp-1.0.0.nap
```

Audit before launch:

```bash
nah --root /opt/nah doctor com.example.myapp
nah --root /opt/nah --json contract show com.example.myapp
```

## Using NAH as a Library

Integrate contract composition into your own tooling:

```cmake
include(FetchContent)
FetchContent_Declare(nah
    GIT_REPOSITORY https://github.com/rtorr/nah.git
    GIT_TAG v1.0.0)
FetchContent_MakeAvailable(nah)

target_link_libraries(your_target PRIVATE nahhost)
```

Or with Conan:

```ini
[requires]
nah/1.0.0
```

## CLI Reference

```bash
nah profile init <dir>          # Initialize a NAH root
nah nak install <file.nak>      # Install an SDK/runtime
nah app install <file.nap>      # Install an application
nah app list                    # List installed apps
nah contract show <app-id>      # Show launch contract
nah doctor <app-id>             # Diagnose issues
nah --json contract show <id>   # Machine-readable output
```

See [docs/cli.md](docs/cli.md) for complete reference.

## Documentation

| Resource | Description |
|----------|-------------|
| [CLI Reference](docs/cli.md) | Complete command-line documentation |
| [API Reference](https://nah.rtorr.com/) | Library documentation |
| [Getting Started: App](docs/getting-started-app.md) | Build apps with NAH manifests |
| [Getting Started: NAK](docs/getting-started-nak.md) | Create SDK/runtime bundles |
| [Getting Started: Host](docs/getting-started-host.md) | Deploy NAH in production |
| [SPEC.md](SPEC.md) | Complete specification |

## Why NAH?

**For app developers**: Ship once, run anywhere NAH is deployed. No per-environment hacks.

**For platform teams**: Know exactly what apps need. Audit before you deploy. Update SDKs without rebuilding apps.

**For everyone**: Stop debugging "works on my machine." The contract is the source of truth.

## License

MIT
