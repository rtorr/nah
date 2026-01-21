# Framework SDK (Simple NAK Example)

This example demonstrates creating a NAK from a simple C SDK with no external
dependencies. It uses only CMake and produces a self-contained NAK pack.

## The Model

```
+-------------------------------------------------------+
|  NAK: com.example.sdk                                 |
|                                                       |
|  +---------------+  +------------+  +-------------+   |
|  | libframework  |  |  loader    |  |  resources  |   |
|  | (shared lib)  |  |  binary    |  |  (config)   |   |
|  +---------------+  +------------+  +-------------+   |
|                                                       |
|  Pure C SDK - no external dependencies                |
+-------------------------------------------------------+
                        |
                        v
+-------------------------------------------------------+
|  NAP: com.example.app                                 |
|                                                       |
|  manifest:                                            |
|    nak_id = "com.example.sdk"                         |
|    nak_version_req = ">=1.0.0 <2.0.0"                 |
|                                                       |
|  App code - links against libframework               |
+-------------------------------------------------------+
```

## Project Structure

```
sdk/
├── CMakeLists.txt        # Build + NAK packaging
├── include/framework/    # Public SDK headers
├── lib/                  # SDK implementation
│   ├── framework.c       # Core functionality
│   ├── resources.c       # Resource loading
│   └── lifecycle.c       # App lifecycle hooks
├── bin/
│   └── loader.c          # NAK loader binary
├── resources/            # SDK resources
└── nak.json.in           # NAK manifest template
```

## Building and Packaging

```bash
cd sdk
mkdir build && cd build
cmake ..
make
make nah_package
```

Output: `build/com.example.sdk-1.2.3.nak`

## NAK Contents

```
com.example.sdk-1.2.3.nak
├── nak.json              # NAK manifest
├── lib/
│   └── libframework.so   # SDK shared library
├── bin/
│   └── framework_loader  # NAK loader
└── resources/
    └── ...               # SDK resources
```

## SDK API

```c
#include <framework/framework.h>

// Initialize the framework
int framework_init(void);

// Load a resource by name
const char* framework_load_resource(const char* name);

// Get framework version
const char* framework_version(void);

// Shutdown the framework
void framework_shutdown(void);
```

## Using the SDK in an App

```c
#include <framework/framework.h>

int main(int argc, char** argv) {
    if (framework_init() != 0) {
        return 1;
    }
    
    printf("Framework version: %s\n", framework_version());
    
    const char* config = framework_load_resource("config.json");
    // Use config...
    
    framework_shutdown();
    return 0;
}
```

The app's manifest:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v1.json",
  "app": {
    "identity": {
      "id": "com.example.app",
      "version": "1.0.0",
      "nak_id": "com.example.sdk",
      "nak_version_req": ">=1.0.0 <2.0.0"
    },
    "execution": {
      "entrypoint": "bin/app"
    }
  }
}
```

## Installing the NAK

```bash
nah install build/com.example.sdk-1.2.3.nak
```

## Comparison with conan-sdk

| Aspect | sdk (this) | conan-sdk |
|--------|------------|-----------|
| Language | C | C++17 |
| Dependencies | None | zlib, OpenSSL, curl, spdlog |
| Package Manager | None | Conan 2 |
| Complexity | Minimal | Production-like |
| Use Case | Simple SDKs | Complex SDKs with deps |

Use this example as a starting point for SDKs that don't need external dependencies.
Use `conan-sdk/` when you need to bundle third-party libraries into your NAK.
