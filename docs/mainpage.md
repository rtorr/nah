# NAH C++ API {#mainpage}

NAH composes installed app, runtime, and host metadata into an inspectable launch contract.

```cpp
#include <nah/nah.h>

int main() {
    auto host = nah::host::NahHost::create("/opt/product/nah");
    auto result = host->getLaunchContract("com.example.app");
    if (!result.ok) return 1;
    return host->executeContract(result.contract, {"argument"});
}
```

Link `NAH::nah` for the complete API or `NAH::core` for dependency-free composition types and functions.

- [CLI reference](cli.md)
- [Specification](../SPEC.md)
- [Source repository](https://github.com/rtorr/nah)
