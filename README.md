# NAH - Native Application Host

[![CI](https://github.com/rtorr/nah/actions/workflows/ci.yml/badge.svg)](https://github.com/rtorr/nah/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-API-blue)](https://nah.rtorr.com/)

NAH defines a contract between applications, SDKs, and host platforms. Each party declares what they provide or require, and NAH composes them into a deterministic launch specification.

**The problem**: Apps need SDKs. Hosts run apps. But these are often built by different teams, companies, or vendors. How do they connect without everyone needing to know everything about each other?

**The solution**: Each party describes only what's in their domain. NAH handles the composition.

## Three Perspectives

### I'm a Host Platform

You run apps you didn't build, using SDKs you didn't write. You need to:

- Install SDKs and apps from vendors
- Control where things go on your filesystem
- Set policies (which SDK versions are allowed, what capabilities are granted)
- Know exactly how apps will launch before running them

**With NAH**: You define a profile with your policies. Apps and SDKs come as packages that describe themselves. You install them, and NAH tells you exactly how each app will launch given your configuration.

```bash
# Set up your host
nah profile init /opt/myplatform

# Install SDKs from vendors
nah --root /opt/myplatform nak install vendor-sdk-2.1.0.nak

# Install apps
nah --root /opt/myplatform app install thirdparty-app-1.0.0.nap

# See exactly what will happen at launch time
nah --root /opt/myplatform contract show com.vendor.app
```

You never need to read the app's source code or the SDK's build scripts. The contract shows you the truth: binary path, library paths, environment variables, working directory. Audit it, approve it, run it.

### I'm an SDK/Framework Developer

You ship a runtime, framework, or SDK that apps depend on. You need to:

- Provide libraries, binaries, and resources
- Tell apps where to find things
- Not care about host filesystem layouts
- Support multiple versions installed side-by-side

**With NAH**: You package your SDK as a NAK (Native App Kit). You declare what you provide - library paths, environment variables, optional loader binaries. Apps target your SDK by ID and version range. Hosts install you wherever they want.

```toml
# META/nak.toml - your SDK's declaration
schema = "nah.nak.pack.v1"

[nak]
id = "com.mycompany.sdk"
version = "2.1.0"

[paths]
resource_root = "resources"
lib_dirs = ["lib", "lib/plugins"]

[environment]
MY_SDK_VERSION = "2.1.0"
```

```bash
# Package it
nah nak pack ./my-sdk -o my-sdk-2.1.0.nak
```

You don't know where the host will install you. You don't care. Apps reference you by ID, and NAH resolves paths at launch time.

### I'm an App Developer

You build an app that needs an SDK. You need to:

- Declare which SDK version you require
- Ship your app without hardcoding paths
- Work on any host that has a compatible SDK

**With NAH**: You embed a manifest in your app declaring what you need. You don't specify paths - just the SDK ID and version requirement. The host figures out the rest.

```cpp
#include <nah/manifest.h>

NAH_APP_MANIFEST(
    NAH_FIELD_ID("com.mycompany.myapp")
    NAH_FIELD_VERSION("1.0.0")
    NAH_FIELD_NAK_ID("com.vendor.sdk")
    NAH_FIELD_NAK_VERSION_REQ(">=2.0.0 <3.0.0")
    NAH_FIELD_ENTRYPOINT("bin/myapp")
    NAH_FIELD_LIB_DIR("lib")
)
```

```bash
# Package it
nah app pack ./my-app -o myapp-1.0.0.nap
```

You don't know where the host will put your app or the SDK. You don't care. Your manifest says "I need SDK 2.x" and NAH ensures you get it at launch.

## Multiple SDKs, Multiple Versions

A host can have many SDKs installed, each with multiple versions. Apps declare what they need, and NAH matches them to what's available.

```
Host: /opt/myplatform
├── naks/
│   ├── com.vendor.runtime/
│   │   ├── 1.0.0/          # Legacy apps still need this
│   │   ├── 2.0.0/
│   │   └── 2.1.0/          # Latest
│   └── com.other.framework/
│       └── 3.2.0/
└── apps/
    ├── legacy-app/         # needs runtime >=1.0.0 <2.0.0 → gets 1.0.0
    ├── modern-app/         # needs runtime >=2.0.0 <3.0.0 → gets 2.1.0
    └── other-app/          # needs framework ^3.0.0 → gets 3.2.0
```

Each app gets the right SDK version for its requirements. Legacy apps keep working. New apps use new SDKs. Different apps can use completely different SDKs. The host doesn't need to manage this - NAH resolves it from the manifests.

```bash
# Install SDK versions as needed
nah --root /opt/myplatform nak install vendor-runtime-1.0.0.nak
nah --root /opt/myplatform nak install vendor-runtime-2.1.0.nak
nah --root /opt/myplatform nak install other-framework-3.2.0.nak

# Install apps - NAH matches each to a compatible SDK
nah --root /opt/myplatform app install legacy-app-1.0.0.nap
nah --root /opt/myplatform app install modern-app-1.0.0.nap

# Each contract shows which SDK version was selected
nah --root /opt/myplatform contract show com.example.legacy-app
# → NAK: com.vendor.runtime v1.0.0

nah --root /opt/myplatform contract show com.example.modern-app
# → NAK: com.vendor.runtime v2.1.0
```

## How It Fits Together

```
┌──────────────────────────────────────────────────────────────────┐
│                         HOST PLATFORM                            │
│  Profile: policies, allowed versions, environment overrides      │
│                                                                  │
│  ┌─────────────────────┐      ┌─────────────────────┐            │
│  │   SDKs (NAKs)       │      │    Application      │            │
│  │                     │      │                     │            │
│  │ runtime 1.0, 2.0,   │      │ "I need runtime 2.x │            │
│  │ 2.1, framework 3.2  │      │  and run bin/app"   │            │
│  └──────────┬──────────┘      └──────────┬──────────┘            │
│             │                            │                       │
│             └──────────┬─────────────────┘                       │
│                        ▼                                         │
│             ┌─────────────────────┐                              │
│             │   Launch Contract   │                              │
│             │                     │                              │
│             │ binary: /opt/.../app│                              │
│             │ NAK: runtime 2.1.0  │                              │
│             │ LD_PATH: /opt/.../lib                              │
│             └─────────────────────┘                              │
└──────────────────────────────────────────────────────────────────┘
```

Each party stays in their lane:

- **Apps** declare dependencies, not paths
- **SDKs** declare capabilities, not install locations
- **Hosts** control layout and policy
- **Contracts** are the composed result - auditable, deterministic

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

## CLI Quick Reference

```bash
# Host setup
nah profile init <dir>              # Initialize a NAH root
nah nak install <file.nak>          # Install an SDK
nah app install <file.nap>          # Install an app

# Inspection
nah app list                        # List installed apps
nah nak list                        # List installed SDKs
nah contract show <app-id>          # Show launch contract
nah doctor <app-id>                 # Diagnose issues

# Packaging
nah app pack <dir> -o <file.nap>    # Package an app
nah nak pack <dir> -o <file.nak>    # Package an SDK

# Machine-readable output
nah --json contract show <app-id>   # JSON for scripting
```

See [docs/cli.md](docs/cli.md) for complete reference.

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

Or with Conan: `nah/1.0.0`

## Documentation

| Resource                                              | Description                         |
| ----------------------------------------------------- | ----------------------------------- |
| [Getting Started: Host](docs/getting-started-host.md) | Set up a host platform              |
| [Getting Started: NAK](docs/getting-started-nak.md)   | Build an SDK/framework package      |
| [Getting Started: App](docs/getting-started-app.md)   | Build an app with a manifest        |
| [CLI Reference](docs/cli.md)                          | Complete command-line documentation |
| [API Reference](https://nah.rtorr.com/)               | Library documentation               |
| [SPEC.md](SPEC.md)                                    | Complete specification              |

## Why NAH?

| Role     | Without NAH                                                                  | With NAH                                          |
| -------- | ---------------------------------------------------------------------------- | ------------------------------------------------- |
| **Host** | Read every app's docs, write custom launch scripts, hope SDK paths are right | Install packages, inspect contracts, apply policy |
| **SDK**  | Write install scripts per platform, document paths, hope apps find you       | Declare what you provide, ship a `.nak`           |
| **App**  | Hardcode paths or write setup scripts, break when hosts differ               | Declare what you need, ship a `.nap`              |

Everyone focuses on their own domain. NAH handles the seams.

## License

MIT
