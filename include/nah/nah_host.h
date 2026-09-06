/** High-level filesystem-backed host API. */

#ifndef NAH_HOST_H
#define NAH_HOST_H

#ifdef __cplusplus

#include "nah_core.h"
#include "nah_json.h"
#include "nah_fs.h"
#include "nah_exec.h"
#include "nah_semver.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <algorithm>
#include <filesystem>

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

inline std::optional<std::filesystem::path> canonical_boundary_path(const std::string& path) {
    std::error_code ec;
    auto absolute = std::filesystem::absolute(std::filesystem::path(path), ec);
    if (ec) return std::nullopt;
    auto canonical = std::filesystem::weakly_canonical(absolute, ec);
    if (ec) return std::nullopt;
    return canonical.lexically_normal();
}

inline std::optional<std::string> resolve_within(const std::string& root,
                                                 const std::string& candidate) {
    if (root.empty() || candidate.empty()) return std::nullopt;
    const auto normalized_root = canonical_boundary_path(root);
    const auto normalized_candidate = canonical_boundary_path(candidate);
    if (!normalized_root || !normalized_candidate) return std::nullopt;

    auto root_it = normalized_root->begin();
    auto candidate_it = normalized_candidate->begin();
    for (; root_it != normalized_root->end(); ++root_it, ++candidate_it) {
        if (candidate_it == normalized_candidate->end() || *root_it != *candidate_it) {
            return std::nullopt;
        }
    }
    return normalized_candidate->string();
}
} // namespace detail

// ============================================================================
// App Info
// ============================================================================

struct AppInfo {
    std::string id;
    std::string version;
    std::string instance_id;
    std::string install_root;
    std::string record_path;
    std::string metadata_json;
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
     * If root_path is empty, uses $NAH_ROOT, then the user's .nah directory.
     * Note: Does not validate the root directory structure.
     */
    static std::unique_ptr<NahHost> create(const std::string& root_path = "");

    /**
     * Discover and create NahHost from multiple candidate paths.
     * Searches paths in order, returns first valid NAH root found.
     *
     * @param search_paths Candidate paths (empty strings skipped)
     * @return NahHost instance, or nullptr if no valid root found
     *
     * Example:
     *   auto host = NahHost::discover({
     *       std::getenv("NAH_ROOT"),
     *       "/path/to/project/.nah",
     *       std::string(std::getenv("HOME")) + "/.nah"
     *   });
     */
    static std::unique_ptr<NahHost> discover(const std::vector<std::string>& search_paths);

    /**
     * Check if a directory is a valid NAH root.
     * A valid root must exist and contain the required directory structure.
     *
     * @param path Directory to check
     * @return true if valid NAH root with required directories
     */
    static bool isValidRoot(const std::string& path);

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
    std::optional<nah::core::HostEnvironment> getHostEnvironment() const;

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
     * Get launch contract for an application with options
     * @param app_id Application identifier
     * @param version Optional specific version (empty = latest)
     * @param options Composition options (trace, loader override, etc.)
     * @return Composition result containing the launch contract
     */
    nah::core::CompositionResult getLaunchContract(
        const std::string& app_id,
        const std::string& version,
        const nah::core::CompositionOptions& options) const;

    /**
     * Execute an application directly (compose and run)
     * @param app_id Application identifier
     * @param version Optional specific version (empty = latest)
     * @param args Additional arguments to pass to the app
     * @return Exit code of the application
     */
    int executeApplication(
        const std::string& app_id,
        const std::string& version = "",
        const std::vector<std::string>& args = {}) const;

    /**
     * Execute using a pre-composed contract
     * @param contract The launch contract to execute
     * @param args Additional arguments to pass to the app
     * @return Exit code of the application
     */
    int executeContract(
        const nah::core::LaunchContract& contract,
        const std::vector<std::string>& args = {}) const;

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

    // Load app manifest (JSON)
    std::optional<nah::core::AppDeclaration> loadAppManifest(const std::string& app_dir) const;

    std::string extractMetadataJson(const std::string& app_dir) const;

    std::string root_;
};

// ============================================================================
// Implementation
// ============================================================================

inline std::unique_ptr<NahHost> NahHost::create(const std::string& root_path) {
    std::string resolved_root = root_path;

    if (resolved_root.empty()) {
        std::string env_root = detail::safe_getenv("NAH_ROOT");
        if (!env_root.empty()) {
            resolved_root = env_root;
        } else {
            std::string home = detail::safe_getenv("HOME");
            if (home.empty()) home = detail::safe_getenv("USERPROFILE");
            resolved_root = home.empty() ? ".nah" : home + "/.nah";
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
        if (entry.size() > 5 && entry.substr(entry.size() - 5) == ".json") {
            auto record = loadInstallRecord(entry);
            if (record) {
                AppInfo info;
                info.id = record->app.id;
                info.version = record->app.version;
                info.instance_id = record->install.instance_id;
                info.install_root = record->paths.install_root;
                info.record_path = entry;
                info.metadata_json = extractMetadataJson(record->paths.install_root);
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

    // If multiple versions, sort by semver and return the highest
    if (matches.size() > 1 && version.empty()) {
        std::sort(matches.begin(), matches.end(), [](const AppInfo& a, const AppInfo& b) {
            auto va = nah::semver::parse_version(a.version);
            auto vb = nah::semver::parse_version(b.version);
            if (va && vb) {
                return *va > *vb;  // Descending order (highest first)
            }
            // Fallback to string comparison if parsing fails
            return a.version > b.version;
        });
    }
    return matches[0];
}

inline std::optional<nah::core::HostEnvironment> NahHost::getHostEnvironment() const {
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

    return std::nullopt;
}

inline nah::core::CompositionResult NahHost::getLaunchContract(
    const std::string& app_id,
    const std::string& version,
    bool enable_trace) const {
    nah::core::CompositionOptions opts;
    opts.enable_trace = enable_trace;
    return getLaunchContract(app_id, version, opts);
}

inline nah::core::CompositionResult NahHost::getLaunchContract(
    const std::string& app_id,
    const std::string& version,
    const nah::core::CompositionOptions& options) const {

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
    if (!host_env) {
        nah::core::CompositionResult result;
        result.critical_error = nah::core::CriticalError::HOST_CONFIG_INVALID;
        result.critical_error_context = "Failed to load host configuration";
        return result;
    }

    // Get inventory
    auto inventory = getInventory();

    auto result = nah::core::nah_compose(*app_decl, *host_env, *record, inventory, options);
    if (!result.ok) return result;

    const std::string binary_boundary = result.contract.nak.id.empty()
        ? result.contract.app.root : result.contract.nak.root;
    const auto binary = detail::resolve_within(binary_boundary, result.contract.execution.binary);
    if (!binary || !nah::fs::is_file(*binary)) {
        result.ok = false;
        result.critical_error = nah::core::CriticalError::ENTRYPOINT_NOT_FOUND;
        result.critical_error_context = "Execution binary not found: " + result.contract.execution.binary;
        return result;
    }
    result.contract.execution.binary = *binary;

    const auto cwd_in_app = detail::resolve_within(result.contract.app.root, result.contract.execution.cwd);
    std::optional<std::string> cwd_in_nak;
    if (!result.contract.nak.root.empty()) {
        cwd_in_nak = detail::resolve_within(result.contract.nak.root, result.contract.execution.cwd);
    }
    const auto cwd = cwd_in_app ? cwd_in_app : cwd_in_nak;
    if (!cwd || !nah::fs::is_directory(*cwd)) {
        result.ok = false;
        result.critical_error = nah::core::CriticalError::PATH_TRAVERSAL;
        result.critical_error_context = "Working directory is outside the installed app and runtime";
        return result;
    }
    result.contract.execution.cwd = *cwd;
    return result;
}

inline int NahHost::executeApplication(
    const std::string& app_id,
    const std::string& version,
    const std::vector<std::string>& args) const {

    auto result = getLaunchContract(app_id, version);
    if (!result.ok) return 1;

    return executeContract(result.contract, args);
}

inline int NahHost::executeContract(
    const nah::core::LaunchContract& contract,
    const std::vector<std::string>& args) const {

    auto effective_contract = contract;
    effective_contract.execution.arguments.insert(
        effective_contract.execution.arguments.end(), args.begin(), args.end());
    auto exec_result = nah::exec::execute(effective_contract);

    if (!exec_result.ok) return 1;

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
            // NAH v2.0: Registry files ARE the runtime descriptors
            // Parse the registry file directly as a RuntimeDescriptor
            auto runtime_content = nah::fs::read_file(entry);
            if (runtime_content) {
                // Extract record_ref from filename
                std::string basename = entry;
                size_t last_slash = entry.rfind('/');
                if (last_slash != std::string::npos) {
                    basename = entry.substr(last_slash + 1);
                }
                std::string record_ref = basename;

                auto result = nah::json::parse_runtime_descriptor(*runtime_content, entry);
                if (result.ok) {
                    result.value.source_path = entry;

                    // Resolve relative paths to absolute (for sandbox/portability support)
                    if (!result.value.paths.root.empty() && !nah::fs::is_absolute_path(result.value.paths.root)) {
                        result.value.paths.root = nah::fs::absolute_path(nah::fs::join_paths(root_, result.value.paths.root));
                    }
                    const auto runtime_root = detail::resolve_within(root_ + "/naks", result.value.paths.root);
                    if (!runtime_root) {
                        continue;
                    }
                    result.value.paths.root = *runtime_root;

                    if (result.value.paths.resource_root.empty()) {
                        result.value.paths.resource_root = result.value.paths.root;
                    } else if (!nah::fs::is_absolute_path(result.value.paths.resource_root)) {
                        result.value.paths.resource_root = nah::fs::absolute_path(
                            nah::fs::join_paths(result.value.paths.root, result.value.paths.resource_root));
                    }
                    const auto resource_root = detail::resolve_within(
                        result.value.paths.root, result.value.paths.resource_root);
                    if (!resource_root) {
                        continue;
                    }
                    result.value.paths.resource_root = *resource_root;

                    // Resolve relative lib_dirs
                    for (auto& lib_dir : result.value.paths.lib_dirs) {
                        if (!lib_dir.empty() && !nah::fs::is_absolute_path(lib_dir)) {
                            lib_dir = nah::fs::absolute_path(nah::fs::join_paths(result.value.paths.root, lib_dir));
                        }
                        const auto resolved = detail::resolve_within(result.value.paths.root, lib_dir);
                        if (!resolved) {
                            lib_dir.clear();
                        } else {
                            lib_dir = *resolved;
                        }
                    }

                    // Resolve relative loader exec_paths
                    for (auto& [name, loader] : result.value.loaders) {
                        if (!loader.exec_path.empty() && !nah::fs::is_absolute_path(loader.exec_path)) {
                            loader.exec_path = nah::fs::absolute_path(nah::fs::join_paths(result.value.paths.root, loader.exec_path));
                        }
                        const auto resolved = detail::resolve_within(result.value.paths.root, loader.exec_path);
                        if (!resolved) {
                            loader.exec_path.clear();
                        } else {
                            loader.exec_path = *resolved;
                        }
                    }

                    inventory.runtimes[record_ref] = result.value;
                }
            }
        }
    }

    return inventory;
}

inline std::string NahHost::validateRoot() const {
    if (!nah::fs::is_directory(root_)) {
        return "NAH root does not exist: " + root_;
    }

    // Check required directories
    const std::vector<std::string> required_dirs = {
        "/apps", "/naks", "/host", "/registry/apps", "/registry/naks", "/staging"
    };

    for (const auto& dir : required_dirs) {
        if (!nah::fs::is_directory(root_ + dir)) {
            return "Missing required directory: " + root_ + dir;
        }
    }

    return "";  // Valid
}

inline bool NahHost::isValidRoot(const std::string& path) {
    return !path.empty() && NahHost(path).validateRoot().empty();
}

inline std::unique_ptr<NahHost> NahHost::discover(const std::vector<std::string>& search_paths) {
    for (const auto& path : search_paths) {
        // Skip empty paths (e.g., from getenv returning nullptr)
        if (path.empty()) {
            continue;
        }

        // Check if this path is a valid NAH root
        if (isValidRoot(path)) {
            return std::unique_ptr<NahHost>(new NahHost(path));
        }
    }

    // No valid root found
    return nullptr;
}

inline std::optional<nah::core::InstallRecord> NahHost::loadInstallRecord(const std::string& path) const {
    auto content = nah::fs::read_file(path);
    if (!content) {
        return std::nullopt;
    }

    auto result = nah::json::parse_install_record(*content);
    if (result.ok) {
        // Ensure absolute paths (portable check for both Unix and Windows)
        if (!result.value.paths.install_root.empty() && !nah::fs::is_absolute_path(result.value.paths.install_root)) {
            result.value.paths.install_root = nah::fs::absolute_path(nah::fs::join_paths(root_, result.value.paths.install_root));
        }
        const auto install_root = detail::resolve_within(root_ + "/apps", result.value.paths.install_root);
        if (!install_root) {
            return std::nullopt;
        }
        result.value.paths.install_root = *install_root;
        return result.value;
    }

    return std::nullopt;
}

inline std::optional<nah::core::AppDeclaration> NahHost::loadAppManifest(const std::string& app_dir) const {
    auto json_content = nah::fs::read_file(app_dir + "/nap.json");
    if (json_content) {
        auto result = nah::json::parse_app_declaration(*json_content);
        if (result.ok) {
            return result.value;
        }
    }

    return std::nullopt;
}

inline std::string NahHost::extractMetadataJson(const std::string& app_dir) const {
    auto json_content = nah::fs::read_file(app_dir + "/nap.json");
    if (!json_content) {
        return "{}";
    }

    try {
        auto j = nah::json::json::parse(*json_content);

        if (j.contains("app") && j["app"].is_object()) {
            j = j["app"];
        }

        if (j.contains("metadata") && j["metadata"].is_object()) {
            return j["metadata"].dump();
        }
    } catch (...) {
    }

    return "{}";
}

} // namespace host
} // namespace nah

#endif // __cplusplus

#endif // NAH_HOST_H
