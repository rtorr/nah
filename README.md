# NAH - Native Application Host

[![CI](https://github.com/rtorr/nah/actions/workflows/ci.yml/badge.svg)](https://github.com/rtorr/nah/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-API-blue)](https://nah.rtorr.com/)

NAH is a launch contract system for native applications.

When you deploy a native application, someone must figure out how to launch it: which binary, what library paths, which environment variables, what SDK version. This information typically lives in documentation that drifts, install scripts that diverge, or tribal knowledge that doesn't scale.

NAH eliminates this by making applications self-describing. Apps declare what they need. SDKs declare what they provide. Hosts declare policy. NAH composes these into a launch contract - the exact parameters needed to run the application.

## Example

```bash
# Install packages
nah install vendor-sdk-2.1.0.nak
nah install myapp-1.0.0.nap

# Query the launch contract
nah status com.example.myapp
```

```
Application: com.example.myapp v1.0.0
NAK: com.vendor.sdk v2.1.0
Binary: /opt/nah/apps/com.example.myapp-1.0.0/bin/myapp
CWD: /opt/nah/apps/com.example.myapp-1.0.0
Library Paths: /opt/nah/naks/com.vendor.sdk/2.1.0/lib
```

The contract is deterministic. Same inputs, same output. Auditable before execution.

## CLI

```
nah install <source>      Install app (.nap) or SDK (.nak)
nah uninstall <id>        Remove a package
nah list                  List installed packages
nah pack <dir>            Create a package
nah status [target]       Show status and diagnose issues
nah init <type> <dir>     Create new project (app, nak, root)
nah profile list|set      Manage host profiles
nah host install <dir>    Set up host from manifest
```

## Installation

```bash
# npm (recommended)
npm install -g @rtorr/nah

# Linux
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-linux-x64.tar.gz | tar xz
sudo mv nah /usr/local/bin/

# macOS
curl -L https://github.com/rtorr/nah/releases/latest/download/nah-macos-arm64.tar.gz | tar xz
sudo mv nah /usr/local/bin/
```

## Library Integration

NAH v2.0 is now a **header-only library**, making integration even simpler:

```cmake
include(FetchContent)
FetchContent_Declare(nah
    GIT_REPOSITORY https://github.com/rtorr/nah.git
    GIT_TAG v2.0.0
)
FetchContent_MakeAvailable(nah)

# Link to the interface library
target_link_libraries(your_target PRIVATE NAH::nah)
```

```cpp
#define NAH_HOST_IMPLEMENTATION
#include <nah/nah.h>

auto host = nah::host::NahHost::create();
int exit_code = host->executeApplication("com.example.app");
```

`nah.h` includes everything. For finer control, include individual headers:

| Header | Description |
| ------ | ----------- |
| `nah/nah_core.h` | Pure composition engine (no I/O, no dependencies) |
| `nah/nah_json.h` | JSON parsing (requires nlohmann/json) |
| `nah/nah_fs.h` | Filesystem operations |
| `nah/nah_exec.h` | Process execution |
| `nah/nah_overrides.h` | NAH_OVERRIDE_* handling |
| `nah/nah_host.h` | High-level NahHost class |

See [Library Headers](docs/README.md#library-headers) for detailed documentation.

## Platform Support

| Platform           | Status                           |
| ------------------ | -------------------------------- |
| Linux (x64, arm64) | Supported                        |
| macOS (x64, arm64) | Supported                        |
| Windows            | Code exists, not actively tested |
| Android            | Planned for future release       |

## Documentation

- [How It Works](docs/how-it-works.md) - Internals of the launch contract system
- [Concepts](docs/concepts.md) - Core terminology: manifests, NAKs, profiles, contracts
- [Getting Started: Host](docs/getting-started-host.md) - Set up a host and deploy applications
- [Getting Started: SDK](docs/getting-started-nak.md) - Package an SDK for distribution
- [Getting Started: App](docs/getting-started-app.md) - Build an application with a manifest
- [CLI Reference](docs/cli.md) - Command-line interface documentation
- [Troubleshooting](docs/troubleshooting.md) - Common issues and solutions
- [Specification](SPEC.md) - Normative specification
- [Migration Guide](MIGRATION.md) - Migrating from v1.0 to v2.0
- [Contributing](CONTRIBUTING.md) - Development setup and releasing

## License

MIT
