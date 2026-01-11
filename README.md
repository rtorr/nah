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
# Install packages (auto-detects .nap/.nak)
nah install vendor-sdk-2.1.0.nak
nah install myapp-1.0.0.nap

# Or install directly from URLs
nah install https://example.com/vendor-sdk-2.1.0.nak
nah install https://example.com/myapp-1.0.0.nap

# Query the launch contract
nah status com.example.myapp
```

```
Application: com.example.myapp v1.0.0
NAK: com.vendor.sdk v2.1.0
Binary: /opt/nah/apps/com.example.myapp-1.0.0/bin/myapp
CWD: /opt/nah/apps/com.example.myapp-1.0.0
Library Paths: /opt/nah/naks/com.vendor.sdk/2.1.0/lib
Environment (NAH_*):
  NAH_APP_ID=com.example.myapp
  NAH_APP_VERSION=1.0.0
  NAH_NAK_ROOT=/opt/nah/naks/com.vendor.sdk/2.1.0
```

The contract is deterministic. Same inputs, same output. Auditable before execution.

## CLI Overview

```
nah install <source>      Install app or NAK (auto-detected from .nap/.nak)
nah uninstall <id>        Remove an installed package
nah list                  List installed apps and NAKs
nah pack <dir>            Create a .nap or .nak package
nah status [target]       Show status, validate files, diagnose issues
nah init <type> <dir>     Create new project (app, nak, or root)
nah profile list|set      Manage host profiles
```

## Decision Flowchart

```
What are you building?
│
├─ An application that uses an SDK
│  └─ Create an app with: nah init app ./myapp
│     Then install with: nah install myapp.nap
│
├─ An SDK/framework for apps to use
│  └─ Create a NAK with: nah init nak ./mysdk
│     Then install with: nah install mysdk.nak
│
└─ A host to run NAH applications
   └─ Create a root with: nah init root ./my-nah
      Then install packages: nah install <package>
      Then check status: nah status
```

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
| [How It Works](docs/how-it-works.md) | Internals of the launch contract system |
| [Concepts](docs/concepts.md) | Core terminology: manifests, NAKs, profiles, contracts |
| [Getting Started: Host](docs/getting-started-host.md) | Set up a host and deploy applications |
| [Getting Started: SDK](docs/getting-started-nak.md) | Package an SDK for distribution |
| [Getting Started: App](docs/getting-started-app.md) | Build an application with a manifest |
| [CLI Reference](docs/cli.md) | Command-line interface documentation |
| [Troubleshooting](docs/troubleshooting.md) | Common issues and solutions |
| [Specification](SPEC.md) | Normative specification |
| [Contributing](CONTRIBUTING.md) | Development setup and releasing |

## License

MIT
