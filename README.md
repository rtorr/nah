# NAH - Native Application Host

[![CI](https://github.com/rtorr/nah/actions/workflows/ci.yml/badge.svg)](https://github.com/rtorr/nah/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-API-blue)](https://nah.rtorr.com/)

Apps need SDKs. Hosts run apps. But these are often built by different teams or vendors. How do they connect without everyone knowing everything about each other?

**NAH makes each party self-describing.** Apps declare what they need. SDKs declare what they provide. Hosts set policy. NAH composes them into a deterministic launch contract.

```
┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
│      App        │   │    SDK (NAK)    │   │      Host       │
│                 │   │                 │   │                 │
│ "I need SDK 2.x"│ + │ "I have 2.1.0"  │ + │ "Here's policy" │
└────────┬────────┘   └────────┬────────┘   └────────┬────────┘
         └──────────────┬────────────────────────────┘
                        ▼
              ┌─────────────────────┐
              │   Launch Contract   │
              │                     │
              │ binary, env, paths  │
              │ (auditable, exact)  │
              └─────────────────────┘
```

## Try It

```bash
# Set up a host
nah profile init ./myhost
nah --root ./myhost nak install sdk-2.1.0.nak
nah --root ./myhost app install myapp-1.0.0.nap

# See exactly how the app will launch
nah --root ./myhost contract show com.example.myapp
```

Output: binary path, library paths, environment variables, working directory. No guessing.

## Who Is This For?

| Role | What You Do | Guide |
|------|-------------|-------|
| **Host Platform** | Run apps and SDKs from vendors, control policy | [Getting Started: Host](docs/getting-started-host.md) |
| **SDK Developer** | Ship runtimes/frameworks that apps depend on | [Getting Started: NAK](docs/getting-started-nak.md) |
| **App Developer** | Build apps that need SDKs to run | [Getting Started: App](docs/getting-started-app.md) |

Each party declares only what's in their domain. NAH handles the seams.

## Multiple SDKs, Multiple Versions

Hosts can have many SDKs installed. Apps declare requirements, NAH matches them:

```
naks/
├── com.vendor.runtime/
│   ├── 1.0.0/    ← legacy-app gets this
│   └── 2.1.0/    ← modern-app gets this
└── com.other.framework/
    └── 3.2.0/    ← other-app gets this
```

Legacy apps keep working. New apps use new SDKs. No manual coordination.

## Install

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

## CLI Cheatsheet

```bash
nah profile init <dir>           # Initialize host
nah nak install <file.nak>       # Install SDK
nah app install <file.nap>       # Install app
nah contract show <app-id>       # Show launch contract
nah doctor <app-id>              # Diagnose issues
nah --json contract show <id>    # Machine-readable
```

[Full CLI Reference](docs/cli.md)

## Use as a Library

```cmake
include(FetchContent)
FetchContent_Declare(nah GIT_REPOSITORY https://github.com/rtorr/nah.git GIT_TAG v1.0.0)
FetchContent_MakeAvailable(nah)
target_link_libraries(your_target PRIVATE nahhost)
```

Or with Conan: `nah/1.0.0`

## Documentation

- [CLI Reference](docs/cli.md)
- [API Reference](https://nah.rtorr.com/)
- [Specification](SPEC.md)

## License

MIT
