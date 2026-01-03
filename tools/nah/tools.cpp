#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <toml++/toml.h>

#include "nah/nahhost.hpp"
#include "nah/manifest.hpp"
#include "nah/manifest_builder.hpp"
#include "nah/platform.hpp"
#include "nah/contract.hpp"
#include "nah/packaging.hpp"
#include "nah/nak_selection.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

// ============================================================================
// Global Options
// ============================================================================

struct GlobalOptions {
    std::string root = "/nah";
    std::string profile;
    bool json = false;
    bool trace = false;
    bool verbose = false;
    bool quiet = false;
};

// ============================================================================
// Helper Functions
// ============================================================================

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void print_error(const std::string& msg, bool json_mode) {
    if (json_mode) {
        nlohmann::json j;
        j["error"] = msg;
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cerr << "error: " << msg << std::endl;
    }
}

// ============================================================================
// App Commands
// ============================================================================

int cmd_app_list(const GlobalOptions& opts) {
    auto host = nah::NahHost::create(opts.root);
    auto apps = host->listApplications();
    
    if (opts.json) {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& app : apps) {
            nlohmann::json a;
            a["id"] = app.id;
            a["version"] = app.version;
            a["instance_id"] = app.instance_id;
            a["install_root"] = app.install_root;
            j.push_back(a);
        }
        std::cout << j.dump(2) << std::endl;
    } else {
        if (apps.empty()) {
            std::cout << "No applications installed." << std::endl;
        } else {
            for (const auto& app : apps) {
                std::cout << app.id << "@" << app.version 
                          << " (" << app.install_root << ")" << std::endl;
            }
        }
    }
    
    return 0;
}

int cmd_app_show(const GlobalOptions& opts, const std::string& target) {
    auto host = nah::NahHost::create(opts.root);
    
    // Parse target: id[@version]
    std::string id = target;
    std::string version;
    auto at_pos = target.find('@');
    if (at_pos != std::string::npos) {
        id = target.substr(0, at_pos);
        version = target.substr(at_pos + 1);
    }
    
    auto result = host->findApplication(id, version);
    if (result.isErr()) {
        print_error(result.error().message(), opts.json);
        return 1;
    }
    
    const auto& app = result.value();
    
    if (opts.json) {
        nlohmann::json j;
        j["id"] = app.id;
        j["version"] = app.version;
        j["instance_id"] = app.instance_id;
        j["install_root"] = app.install_root;
        j["record_path"] = app.record_path;
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cout << "Application: " << app.id << " v" << app.version << std::endl;
        std::cout << "Instance ID: " << app.instance_id << std::endl;
        std::cout << "Install Root: " << app.install_root << std::endl;
        std::cout << "Record: " << app.record_path << std::endl;
    }
    
    return 0;
}

int cmd_app_install(const GlobalOptions& opts, const std::string& package_path,
                     bool force) {
    nah::AppInstallOptions install_opts;
    install_opts.nah_root = opts.root;
    install_opts.profile_name = opts.profile;
    install_opts.force = force;
    
    auto result = nah::install_nap_package(package_path, install_opts);
    
    if (!result.ok) {
        print_error(result.error, opts.json);
        return 1;
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["success"] = true;
        j["install_root"] = result.install_root;
        j["record_path"] = result.record_path;
        j["instance_id"] = result.instance_id;
        j["nak_id"] = result.nak_id;
        j["nak_version"] = result.nak_version;
        std::cout << j.dump(2) << std::endl;
    } else if (!opts.quiet) {
        std::cout << "Installed to: " << result.install_root << std::endl;
        std::cout << "Instance ID: " << result.instance_id << std::endl;
        if (!result.nak_id.empty()) {
            std::cout << "NAK: " << result.nak_id << "@" << result.nak_version << std::endl;
        }
    }
    
    return 0;
}

int cmd_app_uninstall(const GlobalOptions& opts, const std::string& target) {
    // Parse target: id[@version]
    std::string id = target;
    std::string version;
    auto at_pos = target.find('@');
    if (at_pos != std::string::npos) {
        id = target.substr(0, at_pos);
        version = target.substr(at_pos + 1);
    }
    
    auto result = nah::uninstall_app(opts.root, id, version);
    
    if (!result.ok) {
        print_error(result.error, opts.json);
        return 1;
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["success"] = true;
        j["uninstalled"] = target;
        std::cout << j.dump(2) << std::endl;
    } else if (!opts.quiet) {
        std::cout << "Uninstalled: " << target << std::endl;
    }
    
    return 0;
}

int cmd_app_verify(const GlobalOptions& opts, const std::string& target) {
    // Parse target: id[@version]
    std::string id = target;
    std::string version;
    auto at_pos = target.find('@');
    if (at_pos != std::string::npos) {
        id = target.substr(0, at_pos);
        version = target.substr(at_pos + 1);
    }
    
    auto result = nah::verify_app(opts.root, id, version);
    
    if (opts.json) {
        nlohmann::json j;
        j["valid"] = result.ok;
        j["manifest_valid"] = result.manifest_valid;
        j["structure_valid"] = result.structure_valid;
        j["nak_available"] = result.nak_available;
        j["issues"] = result.issues;
        if (!result.ok) {
            j["error"] = result.error;
        }
        std::cout << j.dump(2) << std::endl;
    } else {
        if (result.ok) {
            std::cout << target << ": OK" << std::endl;
        } else {
            std::cout << target << ": FAILED" << std::endl;
            for (const auto& issue : result.issues) {
                std::cout << "  - " << issue << std::endl;
            }
        }
    }
    
    return result.ok ? 0 : 1;
}

int cmd_app_init(const GlobalOptions& opts, const std::string& dir) {
    // Create minimal app skeleton
    fs::create_directories(dir + "/bin");
    fs::create_directories(dir + "/lib");
    fs::create_directories(dir + "/share");
    
    // Create a sample main.cpp with manifest
    std::string main_cpp = R"cpp(#include <iostream>
#include <nah/manifest_builder.hpp>

// Embed NAH manifest
NAH_APP_MANIFEST(
    nah::manifest()
        .id("com.example.myapp")
        .version("1.0.0")
        .nak_id("com.example.nak")
        .nak_version_req("^1.0.0")
        .entrypoint("bin/myapp")
        .lib_dir("lib")
        .asset_dir("share")
        .build()
);

int main(int argc, char* argv[]) {
    std::cout << "Hello from NAH application!" << std::endl;
    return 0;
}
)cpp";
    
    std::ofstream file(dir + "/main.cpp");
    file << main_cpp;
    file.close();
    
    // Create README.md
    std::string readme = R"(# NAH Application

This is a NAH application skeleton.

## Next Steps

1. Edit `main.cpp` to update the manifest:
   - `id`: Your app's unique identifier (e.g., `com.yourcompany.myapp`)
   - `version`: Your app's version
   - `nak_id`: The NAK (SDK) your app depends on
   - `nak_version_req`: Version requirement (e.g., `^1.0.0`)
   - `entrypoint`: Path to your compiled binary

2. Build your application:
   ```bash
   # Your build commands here
   # Ensure binary is placed at bin/myapp
   ```

3. Package as NAP:
   ```bash
   nah app pack . -o myapp-1.0.0.nap
   ```

4. Install and test:
   ```bash
   nah --root /path/to/nah app install myapp-1.0.0.nap
   nah --root /path/to/nah doctor com.example.myapp
   ```

## Documentation

See `docs/getting-started-app.md` for the full guide.
)";
    
    std::ofstream readme_file(dir + "/README.md");
    readme_file << readme;
    readme_file.close();
    
    if (!opts.quiet) {
        std::cout << "Created app skeleton in " << dir << std::endl;
        std::cout << "Files created:" << std::endl;
        std::cout << "  " << dir << "/main.cpp" << std::endl;
        std::cout << "  " << dir << "/bin/" << std::endl;
        std::cout << "  " << dir << "/lib/" << std::endl;
        std::cout << "  " << dir << "/share/" << std::endl;
        std::cout << "  " << dir << "/README.md" << std::endl;
    }
    
    return 0;
}

int cmd_app_pack(const GlobalOptions& opts, const std::string& dir, const std::string& output) {
    auto result = nah::pack_nap(dir);
    
    if (!result.ok) {
        print_error(result.error, opts.json);
        return 1;
    }
    
    // Write to output file
    std::ofstream file(output, std::ios::binary);
    if (!file) {
        print_error("failed to create output file: " + output, opts.json);
        return 1;
    }
    
    file.write(reinterpret_cast<const char*>(result.archive_data.data()),
               static_cast<std::streamsize>(result.archive_data.size()));
    file.close();
    
    if (opts.json) {
        nlohmann::json j;
        j["success"] = true;
        j["output"] = output;
        j["size"] = result.archive_data.size();
        std::cout << j.dump(2) << std::endl;
    } else if (!opts.quiet) {
        std::cout << "Created: " << output << " (" << result.archive_data.size() << " bytes)" << std::endl;
    }
    
    return 0;
}

// ============================================================================
// NAK Commands
// ============================================================================

int cmd_nak_list(const GlobalOptions& opts) {
    auto entries = nah::scan_nak_registry(opts.root);
    
    if (opts.json) {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& entry : entries) {
            nlohmann::json e;
            e["id"] = entry.id;
            e["version"] = entry.version;
            e["record_ref"] = entry.record_ref;
            j.push_back(e);
        }
        std::cout << j.dump(2) << std::endl;
    } else {
        if (entries.empty()) {
            std::cout << "No NAKs installed." << std::endl;
        } else {
            for (const auto& entry : entries) {
                std::cout << entry.id << "@" << entry.version << std::endl;
            }
        }
    }
    
    return 0;
}

int cmd_nak_show(const GlobalOptions& opts, const std::string& target) {
    // Parse target: nak_id[@version]
    std::string id = target;
    std::string version;
    auto at_pos = target.find('@');
    if (at_pos != std::string::npos) {
        id = target.substr(0, at_pos);
        version = target.substr(at_pos + 1);
    }
    
    auto entries = nah::scan_nak_registry(opts.root);
    
    for (const auto& entry : entries) {
        if (entry.id == id && (version.empty() || entry.version == version)) {
            std::string content = read_file(entry.record_path);
            auto result = nah::parse_nak_install_record_full(content, entry.record_path);
            
            if (opts.json) {
                nlohmann::json j;
                j["id"] = result.record.nak.id;
                j["version"] = result.record.nak.version;
                j["root"] = result.record.paths.root;
                j["resource_root"] = result.record.paths.resource_root;
                j["lib_dirs"] = result.record.paths.lib_dirs;
                j["has_loader"] = result.record.loader.present;
                std::cout << j.dump(2) << std::endl;
            } else {
                std::cout << "NAK: " << result.record.nak.id << " v" << result.record.nak.version << std::endl;
                std::cout << "Root: " << result.record.paths.root << std::endl;
                std::cout << "Resource Root: " << result.record.paths.resource_root << std::endl;
                std::cout << "Lib Dirs:" << std::endl;
                for (const auto& lib : result.record.paths.lib_dirs) {
                    std::cout << "  " << lib << std::endl;
                }
                if (result.record.loader.present) {
                    std::cout << "Loader: " << result.record.loader.exec_path << std::endl;
                }
            }
            return 0;
        }
    }
    
    print_error("NAK not found: " + target, opts.json);
    return 1;
}

int cmd_nak_install(const GlobalOptions& opts, const std::string& pack_path,
                     bool force) {
    nah::NakInstallOptions install_opts;
    install_opts.nah_root = opts.root;
    install_opts.force = force;
    
    auto result = nah::install_nak_pack(pack_path, install_opts);
    
    if (!result.ok) {
        print_error(result.error, opts.json);
        return 1;
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["success"] = true;
        j["install_root"] = result.install_root;
        j["record_path"] = result.record_path;
        std::cout << j.dump(2) << std::endl;
    } else if (!opts.quiet) {
        std::cout << "Installed to: " << result.install_root << std::endl;
    }
    
    return 0;
}

int cmd_nak_path(const GlobalOptions& opts, const std::string& target) {
    // Parse target: nak_id@version
    auto at_pos = target.find('@');
    if (at_pos == std::string::npos) {
        print_error("target must be nak_id@version", opts.json);
        return 1;
    }
    
    std::string id = target.substr(0, at_pos);
    std::string version = target.substr(at_pos + 1);
    
    auto entries = nah::scan_nak_registry(opts.root);
    
    for (const auto& entry : entries) {
        if (entry.id == id && entry.version == version) {
            std::string content = read_file(entry.record_path);
            auto result = nah::parse_nak_install_record_full(content, entry.record_path);
            std::cout << result.record.paths.root << std::endl;
            return 0;
        }
    }
    
    print_error("NAK not found: " + target, opts.json);
    return 1;
}

int cmd_nak_init(const GlobalOptions& opts, const std::string& dir) {
    fs::create_directories(dir + "/META");
    fs::create_directories(dir + "/lib");
    fs::create_directories(dir + "/resources");
    fs::create_directories(dir + "/bin");
    
    std::string nak_toml = R"toml(schema = "nah.nak.pack.v1"

[nak]
id = "com.example.nak"
version = "1.0.0"

[paths]
resource_root = "resources"
lib_dirs = ["lib"]

[environment]
# NAH_NAK_FLAG = "1"

# Optional loader configuration
# [loader]
# exec_path = "bin/nah-runtime"
# args_template = ["--app", "{NAH_APP_ENTRY}"]

[execution]
cwd = "{NAH_APP_ROOT}"
)toml";
    
    std::ofstream file(dir + "/META/nak.toml");
    file << nak_toml;
    file.close();
    
    // Create README.md
    std::string readme = R"(# NAH NAK (Native App Kit)

This is a NAK skeleton for building an SDK or framework.

## Next Steps

1. Edit `META/nak.toml` to update:
   - `nak.id`: Your NAK's unique identifier (e.g., `com.yourcompany.mysdk`)
   - `nak.version`: Your NAK's version
   - `paths.lib_dirs`: Directories containing shared libraries

2. Add your libraries to `lib/`:
   - `lib/libmysdk.so` (Linux)
   - `lib/libmysdk.dylib` (macOS)

3. Optional: Add a loader binary to `bin/` and configure `[loader]`

4. Package as NAK:
   ```bash
   nah nak pack . -o mysdk-1.0.0.nak
   ```

5. Install and test:
   ```bash
   nah --root /path/to/nah nak install mysdk-1.0.0.nak
   nah --root /path/to/nah nak list
   ```

## Documentation

See `docs/getting-started-nak.md` for the full guide.
)";
    
    std::ofstream readme_file(dir + "/README.md");
    readme_file << readme;
    readme_file.close();
    
    if (!opts.quiet) {
        std::cout << "Created NAK skeleton in " << dir << std::endl;
        std::cout << "Files created:" << std::endl;
        std::cout << "  " << dir << "/META/nak.toml" << std::endl;
        std::cout << "  " << dir << "/bin/" << std::endl;
        std::cout << "  " << dir << "/lib/" << std::endl;
        std::cout << "  " << dir << "/resources/" << std::endl;
        std::cout << "  " << dir << "/README.md" << std::endl;
    }
    
    return 0;
}

int cmd_nak_pack(const GlobalOptions& opts, const std::string& dir, const std::string& output) {
    auto result = nah::pack_nak(dir);
    
    if (!result.ok) {
        print_error(result.error, opts.json);
        return 1;
    }
    
    // Write to output file
    std::ofstream file(output, std::ios::binary);
    if (!file) {
        print_error("failed to create output file: " + output, opts.json);
        return 1;
    }
    
    file.write(reinterpret_cast<const char*>(result.archive_data.data()),
               static_cast<std::streamsize>(result.archive_data.size()));
    file.close();
    
    if (opts.json) {
        nlohmann::json j;
        j["success"] = true;
        j["output"] = output;
        j["size"] = result.archive_data.size();
        std::cout << j.dump(2) << std::endl;
    } else if (!opts.quiet) {
        std::cout << "Created: " << output << " (" << result.archive_data.size() << " bytes)" << std::endl;
    }
    
    return 0;
}

// ============================================================================
// Profile Commands
// ============================================================================

int cmd_profile_init(const GlobalOptions& opts, const std::string& dir) {
    fs::path root_path(dir);
    
    // Check if host/ already exists
    if (fs::exists(root_path / "host")) {
        print_error("directory already contains host/: " + dir, opts.json);
        return 1;
    }
    
    // Create directory structure
    std::error_code ec;
    fs::create_directories(root_path / "host" / "profiles", ec);
    if (ec) {
        print_error("failed to create host/profiles: " + ec.message(), opts.json);
        return 1;
    }
    
    fs::create_directories(root_path / "apps", ec);
    fs::create_directories(root_path / "naks", ec);
    fs::create_directories(root_path / "registry" / "installs", ec);
    fs::create_directories(root_path / "registry" / "naks", ec);
    
    // Create default.toml
    std::string default_profile = R"(schema = "nah.host.profile.v1"

[nak]
binding_mode = "canonical"

[environment]
# Add environment variables for your deployment
# NAH_HOST_VERSION = "1.0"

[warnings]
# Override warning actions: "warn", "ignore", or "error"
# nak_not_found = "error"

[capabilities]
# Map app capabilities to host enforcement
# "filesystem.read" = "sandbox.readonly"
# "network.connect" = "network.outbound"
)";
    
    std::ofstream profile_file(root_path / "host" / "profiles" / "default.toml");
    if (!profile_file) {
        print_error("failed to write default.toml", opts.json);
        return 1;
    }
    profile_file << default_profile;
    profile_file.close();
    
    // Create profile.current symlink
    fs::path symlink_path = root_path / "host" / "profile.current";
    fs::create_symlink("profiles/default.toml", symlink_path, ec);
    if (ec) {
        print_error("failed to create profile.current symlink: " + ec.message(), opts.json);
        return 1;
    }
    
    // Create README.md
    std::string readme = R"(# NAH Root

This directory is a NAH (Native Application Host) root.

## Structure

```
├── host/
│   ├── profiles/
│   │   └── default.toml    # Host profile configuration
│   └── profile.current     # Symlink to active profile
├── apps/                   # Installed applications
├── naks/                   # Installed NAK packs
└── registry/
    ├── installs/           # App install records
    └── naks/               # NAK install records
```

## Next Steps

1. Edit `host/profiles/default.toml` for your environment
2. Install NAKs: `nah --root )" + dir + R"( nak install <pack.nak>`
3. Install apps: `nah --root )" + dir + R"( app install <app.nap>`
4. Validate: `nah --root )" + dir + R"( doctor <app_id>`

## Documentation

See `docs/getting-started-host.md` for the full host integrator guide.
)";
    
    std::ofstream readme_file(root_path / "README.md");
    if (readme_file) {
        readme_file << readme;
        readme_file.close();
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["created"] = dir;
        j["profile"] = "default";
        std::cout << j.dump(2) << std::endl;
    } else if (!opts.quiet) {
        std::cout << "Created NAH root in " << dir << std::endl;
        std::cout << "Files created:" << std::endl;
        std::cout << "  " << dir << "/host/profiles/default.toml" << std::endl;
        std::cout << "  " << dir << "/host/profile.current -> profiles/default.toml" << std::endl;
        std::cout << "  " << dir << "/apps/" << std::endl;
        std::cout << "  " << dir << "/naks/" << std::endl;
        std::cout << "  " << dir << "/registry/installs/" << std::endl;
        std::cout << "  " << dir << "/registry/naks/" << std::endl;
        std::cout << "  " << dir << "/README.md" << std::endl;
    }
    
    return 0;
}

int cmd_profile_list(const GlobalOptions& opts) {
    auto host = nah::NahHost::create(opts.root);
    auto profiles = host->listProfiles();
    
    if (opts.json) {
        nlohmann::json j = profiles;
        std::cout << j.dump(2) << std::endl;
    } else {
        if (profiles.empty()) {
            std::cout << "No profiles found." << std::endl;
        } else {
            for (const auto& p : profiles) {
                std::cout << p << std::endl;
            }
        }
    }
    
    return 0;
}

int cmd_profile_show(const GlobalOptions& opts, const std::string& name) {
    auto host = nah::NahHost::create(opts.root);
    
    nah::Result<nah::HostProfile> result = name.empty() 
        ? host->getActiveHostProfile()
        : host->loadProfile(name);
    
    if (result.isErr()) {
        print_error(result.error().message(), opts.json);
        return 1;
    }
    
    const auto& profile = result.value();
    
    if (opts.json) {
        nlohmann::json j;
        j["schema"] = profile.schema;
        j["binding_mode"] = nah::binding_mode_to_string(profile.nak.binding_mode);
        j["allow_versions"] = profile.nak.allow_versions;
        j["deny_versions"] = profile.nak.deny_versions;
        
        nlohmann::ordered_json env;
        std::vector<std::string> env_keys;
        for (const auto& [k, _] : profile.environment) env_keys.push_back(k);
        std::sort(env_keys.begin(), env_keys.end());
        for (const auto& k : env_keys) env[k] = profile.environment.at(k);
        j["environment"] = env;
        
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cout << "Schema: " << profile.schema << std::endl;
        std::cout << "Binding Mode: " << nah::binding_mode_to_string(profile.nak.binding_mode) << std::endl;
        if (!profile.nak.allow_versions.empty()) {
            std::cout << "Allow Versions: ";
            for (const auto& v : profile.nak.allow_versions) std::cout << v << " ";
            std::cout << std::endl;
        }
        if (!profile.environment.empty()) {
            std::cout << "Environment:" << std::endl;
            for (const auto& [k, v] : profile.environment) {
                std::cout << "  " << k << "=" << v << std::endl;
            }
        }
    }
    
    return 0;
}

int cmd_profile_set(const GlobalOptions& opts, const std::string& name) {
    auto host = nah::NahHost::create(opts.root);
    auto result = host->setActiveHostProfile(name);
    
    if (result.isErr()) {
        print_error(result.error().message(), opts.json);
        return 1;
    }
    
    if (!opts.quiet) {
        std::cout << "Active profile set to: " << name << std::endl;
    }
    
    return 0;
}

int cmd_profile_validate(const GlobalOptions& opts, const std::string& path) {
    std::string content = read_file(path);
    if (content.empty()) {
        print_error("failed to read file: " + path, opts.json);
        return 1;
    }
    
    auto result = nah::parse_host_profile_full(content, path);
    
    if (opts.json) {
        nlohmann::json j;
        j["valid"] = result.ok;
        if (!result.ok) {
            j["error"] = result.error;
        }
        j["warnings"] = result.warnings;
        std::cout << j.dump(2) << std::endl;
    } else {
        if (result.ok) {
            std::cout << "Profile is valid." << std::endl;
        } else {
            std::cout << "Profile is invalid: " << result.error << std::endl;
        }
        for (const auto& w : result.warnings) {
            std::cout << "  warning: " << w << std::endl;
        }
    }
    
    return result.ok ? 0 : 1;
}

// ============================================================================
// Contract Commands
// ============================================================================

int cmd_contract_show(const GlobalOptions& opts, const std::string& target,
                       const std::string& /*overrides_file*/) {
    auto host = nah::NahHost::create(opts.root);
    
    // Parse target: id[@version]
    std::string id = target;
    std::string version;
    auto at_pos = target.find('@');
    if (at_pos != std::string::npos) {
        id = target.substr(0, at_pos);
        version = target.substr(at_pos + 1);
    }
    
    auto result = host->getLaunchContract(id, version, opts.profile, opts.trace);
    
    if (result.isErr()) {
        if (opts.json) {
            nlohmann::json j;
            j["schema"] = "nah.launch.contract.v1";
            j["critical_error"] = result.error().message();
            j["warnings"] = nlohmann::json::array();
            std::cout << j.dump(2) << std::endl;
        } else {
            std::cerr << "Critical error: " << result.error().message() << std::endl;
        }
        return 1;
    }
    
    const auto& envelope = result.value();
    
    if (opts.json) {
        std::cout << nah::serialize_contract_json(envelope, opts.trace, std::nullopt) << std::endl;
    } else {
        const auto& c = envelope.contract;
        
        std::cout << "Application: " << c.app.id << " v" << c.app.version << std::endl;
        if (!c.nak.id.empty()) {
            std::cout << "NAK: " << c.nak.id << " v" << c.nak.version << std::endl;
        }
        std::cout << "Binary: " << c.execution.binary << std::endl;
        std::cout << "CWD: " << c.execution.cwd << std::endl;
        
        if (!c.execution.arguments.empty()) {
            std::cout << "Arguments:" << std::endl;
            for (const auto& arg : c.execution.arguments) {
                std::cout << "  " << arg << std::endl;
            }
        }
        
        std::cout << std::endl << "Library Paths (" << c.execution.library_path_env_key << "):" << std::endl;
        for (const auto& p : c.execution.library_paths) {
            std::cout << "  " << p << std::endl;
        }
        
        std::cout << std::endl << "Environment (selected):" << std::endl;
        std::vector<std::string> env_keys;
        for (const auto& [k, _] : c.environment) {
            if (k.rfind("NAH_", 0) == 0) env_keys.push_back(k);
        }
        std::sort(env_keys.begin(), env_keys.end());
        for (const auto& k : env_keys) {
            std::cout << "  " << k << "=" << c.environment.at(k) << std::endl;
        }
        
        if (!envelope.warnings.empty()) {
            std::cout << std::endl << "Warnings:" << std::endl;
            for (const auto& w : envelope.warnings) {
                std::cout << "  [" << w.action << "] " << w.key << std::endl;
            }
        }
    }
    
    // Exit codes per SPEC L1972-L1983
    bool has_errors = false;
    bool has_warnings = false;
    for (const auto& w : envelope.warnings) {
        if (w.action == "error") has_errors = true;
        if (w.action == "warn") has_warnings = true;
    }
    
    if (has_errors) return 1;
    if (has_warnings) return 2;
    return 0;
}

int cmd_contract_explain(const GlobalOptions& opts, const std::string& target, 
                          const std::string& path) {
    // Parse target: id[@version]
    std::string id = target;
    std::string version;
    auto at_pos = target.find('@');
    if (at_pos != std::string::npos) {
        id = target.substr(0, at_pos);
        version = target.substr(at_pos + 1);
    }
    
    auto host = nah::NahHost::create(opts.root);
    auto result = host->getLaunchContract(id, version, opts.profile, true); // Always get trace for explain
    
    if (result.isErr()) {
        print_error(result.error().message(), opts.json);
        return 1;
    }
    
    const auto& envelope = result.value();
    const auto& c = envelope.contract;
    
    // Parse path: app.id, nak.version, execution.binary, environment.KEY, etc.
    std::string value;
    std::string source_kind = "unknown";
    std::string source_path;
    int precedence_rank = 0;
    bool found = false;
    
    if (path == "app.id") {
        value = c.app.id;
        source_kind = "manifest";
        found = true;
    } else if (path == "app.version") {
        value = c.app.version;
        source_kind = "manifest";
        found = true;
    } else if (path == "app.root") {
        value = c.app.root;
        source_kind = "install_record";
        found = true;
    } else if (path == "app.entrypoint") {
        value = c.app.entrypoint;
        source_kind = "manifest";
        found = true;
    } else if (path == "nak.id") {
        value = c.nak.id;
        source_kind = "nak_record";
        found = true;
    } else if (path == "nak.version") {
        value = c.nak.version;
        source_kind = "nak_record";
        found = true;
    } else if (path == "nak.root") {
        value = c.nak.root;
        source_kind = "nak_record";
        found = true;
    } else if (path == "execution.binary") {
        value = c.execution.binary;
        source_kind = "manifest";
        found = true;
    } else if (path == "execution.cwd") {
        value = c.execution.cwd;
        source_kind = "nak_record";
        found = true;
    } else if (path.rfind("environment.", 0) == 0) {
        std::string env_key = path.substr(12);
        auto it = c.environment.find(env_key);
        if (it != c.environment.end()) {
            value = it->second;
            // Check trace for provenance if available
            if (envelope.trace.has_value()) {
                auto env_trace = envelope.trace->find("environment");
                if (env_trace != envelope.trace->end()) {
                    auto key_trace = env_trace->second.find(env_key);
                    if (key_trace != env_trace->second.end()) {
                        source_kind = key_trace->second.source_kind;
                        source_path = key_trace->second.source_path;
                        precedence_rank = key_trace->second.precedence_rank;
                    }
                }
            }
            found = true;
        }
    }
    
    if (!found) {
        print_error("path not found: " + path, opts.json);
        return 1;
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["path"] = path;
        j["value"] = value;
        j["source_kind"] = source_kind;
        if (!source_path.empty()) j["source_path"] = source_path;
        if (precedence_rank > 0) j["precedence_rank"] = precedence_rank;
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cout << path << " = " << value << std::endl;
        std::cout << "  source: " << source_kind << std::endl;
        if (!source_path.empty()) {
            std::cout << "  path: " << source_path << std::endl;
        }
        if (precedence_rank > 0) {
            std::cout << "  precedence: " << precedence_rank << std::endl;
        }
    }
    
    return 0;
}

int cmd_contract_diff(const GlobalOptions& opts, const std::string& target,
                       const std::string& profile_a, const std::string& profile_b) {
    // Parse target: id[@version]
    std::string id = target;
    std::string version;
    auto at_pos = target.find('@');
    if (at_pos != std::string::npos) {
        id = target.substr(0, at_pos);
        version = target.substr(at_pos + 1);
    }
    
    auto host = nah::NahHost::create(opts.root);
    
    // Get contracts with both profiles
    auto result_a = host->getLaunchContract(id, version, profile_a, opts.trace);
    auto result_b = host->getLaunchContract(id, version, profile_b, opts.trace);
    
    if (result_a.isErr()) {
        print_error("profile A: " + result_a.error().message(), opts.json);
        return 1;
    }
    if (result_b.isErr()) {
        print_error("profile B: " + result_b.error().message(), opts.json);
        return 1;
    }
    
    const auto& c_a = result_a.value().contract;
    const auto& c_b = result_b.value().contract;
    
    std::vector<std::tuple<std::string, std::string, std::string>> diffs;
    
    // Compare execution fields
    if (c_a.execution.binary != c_b.execution.binary) {
        diffs.emplace_back("execution.binary", c_a.execution.binary, c_b.execution.binary);
    }
    if (c_a.execution.cwd != c_b.execution.cwd) {
        diffs.emplace_back("execution.cwd", c_a.execution.cwd, c_b.execution.cwd);
    }
    
    // Compare NAK fields
    if (c_a.nak.id != c_b.nak.id) {
        diffs.emplace_back("nak.id", c_a.nak.id, c_b.nak.id);
    }
    if (c_a.nak.version != c_b.nak.version) {
        diffs.emplace_back("nak.version", c_a.nak.version, c_b.nak.version);
    }
    
    // Compare environment
    std::set<std::string> all_keys;
    for (const auto& [k, _] : c_a.environment) all_keys.insert(k);
    for (const auto& [k, _] : c_b.environment) all_keys.insert(k);
    
    for (const auto& k : all_keys) {
        std::string val_a = c_a.environment.count(k) ? c_a.environment.at(k) : "";
        std::string val_b = c_b.environment.count(k) ? c_b.environment.at(k) : "";
        if (val_a != val_b) {
            diffs.emplace_back("environment." + k, val_a, val_b);
        }
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["profile_a"] = profile_a;
        j["profile_b"] = profile_b;
        j["target"] = target;
        nlohmann::json diff_arr = nlohmann::json::array();
        for (const auto& [path, val_a, val_b] : diffs) {
            nlohmann::json d;
            d["path"] = path;
            d["value_a"] = val_a;
            d["value_b"] = val_b;
            diff_arr.push_back(d);
        }
        j["differences"] = diff_arr;
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cout << "Contract diff for " << target << std::endl;
        std::cout << "  Profile A: " << profile_a << std::endl;
        std::cout << "  Profile B: " << profile_b << std::endl;
        std::cout << std::endl;
        
        if (diffs.empty()) {
            std::cout << "No differences found." << std::endl;
        } else {
            std::cout << "Differences:" << std::endl;
            for (const auto& [path, val_a, val_b] : diffs) {
                std::cout << "  " << path << ":" << std::endl;
                std::cout << "    A: " << val_a << std::endl;
                std::cout << "    B: " << val_b << std::endl;
            }
        }
    }
    
    return diffs.empty() ? 0 : 2;
}

int cmd_contract_resolve(const GlobalOptions& opts, const std::string& target) {
    // Parse target: id[@version]
    std::string id = target;
    std::string version;
    auto at_pos = target.find('@');
    if (at_pos != std::string::npos) {
        id = target.substr(0, at_pos);
        version = target.substr(at_pos + 1);
    }
    
    auto host = nah::NahHost::create(opts.root);
    
    // Find the app
    auto app_result = host->findApplication(id, version);
    if (app_result.isErr()) {
        print_error(app_result.error().message(), opts.json);
        return 1;
    }
    
    const auto& app = app_result.value();
    
    // Read install record to get NAK info
    std::string record_content = read_file(app.record_path);
    auto record_result = nah::parse_app_install_record_full(record_content, app.record_path);
    if (!record_result.ok) {
        print_error("failed to parse install record: " + record_result.error, opts.json);
        return 1;
    }
    
    const auto& record = record_result.record;
    
    // Scan NAK registry for candidates
    auto nak_entries = nah::scan_nak_registry(opts.root);
    
    // Filter by nak_id
    std::vector<nah::NakRegistryEntry> candidates;
    for (const auto& entry : nak_entries) {
        if (entry.id == record.app.nak_id) {
            candidates.push_back(entry);
        }
    }
    
    // Get active profile for allow/deny rules
    auto profile_result = host->getActiveHostProfile();
    nah::HostProfile profile;
    if (profile_result.isOk()) {
        profile = profile_result.value();
    }
    
    // Apply allow/deny filtering
    std::vector<std::pair<nah::NakRegistryEntry, bool>> filter_results;
    for (const auto& entry : candidates) {
        bool allowed = nah::version_allowed_by_profile(entry.version, profile);
        filter_results.emplace_back(entry, allowed);
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["target"] = target;
        j["nak_id"] = record.app.nak_id;
        j["nak_version_req"] = record.app.nak_version_req;
        j["pinned_version"] = record.nak.version;
        j["pinned_record_ref"] = record.nak.record_ref;
        j["selection_reason"] = record.nak.selection_reason;
        
        nlohmann::json cand_arr = nlohmann::json::array();
        for (const auto& [entry, allowed] : filter_results) {
            nlohmann::json c;
            c["id"] = entry.id;
            c["version"] = entry.version;
            c["record_ref"] = entry.record_ref;
            c["allowed_by_profile"] = allowed;
            cand_arr.push_back(c);
        }
        j["candidates"] = cand_arr;
        
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cout << "NAK Resolution for " << target << std::endl;
        std::cout << std::endl;
        std::cout << "Requirement:" << std::endl;
        std::cout << "  NAK ID: " << record.app.nak_id << std::endl;
        std::cout << "  Version Requirement: " << record.app.nak_version_req << std::endl;
        std::cout << std::endl;
        std::cout << "Pinned Selection:" << std::endl;
        std::cout << "  Version: " << record.nak.version << std::endl;
        std::cout << "  Record: " << record.nak.record_ref << std::endl;
        if (!record.nak.selection_reason.empty()) {
            std::cout << "  Reason: " << record.nak.selection_reason << std::endl;
        }
        std::cout << std::endl;
        std::cout << "Registry Candidates (" << candidates.size() << "):" << std::endl;
        for (const auto& [entry, allowed] : filter_results) {
            std::cout << "  " << entry.id << "@" << entry.version;
            if (!allowed) std::cout << " [denied by profile]";
            std::cout << std::endl;
        }
    }
    
    return 0;
}

// ============================================================================
// Manifest Commands
// ============================================================================

int cmd_manifest_generate(const GlobalOptions& opts, const std::string& input_path,
                           const std::string& output_path) {
    // Read TOML file
    std::string toml_content = read_file(input_path);
    if (toml_content.empty()) {
        print_error("failed to read input file: " + input_path, opts.json);
        return 1;
    }
    
    // Parse TOML
    toml::table tbl;
    try {
        tbl = toml::parse(toml_content);
    } catch (const toml::parse_error& e) {
        print_error("TOML parse error: " + std::string(e.what()), opts.json);
        return 1;
    }
    
    // Build manifest using ManifestBuilder
    nah::ManifestBuilder builder;
    
    // Required fields
    if (auto id = tbl["id"].value<std::string>()) {
        builder.id(*id);
    } else {
        print_error("missing required field: id", opts.json);
        return 1;
    }
    
    if (auto version = tbl["version"].value<std::string>()) {
        builder.version(*version);
    } else {
        print_error("missing required field: version", opts.json);
        return 1;
    }
    
    // Optional fields
    if (auto nak_id = tbl["nak_id"].value<std::string>()) {
        builder.nak_id(*nak_id);
    }
    if (auto nak_ver = tbl["nak_version_req"].value<std::string>()) {
        builder.nak_version_req(*nak_ver);
    }
    if (auto entry = tbl["entrypoint"].value<std::string>()) {
        builder.entrypoint(*entry);
    }
    
    // Arrays
    if (auto args = tbl["entrypoint_args"].as_array()) {
        for (const auto& arg : *args) {
            if (auto s = arg.value<std::string>()) {
                builder.entrypoint_arg(*s);
            }
        }
    }
    
    if (auto libs = tbl["lib_dirs"].as_array()) {
        for (const auto& lib : *libs) {
            if (auto s = lib.value<std::string>()) {
                builder.lib_dir(*s);
            }
        }
    }
    
    if (auto assets = tbl["asset_dirs"].as_array()) {
        for (const auto& asset : *assets) {
            if (auto s = asset.value<std::string>()) {
                builder.asset_dir(*s);
            }
        }
    }
    
    // Permissions table
    if (auto perms = tbl["permissions"].as_table()) {
        if (auto fs = perms->get_as<toml::array>("filesystem")) {
            for (const auto& p : *fs) {
                if (auto s = p.value<std::string>()) {
                    builder.filesystem_permission(*s);
                }
            }
        }
        if (auto net = perms->get_as<toml::array>("network")) {
            for (const auto& p : *net) {
                if (auto s = p.value<std::string>()) {
                    builder.network_permission(*s);
                }
            }
        }
    }
    
    // Environment table
    if (auto env = tbl["environment"].as_table()) {
        for (const auto& [key, val] : *env) {
            if (auto s = val.value<std::string>()) {
                builder.env(std::string(key.str()), *s);
            }
        }
    }
    
    // Metadata
    if (auto desc = tbl["description"].value<std::string>()) {
        builder.description(*desc);
    }
    if (auto author = tbl["author"].value<std::string>()) {
        builder.author(*author);
    }
    if (auto license = tbl["license"].value<std::string>()) {
        builder.license(*license);
    }
    if (auto homepage = tbl["homepage"].value<std::string>()) {
        builder.homepage(*homepage);
    }
    
    // Build binary manifest
    std::vector<uint8_t> manifest_data = builder.build();
    
    // Write output
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        print_error("failed to create output file: " + output_path, opts.json);
        return 1;
    }
    
    out.write(reinterpret_cast<const char*>(manifest_data.data()),
              static_cast<std::streamsize>(manifest_data.size()));
    out.close();
    
    if (opts.json) {
        nlohmann::json j;
        j["success"] = true;
        j["input"] = input_path;
        j["output"] = output_path;
        j["size"] = manifest_data.size();
        std::cout << j.dump(2) << std::endl;
    } else if (!opts.quiet) {
        std::cout << "Generated: " << output_path << " (" << manifest_data.size() << " bytes)" << std::endl;
    }
    
    return 0;
}

int cmd_manifest_show(const GlobalOptions& opts, const std::string& target) {
    std::vector<uint8_t> manifest_data;
    
    // Check if target is a binary file or an installed app
    if (fs::exists(target) && fs::is_regular_file(target)) {
        auto result = nah::read_manifest_section(target);
        if (result.ok) {
            manifest_data = result.data;
        } else {
            // Try reading as raw manifest file
            std::ifstream file(target, std::ios::binary);
            if (file) {
                manifest_data = std::vector<uint8_t>(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
            }
        }
    } else {
        // Try as app target
        auto host = nah::NahHost::create(opts.root);
        
        std::string id = target;
        std::string version;
        auto at_pos = target.find('@');
        if (at_pos != std::string::npos) {
            id = target.substr(0, at_pos);
            version = target.substr(at_pos + 1);
        }
        
        auto app_result = host->findApplication(id, version);
        if (app_result.isOk()) {
            std::string manifest_path = app_result.value().install_root + "/manifest.nah";
            std::ifstream file(manifest_path, std::ios::binary);
            if (file) {
                manifest_data = std::vector<uint8_t>(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
            }
        }
    }
    
    if (manifest_data.empty()) {
        print_error("failed to read manifest from: " + target, opts.json);
        return 1;
    }
    
    auto result = nah::parse_manifest(manifest_data);
    
    if (result.critical_missing) {
        print_error("manifest missing or invalid: " + result.error, opts.json);
        return 1;
    }
    
    const auto& m = result.manifest;
    
    if (opts.json) {
        nlohmann::json j;
        j["id"] = m.id;
        j["version"] = m.version;
        j["nak_id"] = m.nak_id;
        if (m.nak_version_req) {
            j["nak_version_req"] = m.nak_version_req->selection_key;
        }
        j["entrypoint"] = m.entrypoint_path;
        j["entrypoint_args"] = m.entrypoint_args;
        j["lib_dirs"] = m.lib_dirs;
        j["asset_dirs"] = m.asset_dirs;
        j["permissions_filesystem"] = m.permissions_filesystem;
        j["permissions_network"] = m.permissions_network;
        j["warnings"] = result.warnings;
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cout << "ID: " << m.id << std::endl;
        std::cout << "Version: " << m.version << std::endl;
        std::cout << "NAK ID: " << m.nak_id << std::endl;
        if (m.nak_version_req) {
            std::cout << "NAK Version Req: " << m.nak_version_req->selection_key << std::endl;
        }
        std::cout << "Entrypoint: " << m.entrypoint_path << std::endl;
        
        if (!m.lib_dirs.empty()) {
            std::cout << "Lib Dirs:" << std::endl;
            for (const auto& d : m.lib_dirs) std::cout << "  " << d << std::endl;
        }
        
        if (!m.permissions_filesystem.empty() || !m.permissions_network.empty()) {
            std::cout << "Permissions:" << std::endl;
            for (const auto& p : m.permissions_filesystem) std::cout << "  " << p << std::endl;
            for (const auto& p : m.permissions_network) std::cout << "  " << p << std::endl;
        }
        
        if (!result.warnings.empty()) {
            std::cout << "Warnings:" << std::endl;
            for (const auto& w : result.warnings) std::cout << "  " << w << std::endl;
        }
    }
    
    return 0;
}

// ============================================================================
// Doctor Command
// ============================================================================

int cmd_doctor(const GlobalOptions& opts, const std::string& target, bool fix) {
    auto host = nah::NahHost::create(opts.root);
    
    struct Issue {
        std::string severity;  // "error", "warning", "info"
        std::string message;
        std::string fix_command;
    };
    std::vector<Issue> issues;
    
    // Check if target is a binary file or an installed app
    bool is_binary = fs::exists(target) && fs::is_regular_file(target);
    
    if (is_binary) {
        // Check manifest in binary
        auto manifest_result = nah::read_manifest_section(target);
        if (!manifest_result.ok) {
            issues.push_back({"error", "no embedded manifest found in binary", ""});
        } else {
            auto parse_result = nah::parse_manifest(manifest_result.data);
            if (!parse_result.ok) {
                issues.push_back({"error", "manifest parse failed: " + parse_result.error, ""});
            } else {
                // Check entrypoint exists
                if (parse_result.manifest.entrypoint_path.empty()) {
                    issues.push_back({"warning", "manifest has no entrypoint defined", ""});
                }
                // Check nak requirement
                if (parse_result.manifest.nak_id.empty()) {
                    issues.push_back({"warning", "manifest has no nak_id defined", ""});
                }
            }
        }
    } else {
        // Target is an installed app id[@version]
        std::string id = target;
        std::string version;
        auto at_pos = target.find('@');
        if (at_pos != std::string::npos) {
            id = target.substr(0, at_pos);
            version = target.substr(at_pos + 1);
        }
        
        auto app_result = host->findApplication(id, version);
        if (app_result.isErr()) {
            issues.push_back({"error", "application not found: " + id, "nah app install <package>"});
        } else {
            const auto& app = app_result.value();
            
            // Verify app
            auto verify_result = nah::verify_app(opts.root, id, version);
            
            if (!verify_result.manifest_valid) {
                issues.push_back({"error", "manifest is invalid or missing", ""});
            }
            if (!verify_result.structure_valid) {
                issues.push_back({"error", "app directory structure is invalid", ""});
            }
            if (!verify_result.nak_available) {
                issues.push_back({"warning", "pinned NAK is not available", "nah nak install <nak-pack>"});
            }
            
            for (const auto& issue : verify_result.issues) {
                issues.push_back({"warning", issue, ""});
            }
            
            // Check install record
            std::string record_content = read_file(app.record_path);
            if (record_content.empty()) {
                issues.push_back({"error", "install record missing or unreadable", ""});
            } else {
                auto record_result = nah::parse_app_install_record_full(record_content, app.record_path);
                if (!record_result.ok) {
                    issues.push_back({"error", "install record parse error: " + record_result.error, ""});
                }
            }
        }
    }
    
    // Check profile.current symlink
    fs::path profile_current = fs::path(opts.root) / "host" / "profile.current";
    if (fs::exists(profile_current)) {
        if (!fs::is_symlink(profile_current)) {
            issues.push_back({"error", "profile.current exists but is not a symlink", 
                fix ? "" : "nah profile set <name>"});
            if (fix) {
                // Try to fix by removing and recreating as symlink to default
                std::error_code ec;
                fs::remove(profile_current, ec);
                if (!ec) {
                    fs::path default_profile = fs::path(opts.root) / "host" / "profiles" / "default.toml";
                    if (fs::exists(default_profile)) {
                        fs::create_symlink("profiles/default.toml", profile_current, ec);
                        if (!ec) {
                            issues.push_back({"info", "fixed: profile.current symlink recreated", ""});
                        }
                    }
                }
            }
        }
    }
    
    // Output results
    if (opts.json) {
        nlohmann::json j;
        j["target"] = target;
        nlohmann::json issues_arr = nlohmann::json::array();
        for (const auto& issue : issues) {
            nlohmann::json i;
            i["severity"] = issue.severity;
            i["message"] = issue.message;
            if (!issue.fix_command.empty()) {
                i["fix_command"] = issue.fix_command;
            }
            issues_arr.push_back(i);
        }
        j["issues"] = issues_arr;
        j["ok"] = issues.empty() || std::all_of(issues.begin(), issues.end(), 
            [](const Issue& i) { return i.severity == "info"; });
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cout << "Doctor diagnostics for: " << target << std::endl;
        std::cout << std::endl;
        
        if (issues.empty()) {
            std::cout << "Status: OK - no issues found" << std::endl;
        } else {
            int errors = 0, warnings = 0;
            for (const auto& issue : issues) {
                std::string prefix = "[" + issue.severity + "]";
                std::cout << prefix << " " << issue.message << std::endl;
                if (!issue.fix_command.empty()) {
                    std::cout << "  Fix: " << issue.fix_command << std::endl;
                }
                if (issue.severity == "error") errors++;
                if (issue.severity == "warning") warnings++;
            }
            std::cout << std::endl;
            std::cout << "Summary: " << errors << " error(s), " << warnings << " warning(s)" << std::endl;
        }
    }
    
    // Return code: 1 if errors, 2 if warnings only, 0 if ok
    bool has_errors = std::any_of(issues.begin(), issues.end(), 
        [](const Issue& i) { return i.severity == "error"; });
    bool has_warnings = std::any_of(issues.begin(), issues.end(), 
        [](const Issue& i) { return i.severity == "warning"; });
    
    if (has_errors) return 1;
    if (has_warnings) return 2;
    return 0;
}

// ============================================================================
// Validate Command
// ============================================================================

int cmd_validate(const GlobalOptions& opts, const std::string& kind, 
                  const std::string& path, bool strict) {
    std::string content = read_file(path);
    if (content.empty()) {
        print_error("failed to read file: " + path, opts.json);
        return 1;
    }
    
    bool valid = false;
    std::string error;
    std::vector<std::string> warnings;
    
    if (kind == "profile") {
        auto result = nah::parse_host_profile_full(content, path);
        valid = result.ok;
        error = result.error;
        warnings = result.warnings;
    } else if (kind == "install-record") {
        auto result = nah::parse_app_install_record_full(content, path);
        valid = result.ok;
        error = result.error;
        warnings = result.warnings;
    } else if (kind == "nak-record") {
        auto result = nah::parse_nak_install_record_full(content, path);
        valid = result.ok;
        error = result.error;
        warnings = result.warnings;
    } else if (kind == "nak-pack") {
        auto result = nah::parse_nak_pack_manifest(content);
        valid = result.ok;
        error = result.error;
        warnings = result.warnings;
    } else {
        print_error("unknown kind: " + kind, opts.json);
        return 1;
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["valid"] = valid;
        if (!valid) j["error"] = error;
        j["warnings"] = warnings;
        std::cout << j.dump(2) << std::endl;
    } else {
        if (valid) {
            std::cout << path << ": valid" << std::endl;
        } else {
            std::cout << path << ": invalid - " << error << std::endl;
        }
        for (const auto& w : warnings) {
            std::cout << "  warning: " << w << std::endl;
        }
    }
    
    if (strict && !warnings.empty()) return 1;
    return valid ? 0 : 1;
}

// ============================================================================
// Format Command
// ============================================================================

int cmd_format(const GlobalOptions& opts, const std::string& path, bool check) {
    // Read the file
    std::string content = read_file(path);
    if (content.empty() && !fs::exists(path)) {
        print_error("file not found: " + path, opts.json);
        return 1;
    }
    
    // Parse as TOML
    toml::table tbl;
    try {
        tbl = toml::parse(content);
    } catch (const toml::parse_error& e) {
        print_error("TOML parse error: " + std::string(e.what()), opts.json);
        return 1;
    }
    
    // Format to canonical TOML
    std::ostringstream formatted;
    formatted << tbl;
    std::string formatted_str = formatted.str();
    
    // Check if different
    bool differs = (content != formatted_str);
    
    if (check) {
        // Just check, don't modify
        if (opts.json) {
            nlohmann::json j;
            j["path"] = path;
            j["formatted"] = !differs;
            std::cout << j.dump(2) << std::endl;
        } else {
            if (differs) {
                std::cout << path << ": would be reformatted" << std::endl;
            } else {
                std::cout << path << ": already formatted" << std::endl;
            }
        }
        return differs ? 1 : 0;
    }
    
    // Write formatted content atomically
    if (differs) {
        // Write to temp file first
        std::string temp_path = path + ".tmp";
        {
            std::ofstream out(temp_path);
            if (!out) {
                print_error("failed to write temp file: " + temp_path, opts.json);
                return 1;
            }
            out << formatted_str;
            out.close();
            
            // Sync to disk
            out.open(temp_path, std::ios::app);
            out.flush();
        }
        
        // Rename atomically
        std::error_code ec;
        fs::rename(temp_path, path, ec);
        if (ec) {
            fs::remove(temp_path);
            print_error("failed to rename: " + ec.message(), opts.json);
            return 1;
        }
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["path"] = path;
        j["formatted"] = true;
        j["changed"] = differs;
        std::cout << j.dump(2) << std::endl;
    } else {
        if (differs) {
            std::cout << path << ": formatted" << std::endl;
        } else {
            std::cout << path << ": already formatted" << std::endl;
        }
    }
    
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    CLI::App app{"nah - Native Application Host CLI v1.0"};
    app.require_subcommand(0, 1);
    
    GlobalOptions opts;
    
    // Global flags
    app.add_option("--root", opts.root, "NAH root directory")->default_val("/nah");
    app.add_option("--profile", opts.profile, "Profile name to use");
    app.add_flag("--json", opts.json, "Output in JSON format");
    app.add_flag("--trace", opts.trace, "Include trace information");
    app.add_flag("-v,--verbose", opts.verbose, "Verbose output");
    app.add_flag("-q,--quiet", opts.quiet, "Quiet output");
    
    // ========== App Commands ==========
    auto app_cmd = app.add_subcommand("app", "Application lifecycle commands");
    
    auto app_list = app_cmd->add_subcommand("list", "List installed applications");
    app_list->callback([&]() { std::exit(cmd_app_list(opts)); });
    
    std::string app_target;
    auto app_show = app_cmd->add_subcommand("show", "Show application details");
    app_show->add_option("target", app_target, "Application id[@version]")->required();
    app_show->callback([&]() { std::exit(cmd_app_show(opts, app_target)); });
    
    std::string package_path;
    bool app_force = false;
    auto app_install = app_cmd->add_subcommand("install", "Install an application");
    app_install->add_option("package", package_path, "Path to .nap package")->required();
    app_install->add_flag("-f,--force", app_force, "Overwrite existing installation");
    app_install->callback([&]() { std::exit(cmd_app_install(opts, package_path, app_force)); });
    
    auto app_uninstall = app_cmd->add_subcommand("uninstall", "Uninstall an application");
    app_uninstall->add_option("target", app_target, "Application id[@version]")->required();
    app_uninstall->callback([&]() { std::exit(cmd_app_uninstall(opts, app_target)); });
    
    auto app_verify = app_cmd->add_subcommand("verify", "Verify an installed application");
    app_verify->add_option("target", app_target, "Application id[@version]")->required();
    app_verify->callback([&]() { std::exit(cmd_app_verify(opts, app_target)); });
    
    std::string init_dir;
    auto app_init = app_cmd->add_subcommand("init", "Create application skeleton");
    app_init->add_option("dir", init_dir, "Directory to create")->required();
    app_init->callback([&]() { std::exit(cmd_app_init(opts, init_dir)); });
    
    std::string pack_dir, pack_output;
    auto app_pack = app_cmd->add_subcommand("pack", "Create NAP package");
    app_pack->add_option("dir", pack_dir, "Directory to package")->required();
    app_pack->add_option("-o,--output", pack_output, "Output file")->required();
    app_pack->callback([&]() { std::exit(cmd_app_pack(opts, pack_dir, pack_output)); });
    
    // ========== NAK Commands ==========
    auto nak_cmd = app.add_subcommand("nak", "NAK lifecycle commands");
    
    auto nak_list = nak_cmd->add_subcommand("list", "List installed NAKs");
    nak_list->callback([&]() { std::exit(cmd_nak_list(opts)); });
    
    std::string nak_target;
    auto nak_show = nak_cmd->add_subcommand("show", "Show NAK details");
    nak_show->add_option("target", nak_target, "NAK id[@version]")->required();
    nak_show->callback([&]() { std::exit(cmd_nak_show(opts, nak_target)); });
    
    std::string nak_pack_path;
    bool nak_force = false;
    auto nak_install = nak_cmd->add_subcommand("install", "Install a NAK");
    nak_install->add_option("pack", nak_pack_path, "Path to .nak pack")->required();
    nak_install->add_flag("-f,--force", nak_force, "Overwrite existing installation");
    nak_install->callback([&]() { std::exit(cmd_nak_install(opts, nak_pack_path, nak_force)); });
    
    auto nak_path = nak_cmd->add_subcommand("path", "Get NAK root path");
    nak_path->add_option("target", nak_target, "NAK id@version")->required();
    nak_path->callback([&]() { std::exit(cmd_nak_path(opts, nak_target)); });
    
    std::string nak_init_dir;
    auto nak_init = nak_cmd->add_subcommand("init", "Create NAK skeleton");
    nak_init->add_option("dir", nak_init_dir, "Directory to create")->required();
    nak_init->callback([&]() { std::exit(cmd_nak_init(opts, nak_init_dir)); });
    
    std::string nak_pack_dir, nak_pack_output;
    auto nak_pack_cmd = nak_cmd->add_subcommand("pack", "Create NAK pack");
    nak_pack_cmd->add_option("dir", nak_pack_dir, "Directory to package")->required();
    nak_pack_cmd->add_option("-o,--output", nak_pack_output, "Output file")->required();
    nak_pack_cmd->callback([&]() { std::exit(cmd_nak_pack(opts, nak_pack_dir, nak_pack_output)); });
    
    // ========== Profile Commands ==========
    auto profile_cmd = app.add_subcommand("profile", "Profile management commands");
    
    std::string profile_init_dir;
    auto profile_init = profile_cmd->add_subcommand("init", "Create NAH root directory");
    profile_init->add_option("dir", profile_init_dir, "Directory to initialize")->required();
    profile_init->callback([&]() { std::exit(cmd_profile_init(opts, profile_init_dir)); });
    
    auto profile_list = profile_cmd->add_subcommand("list", "List profiles");
    profile_list->callback([&]() { std::exit(cmd_profile_list(opts)); });
    
    std::string profile_name;
    auto profile_show = profile_cmd->add_subcommand("show", "Show profile");
    profile_show->add_option("name", profile_name, "Profile name");
    profile_show->callback([&]() { std::exit(cmd_profile_show(opts, profile_name)); });
    
    auto profile_set = profile_cmd->add_subcommand("set", "Set active profile");
    profile_set->add_option("name", profile_name, "Profile name")->required();
    profile_set->callback([&]() { std::exit(cmd_profile_set(opts, profile_name)); });
    
    std::string profile_path;
    auto profile_validate = profile_cmd->add_subcommand("validate", "Validate profile");
    profile_validate->add_option("path", profile_path, "Profile file path")->required();
    profile_validate->callback([&]() { std::exit(cmd_profile_validate(opts, profile_path)); });
    
    // ========== Contract Commands ==========
    auto contract_cmd = app.add_subcommand("contract", "Contract commands");
    
    std::string contract_target, overrides_file;
    auto contract_show = contract_cmd->add_subcommand("show", "Show launch contract");
    contract_show->add_option("target", contract_target, "Application id[@version]")->required();
    contract_show->add_option("--overrides", overrides_file, "Overrides file");
    contract_show->callback([&]() { std::exit(cmd_contract_show(opts, contract_target, overrides_file)); });
    
    std::string explain_path;
    auto contract_explain = contract_cmd->add_subcommand("explain", "Explain contract value");
    contract_explain->add_option("target", contract_target, "Application id[@version]")->required();
    contract_explain->add_option("path", explain_path, "Field path")->required();
    contract_explain->callback([&]() { std::exit(cmd_contract_explain(opts, contract_target, explain_path)); });
    
    std::string diff_profile_a, diff_profile_b;
    auto contract_diff = contract_cmd->add_subcommand("diff", "Diff contracts between profiles");
    contract_diff->add_option("target", contract_target, "Application id[@version]")->required();
    contract_diff->add_option("profile_a", diff_profile_a, "First profile")->required();
    contract_diff->add_option("profile_b", diff_profile_b, "Second profile")->required();
    contract_diff->callback([&]() { std::exit(cmd_contract_diff(opts, contract_target, diff_profile_a, diff_profile_b)); });
    
    auto contract_resolve = contract_cmd->add_subcommand("resolve", "Explain NAK selection");
    contract_resolve->add_option("target", contract_target, "Application id[@version]")->required();
    contract_resolve->callback([&]() { std::exit(cmd_contract_resolve(opts, contract_target)); });
    
    // ========== Manifest Commands ==========
    auto manifest_cmd = app.add_subcommand("manifest", "Manifest commands");
    
    std::string manifest_target;
    auto manifest_show = manifest_cmd->add_subcommand("show", "Show manifest");
    manifest_show->add_option("target", manifest_target, "Binary or app id[@version]")->required();
    manifest_show->callback([&]() { std::exit(cmd_manifest_show(opts, manifest_target)); });
    
    std::string manifest_input, manifest_output;
    auto manifest_generate = manifest_cmd->add_subcommand("generate", "Generate binary manifest from TOML");
    manifest_generate->add_option("input", manifest_input, "Input TOML file")->required();
    manifest_generate->add_option("-o,--output", manifest_output, "Output binary manifest file")->required();
    manifest_generate->callback([&]() { std::exit(cmd_manifest_generate(opts, manifest_input, manifest_output)); });
    
    // ========== Doctor Command ==========
    std::string doctor_target;
    bool doctor_fix = false;
    auto doctor_cmd = app.add_subcommand("doctor", "Diagnose issues");
    doctor_cmd->add_option("target", doctor_target, "Binary or app id[@version]")->required();
    doctor_cmd->add_flag("--fix", doctor_fix, "Attempt to fix issues");
    doctor_cmd->callback([&]() { std::exit(cmd_doctor(opts, doctor_target, doctor_fix)); });
    
    // ========== Validate Command ==========
    std::string validate_kind, validate_path;
    bool validate_strict = false;
    auto validate_cmd = app.add_subcommand("validate", "Validate files");
    validate_cmd->add_option("kind", validate_kind, 
        "Kind: profile, install-record, nak-record, package, nak-pack, capabilities")->required();
    validate_cmd->add_option("path", validate_path, "File path")->required();
    validate_cmd->add_flag("--strict", validate_strict, "Strict mode");
    validate_cmd->callback([&]() { std::exit(cmd_validate(opts, validate_kind, validate_path, validate_strict)); });
    
    // ========== Format Command ==========
    std::string format_path;
    bool format_check = false;
    auto format_cmd = app.add_subcommand("format", "Format TOML files");
    format_cmd->add_option("file", format_path, "File to format")->required();
    format_cmd->add_flag("--check", format_check, "Check only, don't modify");
    format_cmd->callback([&]() { std::exit(cmd_format(opts, format_path, format_check)); });
    
    // Parse and run
    CLI11_PARSE(app, argc, argv);
    
    // If no subcommand, show help
    if (app.get_subcommands().empty()) {
        std::cout << app.help() << std::endl;
    }
    
    return 0;
}
