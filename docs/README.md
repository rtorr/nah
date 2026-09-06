# NAH documentation

NAH is a local launch-contract system. Start with the document that matches the work:

- [App packages](getting-started-app.md)
- [NAK packages](getting-started-nak.md)
- [Host integration](getting-started-host.md)
- [CLI reference](cli.md)
- [Specification](../SPEC.md)
- [Troubleshooting](troubleshooting.md)

The schemas in [schemas](schemas) define current JSON authoring formats.

## Library surface

Use `<nah/nah.h>` and `NAH::nah` for the complete filesystem, JSON, host, and execution API.

Use `<nah/nah_core.h>` and `NAH::core` when only dependency-free types and pure composition are needed.

Use `<nah/nah_archive.h>` and `NAH::package` for deterministic `.nap`/`.nak`
archive creation and extraction. This target adds zlib; it is not pulled into
`NAH::core` or `NAH::nah`.

`NahHost` provides these primary operations:

- `create(root)` and `discover(search_paths)`;
- `listApplications()` and `findApplication(id, version)`;
- `getLaunchContract(...)`;
- `executeApplication(...)` and `executeContract(...)`;

```cpp
#include <nah/nah.h>

int main() {
    auto host = nah::host::NahHost::create("./nah-root");
    auto result = host->getLaunchContract("com.example.app");
    if (!result.ok) return 1;
    return host->executeContract(result.contract);
}
```

Generated API documentation is configured by [Doxyfile](Doxyfile).
