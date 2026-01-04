# NAH - Native Application Host

[![CI](https://github.com/rtorr/nah/actions/workflows/ci.yml/badge.svg)](https://github.com/rtorr/nah/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-API-blue)](https://nah.rtorr.com/)

NAH provides deterministic launch contracts for native applications. Applications declare their requirements in a manifest. SDKs (NAKs) declare what they provide. Hosts define policy. NAH composes these into an executable specification: binary path, arguments, environment, library paths.

## Core Mechanism

NAH separates three concerns that are typically conflated:

| Artifact | Owner | Contains |
|----------|-------|----------|
| **App Manifest** | App developer | Identity, NAK requirement, entrypoint, layout |
| **NAK** | SDK developer | Libraries, resources, loader (optional) |
| **Host Profile** | Host platform | Binding policy, allowed versions, environment |

At install time, NAH pins a compatible NAK version. At launch time, NAH composes these artifacts into a **Launch Contract** containing the exact execution parameters.

## Usage

```bash
# Initialize host
nah profile init /opt/nah

# Install SDK and app
nah --root /opt/nah nak install sdk-2.1.0.nak
nah --root /opt/nah app install myapp-1.0.0.nap

# Show launch contract
nah --root /opt/nah contract show com.example.myapp
```

Contract output:
```
Application: com.example.myapp v1.0.0
NAK: com.example.sdk v2.1.0
Binary: /opt/nah/apps/com.example.myapp-1.0.0/bin/myapp
CWD: /opt/nah/apps/com.example.myapp-1.0.0
Library Paths: /opt/nah/naks/com.example.sdk/2.1.0/lib
```

## Multiple SDK Versions

NAKs are installed by ID and version. Multiple versions coexist:

```
/opt/nah/naks/
├── com.vendor.runtime/1.0.0/
├── com.vendor.runtime/2.1.0/
└── com.other.framework/3.2.0/
```

Apps declare version requirements (e.g., `>=2.0.0 <3.0.0`). NAH selects the highest compatible version at install time.

## Installation

**Pre-built binaries:**
```bash
# Linux
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-linux-x64.tar.gz | tar xz
sudo mv nah /usr/local/bin/

# macOS
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-macos-arm64.tar.gz | tar xz
sudo mv nah /usr/local/bin/
```

**From source:**
```bash
git clone https://github.com/rtorr/nah.git && cd nah
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

Requirements: CMake 3.21+, C++17 compiler

## Library Integration

```cmake
include(FetchContent)
FetchContent_Declare(nah GIT_REPOSITORY https://github.com/rtorr/nah.git GIT_TAG v1.0.0)
FetchContent_MakeAvailable(nah)
target_link_libraries(your_target PRIVATE nahhost)
```

Conan: `nah/1.0.0`

## Documentation

| Document | Description |
|----------|-------------|
| [Concepts](docs/concepts.md) | NAK, NAP, Host Profile, Launch Contract |
| [Getting Started: Host](docs/getting-started-host.md) | Deploy NAH and manage apps |
| [Getting Started: NAK](docs/getting-started-nak.md) | Build an SDK package |
| [Getting Started: App](docs/getting-started-app.md) | Build an app with manifest |
| [CLI Reference](docs/cli.md) | Command documentation |
| [API Reference](https://nah.rtorr.com/) | Library documentation |
| [Specification](SPEC.md) | Normative specification |

## License

MIT
