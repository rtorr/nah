# GameEngine SDK (Conan-based NAK Example)

This example demonstrates the workflow for creating a NAK from a Conan-based
SDK project. The SDK aggregates multiple Conan dependencies (zlib, OpenSSL,
curl, etc.) into a single NAK that applications target.

## The Model

```
┌─────────────────────────────────────────────────────────┐
│  NAK: com.example.gameengine                            │
│                                                         │
│  ┌───────┐ ┌─────────┐ ┌──────┐ ┌────────┐ ┌────────┐  │
│  │ zlib  │ │ openssl │ │ curl │ │ spdlog │ │  SDK   │  │
│  │       │ │         │ │      │ │  fmt   │ │  code  │  │
│  └───────┘ └─────────┘ └──────┘ └────────┘ └────────┘  │
│                                                         │
│  Apps just see "the engine" - dependencies are internal │
└─────────────────────────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│  NAP: com.example.mygame                                │
│                                                         │
│  manifest:                                              │
│    nak_id = "com.example.gameengine"                    │
│    nak_version_req = ">=1.0.0 <2.0.0"                   │
│                                                         │
│  Game code - uses engine API, doesn't know about deps   │
└─────────────────────────────────────────────────────────┘
```

## Project Structure

```
conan-sdk/
├── conanfile.py          # SDK recipe with dependencies
├── CMakeLists.txt        # Build + NAK packaging
├── include/sdk/          # Public SDK headers
├── src/                  # SDK implementation
├── bin/                  # Loader source
├── resources/            # Engine resources
└── META/
    └── nak.json.in       # NAK manifest template
```

## Dependencies

This SDK bundles:
- **zlib** - Compression for assets
- **OpenSSL** - Crypto and TLS
- **libcurl** - HTTP networking
- **nlohmann_json** - JSON parsing
- **spdlog** + **fmt** - Logging

Apps using this SDK don't need to know about any of these.

## Building and Packaging

```bash
# Install dependencies with full_deploy to collect libs
conan install . --output-folder=build --build=missing \
    --deployer=full_deploy --deployer-folder=build/deploy

# Configure and build
cd build
cmake .. --preset conan-release
cmake --build build/Release

# Package into NAK (collects SDK + all Conan deps)
cmake --build build/Release --target package_nak
```

Output: `build/build/Release/com.example.gameengine-1.0.0.nak` (~9MB)

## NAK Contents

```
com.example.gameengine-1.0.0.nak
├── META/
│   └── nak.json
├── lib/
│   ├── libgameengine.a       # SDK library
│   ├── libz.a                # zlib
│   ├── libssl.a              # OpenSSL
│   ├── libcrypto.a           # OpenSSL
│   ├── libcurl.a             # curl
│   ├── libspdlog.a           # spdlog
│   └── libfmt.a              # fmt
├── bin/
│   └── engine-loader         # NAK loader
└── resources/
    └── engine.json
```

## Using the SDK in an App

```cpp
#include <sdk/engine.hpp>

int main() {
    auto engine = gameengine::Engine::create({});
    engine->initialize();
    
    // Use networking (backed by curl + OpenSSL)
    auto response = engine->network().get("https://api.example.com/data");
    
    // Load compressed assets (backed by zlib)
    auto data = engine->assets().load_compressed("level1.dat.gz");
    
    // Crypto operations (backed by OpenSSL)
    auto hash = engine->crypto().sha256("some data");
    
    engine->shutdown();
    return 0;
}
```

The app's manifest:

```json
{
  "app": {
    "id": "com.example.mygame",
    "version": "1.0.0",
    "nak_id": "com.example.gameengine",
    "nak_version_req": ">=1.0.0 <2.0.0",
    "entrypoint": "bin/mygame"
  }
}
```

## Installing the NAK

```bash
# Install into a NAH root
nah --root /path/to/nah_root nak install build/build/Release/com.example.gameengine-1.0.0.nak
```
