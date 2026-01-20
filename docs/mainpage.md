# NAH - Native Application Host {#mainpage}

NAH standardizes how native applications are installed, inspected, and launched. It provides a deterministic contract between applications and hosts.

## Quick Start

```cpp
#include <nah/nah.h>

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
| `<nah/nah.h>` | Complete API - includes all headers |
| `<nah/nah_host.h>` | NahHost class for contract composition |
| `<nah/nah_semver.h>` | Semantic versioning - parse and compare versions |
| `<nah/nah_core.h>` | Core types and pure computation |

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
