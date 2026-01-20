# NAH Documentation

## Concepts

Start here to understand NAH terminology:

- [Core Concepts](concepts.md) - NAK, NAP, Host Profile, Launch Contract, and how they fit together

## Getting Started

Choose based on your role:

| Guide                                           | You are...                                    |
| ----------------------------------------------- | --------------------------------------------- |
| [Host Integrator](getting-started-host.md)      | Running apps and SDKs from vendors            |
| [NAK Developer](getting-started-nak.md)         | Building an SDK or framework                  |
| [App Developer](getting-started-app.md)         | Building a native app that needs an SDK       |
| [Bundle App Developer](getting-started-bundle.md) | Building a JS/Python app with a runtime NAK |

## Reference

| Document                                | Description                      |
| --------------------------------------- | -------------------------------- |
| [CLI Reference](cli.md)                 | Command-line tool documentation  |
| [API Reference](https://nah.rtorr.com/) | Library documentation (Doxygen)  |
| [SPEC.md](../SPEC.md)                   | Complete normative specification |

## Library Headers

NAH is a header-only C++ library.

### Which Header Should I Use?

**Start here:** Most users should just use `nah/nah.h`. It includes everything.

```
┌─────────────────────────────────────────────────────────────────┐
│  nah/nah.h  (USE THIS)                                          │
│  Everything: NahHost class + all components                     │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────┐  │
│  │ nah_core.h  │ nah_json.h  │ nah_fs.h    │ nah_exec.h      │  │
│  │ Pure logic  │ JSON parse  │ File I/O    │ Execute         │  │
│  │ No deps     │ nlohmann    │ C++17 fs    │ Platform        │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────┘  │
│          + nah_overrides.h  + nah_host.h (NahHost class)        │
└─────────────────────────────────────────────────────────────────┘
```

| Your situation | Use this header |
| -------------- | --------------- |
| Most users | `nah/nah.h` |
| Embedding in constrained environment (no filesystem) | `nah/nah_core.h` |
| Only need JSON parsing or only need execution | Individual headers |

Use individual headers (`nah_core.h`, `nah_json.h`, etc.) only when you need finer control over dependencies or want to replace a component with a custom implementation.

---

### nah/nah_host.h

High-level API for hosts to integrate NAH. Includes all other headers and provides the `NahHost` class for managing a NAH root directory.

**When to use:** You're building a launcher, app manager, or any system that needs to run NAH applications.

```cpp
#define NAH_HOST_IMPLEMENTATION
#include <nah/nah_host.h>

// Create a host instance (uses $NAH_ROOT or /nah by default)
auto host = nah::host::NahHost::create();

// List installed applications
for (const auto& app : host->listApplications()) {
    std::cout << app.id << "@" << app.version << "\n";
}

// Get a launch contract
auto result = host->getLaunchContract("com.example.app");
if (result.ok) {
    // Inspect or modify contract before execution
    std::cout << "Binary: " << result.contract.execution.binary << "\n";
}

// Execute an application directly
int exit_code = host->executeApplication("com.example.app");
```

**Key methods:**
- `NahHost::create(root)` - Create a host instance for a NAH root directory
- `listApplications()` - List all installed applications
- `findApplication(id, version)` - Find an app by ID and optional version
- `getLaunchContract(id, version, trace)` - Generate a launch contract
- `executeApplication(id, version, args, handler)` - Compose and run an app
- `executeContract(contract, args, handler)` - Execute a pre-composed contract
- `getInventory()` - Get inventory of installed NAKs
- `validateRoot()` - Validate NAH root structure

**Convenience functions:**
- `nah::host::quickExecute(app_id, root)` - One-liner to run an app
- `nah::host::listInstalledApps(root)` - Get app IDs as strings

---

### nah/nah.h

All-in-one include that provides the complete NAH library. Use this when you need full pipeline control.

**When to use:** You need to customize the composition pipeline or handle files/execution yourself.

```cpp
#include <nah/nah.h>

// Read and parse inputs
auto app_json = nah::fs::read_file("nah.json");
auto app = nah::json::parse_app_declaration(*app_json);

auto host_json = nah::fs::read_file("/nah/host/host.json");
auto host_env = nah::json::parse_host_environment(*host_json);

auto install_json = nah::fs::read_file("/nah/registry/apps/myapp.json");
auto install = nah::json::parse_install_record(*install_json);

// Load runtime inventory
auto inventory = nah::fs::load_inventory_from_directory("/nah/registry/naks");

// Compose the launch contract
auto result = nah::core::nah_compose(
    app.value, host_env.value, install.value, inventory);

// Execute
if (result.ok) {
    nah::exec::execute(result.contract);
}
```

---

### nah/nah_core.h

Pure composition engine with no I/O or side effects. All functions are deterministic.

**When to use:** Embedding NAH in constrained environments, testing, or when you need to implement custom I/O.

```cpp
#include <nah/nah_core.h>
using namespace nah::core;

// Build inputs programmatically (no file I/O)
AppDeclaration app;
app.id = "com.example.myapp";
app.version = "1.0.0";
app.entrypoint_path = "main.lua";
app.nak_id = "lua";
app.nak_version_req = ">=5.4.0";

InstallRecord install;
install.install.instance_id = "abc123";
install.paths.install_root = "/apps/myapp";
install.nak.record_ref = "lua@5.4.6.json";

HostEnvironment host_env;
host_env.vars["LOG_LEVEL"] = "info";

RuntimeInventory inventory;
// ... populate from your own data source

// Compose (pure function, no side effects)
CompositionResult result = nah_compose(app, host_env, install, inventory);

if (result.ok) {
    // Use result.contract with your own execution logic
}
```

**Key types:**
- `AppDeclaration` - What the app needs (id, version, entrypoint, runtime)
- `HostEnvironment` - Host-provided environment and policy
- `InstallRecord` - Where the app is installed and which runtime to use
- `RuntimeInventory` - Available runtimes on the host
- `LaunchContract` - Complete execution specification (output)
- `CompositionResult` - Result with contract, warnings, and optional trace

**Key functions:**
- `nah_compose()` - Compose inputs into a launch contract
- `validate_declaration()` - Validate an app declaration
- `validate_install_record()` - Validate an install record
- `expand_placeholders()` - Expand {VAR} placeholders in strings

---

### nah/nah_json.h

JSON parsing and serialization for all NAH types. Requires nlohmann/json.

**When to use:** You're using `nah_core.h` and need to parse JSON manifests.

```cpp
#include <nah/nah_core.h>
#include <nah/nah_json.h>

std::string json_str = R"({
    "id": "com.example.app",
    "version": "1.0.0",
    "entrypoint": "main.lua",
    "nak": { "id": "lua", "version_req": ">=5.4" }
})";

auto result = nah::json::parse_app_declaration(json_str);
if (result.ok) {
    // Use result.value
}
```

**Parse functions:**
- `parse_app_declaration(json_str)` - Parse app manifest
- `parse_host_environment(json_str, source_path)` - Parse host.json
- `parse_install_record(json_str, source_path)` - Parse install record
- `parse_runtime_descriptor(json_str, source_path)` - Parse NAK descriptor
- `parse_launch_contract(json_str)` - Parse cached contract

**Serialization:**
- `serialize_contract(contract)` - Contract to JSON string
- `serialize_result(result)` - Full result to JSON string

---

### nah/nah_fs.h

Filesystem operations for reading manifests and loading inventories.

**When to use:** You need file operations alongside `nah_core.h` and `nah_json.h`.

```cpp
#include <nah/nah_fs.h>

// Read a file
auto content = nah::fs::read_file("/path/to/file.json");
if (content) {
    // Use *content
}

// Check existence
if (nah::fs::exists("/nah/apps/myapp")) {
    // ...
}

// Load entire NAK inventory from a directory
std::vector<std::string> errors;
auto inventory = nah::fs::load_inventory_from_directory(
    "/nah/registry/naks", &errors);
```

**Key functions:**
- `read_file(path)` - Read file contents as string
- `write_file(path, content)` - Write string to file
- `exists(path)` - Check if path exists
- `is_file(path)` / `is_directory(path)` - Check path type
- `list_directory(path)` - List directory entries
- `load_inventory_from_directory(path, errors)` - Load RuntimeInventory

---

### nah/nah_exec.h

Cross-platform process execution for launch contracts.

**When to use:** You have a `LaunchContract` and want to execute it.

```cpp
#include <nah/nah_exec.h>

// After composing a contract...
if (result.ok) {
    // Spawn process and wait for exit
    auto exec_result = nah::exec::execute(result.contract);
    if (exec_result.ok) {
        return exec_result.exit_code;
    }
    
    // Or replace current process (Unix-style exec)
    nah::exec::exec_replace(result.contract);  // Does not return on success
}
```

**Key functions:**
- `execute(contract, wait)` - Spawn process, optionally wait for exit
- `exec_replace(contract)` - Replace current process (Unix) or spawn and exit (Windows)
- `build_argv(contract)` - Build argument vector
- `build_environment(contract)` - Build environment array

---

### nah/nah_overrides.h

Handles `NAH_OVERRIDE_*` environment variables for runtime configuration.

**When to use:** You want to allow users to override environment variables at launch time.

```cpp
#include <nah/nah_overrides.h>

// After composing...
if (result.ok) {
    // Apply NAH_OVERRIDE_ENVIRONMENT from process environment
    nah::overrides::apply_overrides(result, host_env);
    
    // Or from a custom environment map
    std::unordered_map<std::string, std::string> env = {
        {"NAH_OVERRIDE_ENVIRONMENT", R"({"DEBUG": "1"})"}
    };
    nah::overrides::apply_overrides(result, host_env, env);
}
```

**Key functions:**
- `parse_env_override()` - Parse NAH_OVERRIDE_ENVIRONMENT
- `apply_overrides(result, host_env)` - Apply overrides to composition result
- `is_key_allowed(key, host_env)` - Check if override key is permitted

## Planned

| Document | Description |
| -------- | ----------- |
| [Android Support](planned-android.md) | How NAH could work on Android |

## Examples

See [examples/](../examples/) for working code:

- `examples/sdk/` - NAK without dependencies
- `examples/conan-sdk/` - NAK with Conan dependencies
- `examples/apps/` - Sample applications
- `examples/host/` - Host profile examples
