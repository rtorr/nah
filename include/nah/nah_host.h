/**
 * NAH Host Library
 * ================
 * Complete host implementation with all dependencies included.
 * This provides a high-level API for hosts to integrate NAH without
 * reimplementing all the boilerplate.
 */

#ifndef NAH_HOST_H
#define NAH_HOST_H

#ifdef __cplusplus

#include "nah_core.h"
#include "nah_json.h"
#include "nah_fs.h"
#include "nah_exec.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>

namespace nah {
namespace host {

// Portable getenv that avoids MSVC warnings
namespace detail {
inline std::string safe_getenv(const char* name) {
#ifdef _WIN32
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, name) == 0 && buf != nullptr) {
        std::string result(buf);
        free(buf);
        return result;
    }
    return "";
#else
    const char* val = std::getenv(name);
    return val ? val : "";
#endif
}
} // namespace detail

// ============================================================================
// App Info
// ============================================================================

/**
 * Application metadata for installed apps
 */
struct AppInfo {
    std::string id;
    std::string version;
    std::string instance_id;
    std::string install_root;
    std::string record_path;
};

// ============================================================================
// NAH Host Class
// ============================================================================

/**
 * Main interface for interacting with a NAH root.
 *
 * Example usage:
 *   auto host = nah::host::NahHost::create("/nah");
 *
 *   // List apps
 *   auto apps = host->listApplications();
 *
 *   // Get launch contract
 *   auto result = host->getLaunchContract("com.example.app");
 *   if (result.ok) {
 *       // Use result.contract for execution
 *   }
 *
 *   // Execute app directly
 *   int exit_code = host->executeApplication("com.example.app");
 */
class NahHost {
public:
    /**
     * Create a NahHost instance for a NAH root directory.
     * If root_path is empty, uses $NAH_ROOT or /nah as default.
     */
    static std::unique_ptr<NahHost> create(const std::string& root_path = "");

    /**
     * Get the NAH root path
     */
    const std::string& root() const { return root_; }

    /**
     * List all installed applications
     */
    std::vector<AppInfo> listApplications() const;

    /**
     * Find an installed application by ID
     * @param id Application identifier (e.g., "com.example.app")
     * @param version Optional specific version (empty = latest)
     * @return AppInfo if found, nullopt otherwise
     */
    std::optional<AppInfo> findApplication(const std::string& id,
                                          const std::string& version = "") const;

    /**
     * Get the host environment from host.json
     */
    nah::core::HostEnvironment getHostEnvironment() const;

    /**
     * Generate a launch contract for an application
     * @param app_id Application identifier
     * @param version Optional specific version (empty = latest)
     * @param enable_trace Include composition trace in result
     * @return Composition result containing the launch contract
     */
    nah::core::CompositionResult getLaunchContract(
        const std::string& app_id,
        const std::string& version = "",
        bool enable_trace = false) const;

    /**
     * Execute an application directly (compose and run)
     * @param app_id Application identifier
     * @param version Optional specific version (empty = latest)
     * @param args Additional arguments to pass to the app
     * @param output_handler Optional callback for output (line by line)
     * @return Exit code of the application
     */
    int executeApplication(
        const std::string& app_id,
        const std::string& version = "",
        const std::vector<std::string>& args = {},
        std::function<void(const std::string&)> output_handler = nullptr) const;

    /**
     * Execute using a pre-composed contract
     * @param contract The launch contract to execute
     * @param args Additional arguments to pass to the app
     * @param output_handler Optional callback for output (line by line)
     * @return Exit code of the application
     */
    int executeContract(
        const nah::core::LaunchContract& contract,
        const std::vector<std::string>& args = {},
        std::function<void(const std::string&)> output_handler = nullptr) const;

    /**
     * Check if an application is installed
     */
    bool isApplicationInstalled(const std::string& app_id,
                               const std::string& version = "") const;

    /**
     * Get inventory of installed NAKs
     */
    nah::core::RuntimeInventory getInventory() const;

    /**
     * Validate NAH root structure
     * @return Error message if invalid, empty string if valid
     */
    std::string validateRoot() const;

private:
    explicit NahHost(std::string root) : root_(std::move(root)) {}

    // Load install record for an app
    std::optional<nah::core::InstallRecord> loadInstallRecord(const std::string& path) const;

    // Load app manifest (JSON or binary)
    std::optional<nah::core::AppDeclaration> loadAppManifest(const std::string& app_dir) const;

    // Parse binary TLV manifest
    std::optional<std::string> parseBinaryManifest(const std::string& data) const;

    std::string root_;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * Quick execute - compose and run an app in one call
 * @param app_id Application identifier
 * @param nah_root NAH root directory (empty = use default)
 * @return Exit code
 */
inline int quickExecute(const std::string& app_id, const std::string& nah_root = "") {
    auto host = NahHost::create(nah_root);
    return host->executeApplication(app_id);
}

/**
 * List all installed apps
 * @param nah_root NAH root directory (empty = use default)
 * @return Vector of app IDs with versions
 */
inline std::vector<std::string> listInstalledApps(const std::string& nah_root = "") {
    auto host = NahHost::create(nah_root);
    auto apps = host->listApplications();
    std::vector<std::string> results;
    for (const auto& app : apps) {
        results.push_back(app.id + "@" + app.version);
    }
    return results;
}

// ============================================================================
// Implementation
// ============================================================================

#ifdef NAH_HOST_IMPLEMENTATION

inline std::unique_ptr<NahHost> NahHost::create(const std::string& root_path) {
    std::string resolved_root = root_path;

    if (resolved_root.empty()) {
        std::string env_root = detail::safe_getenv("NAH_ROOT");
        if (!env_root.empty()) {
            resolved_root = env_root;
        } else {
            resolved_root = "/nah";
        }
    }

    return std::unique_ptr<NahHost>(new NahHost(resolved_root));
}

inline std::vector<AppInfo> NahHost::listApplications() const {
    std::vector<AppInfo> apps;
    std::string apps_dir = root_ + "/registry/apps";

    if (!nah::fs::exists(apps_dir)) {
        return apps;
    }

    auto files = nah::fs::list_directory(apps_dir);
    for (const auto& entry : files) {
        // list_directory returns full paths, so use entry directly
        if (entry.size() > 5 && entry.substr(entry.size() - 5) == ".json") {
            auto record = loadInstallRecord(entry);
            if (record) {
                AppInfo info;
                info.id = record->app.id;
                info.version = record->app.version;
                info.instance_id = record->install.instance_id;
                info.install_root = record->paths.install_root;
                info.record_path = entry;
                apps.push_back(info);
            }
        }
    }

    return apps;
}

inline std::optional<AppInfo> NahHost::findApplication(const std::string& id,
                                                      const std::string& version) const {
    auto apps = listApplications();

    std::vector<AppInfo> matches;
    for (const auto& app : apps) {
        if (app.id == id) {
            if (version.empty() || app.version == version) {
                matches.push_back(app);
            }
        }
    }

    if (matches.empty()) {
        return std::nullopt;
    }

    // If multiple versions and no specific version requested, return latest
    // (In real implementation, would sort by version)
    return matches[0];
}

inline nah::core::HostEnvironment NahHost::getHostEnvironment() const {
    std::string host_json_path = root_ + "/host/host.json";
    auto content = nah::fs::read_file(host_json_path);
    if (!content) {
        // Return empty environment
        return nah::core::HostEnvironment{};
    }

    auto result = nah::json::parse_host_environment(*content, host_json_path);
    if (result.ok) {
        return result.value;
    }

    // Return empty environment on parse failure
    return nah::core::HostEnvironment{};
}

inline nah::core::CompositionResult NahHost::getLaunchContract(
    const std::string& app_id,
    const std::string& version,
    bool enable_trace) const {

    // Find the application
    auto app_info = findApplication(app_id, version);
    if (!app_info) {
        nah::core::CompositionResult result;
        result.ok = false;
        result.critical_error = nah::core::CriticalError::MANIFEST_MISSING;
        result.critical_error_context = "Application not found: " + app_id;
        return result;
    }

    // Load install record
    auto record = loadInstallRecord(app_info->record_path);
    if (!record) {
        nah::core::CompositionResult result;
        result.ok = false;
        result.critical_error = nah::core::CriticalError::INSTALL_RECORD_INVALID;
        result.critical_error_context = "Failed to load install record";
        return result;
    }

    // Load app manifest
    auto app_decl = loadAppManifest(app_info->install_root);
    if (!app_decl) {
        nah::core::CompositionResult result;
        result.ok = false;
        result.critical_error = nah::core::CriticalError::MANIFEST_MISSING;
        result.critical_error_context = "Failed to load app manifest";
        return result;
    }

    // Load host environment
    auto host_env = getHostEnvironment();

    // Get inventory
    auto inventory = getInventory();

    // Compose
    nah::core::CompositionOptions opts;
    opts.enable_trace = enable_trace;

    return nah::core::nah_compose(*app_decl, host_env, *record, inventory, opts);
}

inline int NahHost::executeApplication(
    const std::string& app_id,
    const std::string& version,
    const std::vector<std::string>& args,
    std::function<void(const std::string&)> output_handler) const {

    auto result = getLaunchContract(app_id, version);
    if (!result.ok) {
        if (output_handler) {
            output_handler("Error: " + result.critical_error_context);
        }
        return 1;
    }

    return executeContract(result.contract, args, output_handler);
}

inline int NahHost::executeContract(
    const nah::core::LaunchContract& contract,
    const std::vector<std::string>& /* args */,
    std::function<void(const std::string&)> output_handler) const {

    // Use nah::exec::execute which takes the contract directly
    // Note: args parameter reserved for future use (appending to contract arguments)
    auto exec_result = nah::exec::execute(contract);

    if (!exec_result.ok) {
        if (output_handler) {
            output_handler("Execution error: " + exec_result.error);
        }
        return 1;
    }

    return exec_result.exit_code;
}

inline bool NahHost::isApplicationInstalled(const std::string& app_id,
                                           const std::string& version) const {
    return findApplication(app_id, version).has_value();
}

inline nah::core::RuntimeInventory NahHost::getInventory() const {
    nah::core::RuntimeInventory inventory;
    std::string naks_dir = root_ + "/registry/naks";

    if (!nah::fs::exists(naks_dir)) {
        return inventory;
    }

    auto files = nah::fs::list_directory(naks_dir);
    for (const auto& entry : files) {
        // list_directory returns full paths, so use entry directly
        if (entry.size() > 5 && entry.substr(entry.size() - 5) == ".json") {
            auto record = loadInstallRecord(entry);
            if (record) {
                // Try to find runtime descriptor
                std::string runtime_path = record->paths.install_root + "/nah-runtime.json";
                auto runtime_content = nah::fs::read_file(runtime_path);
                if (runtime_content) {
                    // Use the NAK's record_ref as key (e.g., "lua@5.4.6.json")
                    std::string record_ref = record->app.id + "@" + record->app.version + ".json";

                    auto result = nah::json::parse_runtime_descriptor(*runtime_content, entry);
                    if (result.ok) {
                        result.value.source_path = entry;
                        inventory.runtimes[record_ref] = result.value;
                    }
                }
            }
        }
    }

    return inventory;
}

inline std::string NahHost::validateRoot() const {
    if (!nah::fs::exists(root_)) {
        return "NAH root does not exist: " + root_;
    }

    // Check required directories
    std::vector<std::string> required_dirs = {
        "/registry/apps",
        "/host"
    };

    for (const auto& dir : required_dirs) {
        if (!nah::fs::exists(root_ + dir)) {
            return "Missing required directory: " + root_ + dir;
        }
    }

    return "";  // Valid
}

inline std::optional<nah::core::InstallRecord> NahHost::loadInstallRecord(const std::string& path) const {
    auto content = nah::fs::read_file(path);
    if (!content) {
        return std::nullopt;
    }

    auto result = nah::json::parse_install_record(*content);
    if (result.ok) {
        // Ensure absolute paths
        if (!result.value.paths.install_root.empty() && result.value.paths.install_root[0] != '/') {
            result.value.paths.install_root = nah::fs::absolute_path(root_ + "/" + result.value.paths.install_root);
        }
        return result.value;
    }

    return std::nullopt;
}

inline std::optional<nah::core::AppDeclaration> NahHost::loadAppManifest(const std::string& app_dir) const {
    // Try JSON first
    auto json_content = nah::fs::read_file(app_dir + "/nah.json");
    if (json_content) {
        auto result = nah::json::parse_app_declaration(*json_content);
        if (result.ok) {
            return result.value;
        }
    }

    // Try binary manifest
    auto binary_content = nah::fs::read_file(app_dir + "/manifest.nah");
    if (binary_content) {
        auto json_str = parseBinaryManifest(*binary_content);
        if (json_str) {
            auto result = nah::json::parse_app_declaration(*json_str);
            if (result.ok) {
                return result.value;
            }
        }
    }

    return std::nullopt;
}

inline std::optional<std::string> NahHost::parseBinaryManifest(const std::string& data) const {
    if (data.size() < 4) return std::nullopt;

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.data());
    size_t len = data.size();

    // Check magic and version
    if (bytes[0] != 'N' || bytes[1] != 'A' || bytes[2] != 'H' || bytes[3] != 0x02) {
        return std::nullopt;
    }

    nlohmann::json manifest;
    size_t offset = 4;

    while (offset + 3 <= len) {
        uint8_t field_type = bytes[offset];
        if (field_type == 0 || field_type > 0x0F) break;

        uint16_t field_len = static_cast<uint16_t>(
            static_cast<uint16_t>(bytes[offset + 1]) |
            (static_cast<uint16_t>(bytes[offset + 2]) << 8));
        offset += 3;

        if (offset + field_len > len) break;

        std::string value(reinterpret_cast<const char*>(&bytes[offset]), field_len);
        offset += field_len;

        switch (field_type) {
            case 0x01: manifest["id"] = value; break;
            case 0x02: manifest["version"] = value; break;
            case 0x03: manifest["nak_id"] = value; break;
            case 0x04: manifest["nak_version_req"] = value; break;
            case 0x05: manifest["entrypoint"] = value; break;
            case 0x06:
                if (!manifest.contains("lib_dirs")) {
                    manifest["lib_dirs"] = nlohmann::json::array();
                }
                manifest["lib_dirs"].push_back(value);
                break;
            case 0x07:
                if (!manifest.contains("asset_dirs")) {
                    manifest["asset_dirs"] = nlohmann::json::array();
                }
                manifest["asset_dirs"].push_back(value);
                break;
            case 0x0A: manifest["nak_loader"] = value; break;
        }
    }

    return manifest.dump();
}

#endif // NAH_HOST_IMPLEMENTATION

} // namespace host
} // namespace nah

#endif // __cplusplus

#endif // NAH_HOST_H