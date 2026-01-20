# NAH - Native Application Host {#mainpage}

NAH standardizes how native applications are installed, inspected, and launched. It provides a deterministic contract between applications and hosts.

## Quick Start

```cpp
#include <nah/nahhost.hpp>
#include <nah/semver.hpp>

// Create a host instance
auto host = nah::NahHost::create("/nah");

// List installed applications
for (const auto& app : host->listApplications()) {
    std::cout << app.id << "@" << app.version << "\n";
}

// Get launch contract for an app
auto result = host->getLaunchContract("com.example.myapp");
if (result.isOk()) {
    const auto& contract = result.value().contract;
    // Use contract.execution.binary, contract.environment, etc.
}
```

## Key Headers

| Header | Description |
|--------|-------------|
| `<nah/nahhost.hpp>` | Main API - NahHost class for contract composition |
| `<nah/semver.hpp>` | Semantic versioning - parse and compare versions |
| `<nah/manifest.hpp>` | Manifest parsing and building |
| `<nah/types.hpp>` | Core types - Manifest, Contract, etc. |

## Integration

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(nah
    GIT_REPOSITORY https://github.com/rtorr/nah.git
    GIT_TAG v1.0.0)
FetchContent_MakeAvailable(nah)
target_link_libraries(your_target PRIVATE nahhost)
```

### Conan 2

```python
def requirements(self):
    self.requires("nah/1.0.0")
```

## Links

- [GitHub Repository](https://github.com/rtorr/nah)
- [CLI Reference](https://github.com/rtorr/nah/blob/main/docs/cli.md)
- [Full Specification](https://github.com/rtorr/nah/blob/main/SPEC.md)
