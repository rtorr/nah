# NAH

[![CI](https://github.com/rtorr/nah/actions/workflows/ci.yml/badge.svg)](https://github.com/rtorr/nah/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-API-blue)](https://nah.rtorr.com/)

NAH composes native application metadata into an explicit launch contract: executable, arguments, environment, library paths, runtime selection, and trust state. It can inspect that contract or execute it.

The project intentionally covers local, filesystem-backed packages. It does not fetch packages, resolve remote registries, verify signatures, or enforce declared permissions. Those are host or distribution-system responsibilities.

## Quick start

Build with CMake 3.21+ and a C++17 compiler:

```bash
cmake -S . -B build -DNAH_ENABLE_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Create and run a standalone app:

```bash
build/tools/nah/nah init --app --id com.example.hello hello
build/tools/nah/nah pack hello --output hello.nap
build/tools/nah/nah --root ./nah-root install hello.nap
build/tools/nah/nah --root ./nah-root show com.example.hello
build/tools/nah/nah --root ./nah-root run com.example.hello
build/tools/nah/nah --root ./nah-root uninstall com.example.hello
```

Global options such as `--root` precede the subcommand.

## Package model

- `.nap` is a gzip-compressed tar archive containing one app and `nap.json`.
- `.nak` is the same archive format for one local runtime or SDK and `nak.json`.
- An app may be standalone or request a NAK with a semantic-version range.
- Installation records pin the highest installed matching NAK version.
- Packages reject links, special files, path traversal, invalid identities, and oversized content.

Run `nah <command> --help` for the CLI contract. Manifest schemas live in [`docs/schemas`](docs/schemas), and [`SPEC.md`](SPEC.md) defines composition semantics.

## Library

The public headers require C++17. `NAH::core` is dependency-free composition
logic. `NAH::nah` adds JSON, filesystem, store, host, and execution APIs.
`NAH::package` adds archive support and depends on zlib.

```cmake
include(FetchContent)
FetchContent_Declare(nah
    GIT_REPOSITORY https://github.com/rtorr/nah.git
    GIT_TAG v3.0.0
)
FetchContent_MakeAvailable(nah)
target_link_libraries(my_host PRIVATE NAH::nah)
```

```cpp
#include <nah/nah.h>

auto host = nah::host::NahHost::create("./nah-root");
auto result = host->getLaunchContract("com.example.hello");
if (result.ok) {
    return host->executeContract(result.contract);
}
```

Installed CMake packages provide the same three targets.

## Support

CI covers Linux, macOS, and Windows on x64. Other architectures are best effort until their release jobs are continuously exercised.

- [CLI reference](docs/cli.md)
- [Concepts](docs/concepts.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Contributing](CONTRIBUTING.md)

MIT licensed. See [LICENSE](LICENSE).
