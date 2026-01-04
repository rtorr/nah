# NAH - Native Application Host

[![CI](https://github.com/rtorr/nah/actions/workflows/ci.yml/badge.svg)](https://github.com/rtorr/nah/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-API-blue)](https://nah.rtorr.com/)

NAH is a launch contract system for native applications. It computes how to run an application - binary path, library paths, environment variables - from declarations provided by the app, the SDK, and the host.

NAH is not a package manager. It does not download packages, resolve transitive dependencies, or manage repositories. Apps and SDKs are delivered to the host through whatever mechanism you already use. NAH's job starts after they arrive: given what's installed, compute the exact parameters needed to launch correctly.

## The Problem

When you deploy a native application, someone must determine how to launch it: which binary to run, what library paths to set, which environment variables are required, what SDK version it needs. This information typically lives in documentation, install scripts, or tribal knowledge. It drifts. It breaks.

## The Solution

NAH makes applications self-describing. Each party declares what they own:

- **App developers** embed a manifest: identity, SDK requirement, entrypoint
- **SDK developers** package their libraries and declare what they provide
- **Host operators** configure policy: where things go, what versions are allowed

NAH composes these declarations into a launch contract. The contract is queryable, auditable, and deterministic.

## What This Looks Like

```bash
# Install SDK and app (delivered via your existing mechanism)
nah --root /opt/nah nak install vendor-sdk-2.1.0.nak
nah --root /opt/nah app install myapp-1.0.0.nap

# Query the launch contract
nah --root /opt/nah contract show com.example.myapp
```

Output:
```
Binary: /opt/nah/apps/com.example.myapp-1.0.0/bin/myapp
CWD: /opt/nah/apps/com.example.myapp-1.0.0
Library Paths: /opt/nah/naks/vendor-sdk/2.1.0/lib
Environment:
  NAH_APP_ID=com.example.myapp
  NAH_NAK_ROOT=/opt/nah/naks/vendor-sdk/2.1.0
```

The contract is computed from declarations. No documentation to read. No install script to debug.

## Multiple SDK Versions

Multiple SDK versions coexist on the same host. Apps declare version requirements (e.g., `>=2.0.0 <3.0.0`). NAH selects a compatible version at install time and pins it. Legacy apps continue using old SDK versions. New apps use new versions. No manual coordination required.

## Installation

```bash
# Linux
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-linux-x64.tar.gz | tar xz
sudo mv nah /usr/local/bin/

# macOS
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-macos-arm64.tar.gz | tar xz
sudo mv nah /usr/local/bin/

# From source
git clone https://github.com/rtorr/nah.git && cd nah
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
sudo cmake --install build
```

## Library Integration

NAH can be used as a C++ library for programmatic contract composition:

```cmake
include(FetchContent)
FetchContent_Declare(nah GIT_REPOSITORY https://github.com/rtorr/nah.git GIT_TAG v1.0.0)
FetchContent_MakeAvailable(nah)
target_link_libraries(your_target PRIVATE nahhost)
```

## Documentation

- [Concepts](docs/concepts.md) - Terminology and architecture
- [Getting Started: Host](docs/getting-started-host.md) - Deploy and manage applications
- [Getting Started: SDK](docs/getting-started-nak.md) - Package an SDK for NAH
- [Getting Started: App](docs/getting-started-app.md) - Build an app with a manifest
- [CLI Reference](docs/cli.md) - Command documentation
- [Specification](SPEC.md) - Normative specification

## License

MIT
