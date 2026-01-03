# Getting Started: App Developer

Build an application that runs on NAH.

## Prerequisites

- NAH CLI installed (`nah` command available)
- A NAK (SDK/framework) to target

## 1. Create App Skeleton

```bash
nah app init myapp
cd myapp
```

This creates:
```
myapp/
├── main.cpp      # Application with embedded manifest
├── bin/          # Compiled binary goes here
├── lib/          # App libraries
└── share/        # App assets
```

## 2. Edit the Manifest

Open `main.cpp` and update the manifest:

```cpp
NAH_APP_MANIFEST(
    nah::manifest()
        .id("com.yourcompany.myapp")    // Your app ID
        .version("1.0.0")                // Your version
        .nak_id("com.example.sdk")       // NAK you depend on
        .nak_version_req("^1.0.0")       // Version requirement
        .entrypoint("bin/myapp")         // Path to binary
        .lib_dir("lib")
        .asset_dir("share")
        .build()
);
```

### Version Requirements

| Format   | Example  | Meaning                      |
|----------|----------|------------------------------|
| Caret    | `^1.2.0` | Compatible with 1.x (>=1.2.0 <2.0.0) |
| Tilde    | `~1.2.0` | Patch updates only (>=1.2.0 <1.3.0) |
| Exact    | `1.2.0`  | Exactly this version         |
| Wildcard | `1.2.*`  | Any 1.2.x                    |

## 3. Build Your App

Build with your toolchain:

```bash
mkdir build && cd build
cmake ..
make
```

Ensure the binary is at `bin/myapp` (matching your entrypoint).

## 4. Package as NAP

```bash
nah app pack . -o myapp-1.0.0.nap
```

This creates a NAP (Native App Package) archive.

## 5. Test Installation

Install into a NAH root:

```bash
nah --root /path/to/nah app install myapp-1.0.0.nap
```

## 6. Verify

```bash
nah --root /path/to/nah doctor com.yourcompany.myapp
```

A clean output means your app is correctly configured.

## 7. View Launch Contract

```bash
nah --root /path/to/nah contract show com.yourcompany.myapp
```

This shows how your app will be launched: binary path, library paths, environment variables.

## Alternative: Standalone Manifest

Instead of embedding the manifest in code, you can use a separate `manifest.toml`:

```toml
id = "com.yourcompany.myapp"
version = "1.0.0"
nak_id = "com.example.sdk"
nak_version_req = "^1.0.0"
entrypoint = "bin/myapp"
lib_dirs = ["lib"]
asset_dirs = ["share"]
```

Generate the binary manifest:

```bash
nah manifest generate manifest.toml -o manifest.nah
```

Include `manifest.nah` in your NAP package root.

## Next Steps

- See `examples/apps/` for complete working examples
- Read SPEC.md for the full manifest format
