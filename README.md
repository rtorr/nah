# NAH - Native Application Host

[![CI](https://github.com/rtorr/nah/actions/workflows/ci.yml/badge.svg)](https://github.com/rtorr/nah/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-API-blue)](https://nah.rtorr.com/)

NAH is a launch contract system for native applications.

When you deploy a native application, someone must figure out how to launch it: which binary, what library paths, which environment variables, what SDK version. This information typically lives in documentation that drifts, install scripts that diverge, or tribal knowledge that doesn't scale.

NAH eliminates this by making applications self-describing. Apps declare what they need. SDKs declare what they provide. Hosts declare policy. NAH composes these into a launch contract - the exact parameters needed to run the application.

## What NAH Provides

**For host operators**: Install an app, query its launch contract. No need to read documentation or reverse-engineer scripts. The contract tells you the binary path, library paths, environment variables, and working directory.

**For app developers**: Embed a manifest declaring your SDK requirement and entrypoint. Ship one package that works on any host with a compatible SDK, regardless of where the SDK is installed.

**For SDK developers**: Package your libraries once. Multiple versions coexist on the same host. Apps pin to compatible versions at install time. Update the SDK without breaking existing apps.

## Example

```bash
# Install from local files
nah --root /opt/nah nak install vendor-sdk-2.1.0.nak
nah --root /opt/nah app install myapp-1.0.0.nap

# Or install directly from URLs (with SHA-256 verification)
nah --root /opt/nah nak install 'https://example.com/vendor-sdk-2.1.0.nak#sha256=abc123...'
nah --root /opt/nah app install 'https://example.com/myapp-1.0.0.nap#sha256=def456...'

# Query the launch contract
nah --root /opt/nah contract show com.example.myapp
```

```
Application: com.example.myapp v1.0.0
SDK: com.vendor.sdk v2.1.0
Binary: /opt/nah/apps/com.example.myapp-1.0.0/bin/myapp
CWD: /opt/nah/apps/com.example.myapp-1.0.0
Library Paths: /opt/nah/naks/com.vendor.sdk/2.1.0/lib
Environment:
  NAH_APP_ID=com.example.myapp
  NAH_APP_VERSION=1.0.0
  NAH_NAK_ROOT=/opt/nah/naks/com.vendor.sdk/2.1.0
```

The contract is deterministic. Same inputs, same output. Auditable before execution.

## Key Properties

- **No network at launch time**: Contract composition uses only local state
- **No dependency solving**: Apps declare requirements, hosts install SDKs, NAH matches them
- **Version coexistence**: Multiple SDK versions installed side-by-side
- **Install-time pinning**: SDK version locked when app is installed, not resolved at launch
- **Host controls layout**: SDKs and apps go where the host decides, not where the app expects

## When to Use NAH

NAH is designed for environments where:

- Apps and SDKs come from different vendors or teams
- Hosts need to control where software is installed
- Multiple SDK versions must coexist for different apps
- Launch configuration must be auditable
- Apps must remain portable across different host configurations

## Installation

```bash
# npm (recommended)
npm install -g @rtorr/nah

# Linux (manual)
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-linux-x64.tar.gz | tar xz
sudo mv nah /usr/local/bin/

# macOS (manual)
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-macos-arm64.tar.gz | tar xz
sudo mv nah /usr/local/bin/

# From source
git clone https://github.com/rtorr/nah.git && cd nah
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
sudo cmake --install build
```

## Library Integration

NAH can be embedded as a C++ library:

```cmake
include(FetchContent)
FetchContent_Declare(nah GIT_REPOSITORY https://github.com/rtorr/nah.git GIT_TAG v1.0.0)
FetchContent_MakeAvailable(nah)
target_link_libraries(your_target PRIVATE nahhost)
```

## Documentation

| Document | Description |
|----------|-------------|
| [Concepts](docs/concepts.md) | Core terminology: manifests, NAKs, profiles, contracts |
| [Getting Started: Host](docs/getting-started-host.md) | Set up a host and deploy applications |
| [Getting Started: SDK](docs/getting-started-nak.md) | Package an SDK for distribution |
| [Getting Started: App](docs/getting-started-app.md) | Build an application with a manifest |
| [CLI Reference](docs/cli.md) | Command-line interface documentation |
| [Specification](SPEC.md) | Normative specification |

## License

MIT
