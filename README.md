# NAH - Native Application Host

[![CI](https://github.com/rtorr/nah/actions/workflows/ci.yml/badge.svg)](https://github.com/rtorr/nah/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-API-blue)](https://nah.rtorr.com/)

When you deploy a native application, someone must determine how to launch it: which binary to run, what library paths to set, which environment variables are required, what SDK version it needs. This information typically lives in documentation, install scripts, or tribal knowledge. It drifts. It breaks.

NAH solves this by making applications self-describing. The app declares what it requires. The host declares what it provides. NAH computes the exact launch parameters - binary, arguments, environment, library paths - as a queryable contract.

## What This Looks Like

Install an SDK and an app:
```bash
nah --root /opt/nah nak install vendor-sdk-2.1.0.nak
nah --root /opt/nah app install myapp-1.0.0.nap
```

Ask NAH how to launch the app:
```bash
nah --root /opt/nah contract show com.example.myapp
```

NAH returns:
```
Binary: /opt/nah/apps/com.example.myapp-1.0.0/bin/myapp
CWD: /opt/nah/apps/com.example.myapp-1.0.0
Library Paths: /opt/nah/naks/vendor-sdk/2.1.0/lib
Environment:
  NAH_APP_ID=com.example.myapp
  NAH_NAK_ROOT=/opt/nah/naks/vendor-sdk/2.1.0
```

No documentation to read. No install script to debug. The contract is computed from declarations.

## How It Works

Three parties, each declaring only what they own:

- **App developers** embed a manifest: "I am app X, I need SDK Y version >=2.0"
- **SDK developers** package their SDK: "I am SDK Y version 2.1, here are my libraries"
- **Host operators** set policy: "I allow SDK Y versions 2.x, install apps here"

NAH composes these into a launch contract. If the app needs SDK 2.x and the host has 2.1 installed, the contract resolves the paths. If the SDK is missing, NAH reports it.

Multiple SDK versions coexist. Legacy apps get old versions. New apps get new versions. The host doesn't coordinate this manually.

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
