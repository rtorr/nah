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
#include "nah_semver.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <algorithm>

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
     * If root_path is empty, uses $NAH_ROOT or /nah as default.
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

    // ========================================================================
    // Component API
    // ========================================================================

    /**
     * Compose launch contract for a component via URI
     * @param uri Component URI (e.g., "com.example.suite://editor/open?file=doc.txt")
     * @param referrer_uri URI of calling component (optional, for context)
     * @return Composition result containing launch contract
     */
    nah::core::CompositionResult composeComponentLaunch(
        const std::string& uri,
        const std::string& referrer_uri = "") const;
    
    /**
     * Execute a component via URI
     * @param uri Component URI
     * @param referrer_uri URI of calling component (optional)
     * @param args Additional arguments
     * @param output_handler Optional callback for output
     * @return Exit code
     */
    int launchComponent(
        const std::string& uri,
        const std::string& referrer_uri = "",
        const std::vector<std::string>& args = {},
        std::function<void(const std::string&)> output_handler = nullptr) const;
    
    /**
     * Check if a component URI can be handled
     * @param uri Component URI
     * @return true if a component can handle this URI
     */
    bool canHandleComponentUri(const std::string& uri) const;
    
    /**
     * List all components across all installed applications
     * @return Vector of (app_id, component) pairs
     */
    std::vector<std::pair<std::string, nah::core::ComponentDecl>> listAllComponents() const;

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

    // Get inventory
    auto inventory = getInventory();

    // Use provided options (including loader_override)
    return nah::core::nah_compose(*app_decl, host_env, *record, inventory, options);
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
                    
                    // Resolve relative lib_dirs
                    for (auto& lib_dir : result.value.paths.lib_dirs) {
                        if (!lib_dir.empty() && !nah::fs::is_absolute_path(lib_dir)) {
                            lib_dir = nah::fs::absolute_path(nah::fs::join_paths(result.value.paths.root, lib_dir));
                        }
                    }
                    
                    // Resolve relative loader exec_paths
                    for (auto& [name, loader] : result.value.loaders) {
                        if (!loader.exec_path.empty() && !nah::fs::is_absolute_path(loader.exec_path)) {
                            loader.exec_path = nah::fs::absolute_path(nah::fs::join_paths(result.value.paths.root, loader.exec_path));
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

inline bool NahHost::isValidRoot(const std::string& path) {
    if (path.empty() || !nah::fs::exists(path)) {
        return false;
    }

    // Check required directories that make up a valid NAH root
    std::vector<std::string> required_dirs = {
        "/registry/apps",
        "/host"
    };

    for (const auto& dir : required_dirs) {
        std::string full_path = path + dir;
        if (!nah::fs::exists(full_path)) {
            return false;
        }
    }

    return true;
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

// ============================================================================
// Component Implementation
// ============================================================================

// Helper: Match URI against pattern
inline bool matches_uri_pattern(const std::string& pattern, const std::string& uri) {
    auto pattern_parsed = nah::core::parse_component_uri(pattern);
    auto uri_parsed = nah::core::parse_component_uri(uri);
    
    if (!pattern_parsed.valid || !uri_parsed.valid) {
        return false;
    }
    
    // App IDs must match
    if (pattern_parsed.app_id != uri_parsed.app_id) {
        return false;
    }
    
    // Check if pattern ends with wildcard
    if (pattern_parsed.component_path.size() >= 2 &&
        pattern_parsed.component_path.substr(pattern_parsed.component_path.size() - 2) == "/*") {
        // Prefix match
        std::string prefix = pattern_parsed.component_path.substr(
            0, pattern_parsed.component_path.size() - 2
        );
        return uri_parsed.component_path.substr(0, prefix.size()) == prefix;
    } else {
        // Exact match
        return pattern_parsed.component_path == uri_parsed.component_path;
    }
}

inline nah::core::CompositionResult NahHost::composeComponentLaunch(
    const std::string& uri,
    const std::string& referrer_uri) const {
    
    // 1. Parse URI
    auto parsed = nah::core::parse_component_uri(uri);
    if (!parsed.valid) {
        nah::core::CompositionResult result;
        result.ok = false;
        result.critical_error = nah::core::CriticalError::MANIFEST_MISSING;
        result.critical_error_context = "Invalid component URI: " + uri;
        return result;
    }
    
    // 2. Find application
    auto app_info = findApplication(parsed.app_id);
    if (!app_info) {
        nah::core::CompositionResult result;
        result.ok = false;
        result.critical_error = nah::core::CriticalError::MANIFEST_MISSING;
        result.critical_error_context = "Application not found: " + parsed.app_id;
        return result;
    }
    
    // 3. Load app manifest to get components
    auto app_decl = loadAppManifest(app_info->install_root);
    if (!app_decl) {
        nah::core::CompositionResult result;
        result.ok = false;
        result.critical_error = nah::core::CriticalError::MANIFEST_MISSING;
        result.critical_error_context = "Failed to load app manifest";
        return result;
    }
    
    // 4. Match component by URI pattern
    nah::core::ComponentDecl* matched_component = nullptr;
    for (auto& comp : app_decl->components) {
        if (matches_uri_pattern(comp.uri_pattern, uri)) {
            matched_component = &comp;
            break;  // First match wins
        }
    }
    
    if (!matched_component) {
        nah::core::CompositionResult result;
        result.ok = false;
        result.critical_error = nah::core::CriticalError::MANIFEST_MISSING;
        result.critical_error_context = "No component matches URI: " + uri;
        return result;
    }
    
    // 5. Create modified app declaration with component entrypoint
    //    We reuse nah_compose by creating a temporary AppDeclaration
    //    with the component's entrypoint
    nah::core::AppDeclaration component_app = *app_decl;
    component_app.entrypoint_path = matched_component->entrypoint;
    
    // Override loader if component specifies one
    if (!matched_component->loader.empty()) {
        component_app.nak_loader = matched_component->loader;
    }
    
    // Merge component-specific environment
    for (const auto& [key, value] : matched_component->environment) {
        // Convert to KEY=value format
        component_app.env_vars.push_back(key + "=" + value.value);
    }
    
    // Merge component-specific permissions
    component_app.permissions_filesystem.insert(
        component_app.permissions_filesystem.end(),
        matched_component->permissions_filesystem.begin(),
        matched_component->permissions_filesystem.end()
    );
    component_app.permissions_network.insert(
        component_app.permissions_network.end(),
        matched_component->permissions_network.begin(),
        matched_component->permissions_network.end()
    );
    
    // 6. Get install record (need for nah_compose)
    auto install_record = loadInstallRecord(app_info->record_path);
    if (!install_record) {
        nah::core::CompositionResult result;
        result.ok = false;
        result.critical_error = nah::core::CriticalError::INSTALL_RECORD_INVALID;
        result.critical_error_context = "Failed to load install record";
        return result;
    }
    
    // Override pinned loader if component specifies one
    if (!matched_component->loader.empty()) {
        install_record->nak.loader = matched_component->loader;
    }
    
    // 7. Get host environment and inventory
    auto host_env = getHostEnvironment();
    auto inventory = getInventory();
    
    // 8. Compose using the standard nah_compose function
    nah::core::CompositionOptions comp_opts;
    auto result = nah::core::nah_compose(component_app, host_env, *install_record, inventory, comp_opts);
    
    if (!result.ok) {
        return result;
    }
    
    // 9. Inject component-specific environment variables
    result.contract.environment["NAH_COMPONENT_ID"] = matched_component->id;
    result.contract.environment["NAH_COMPONENT_URI"] = uri;
    result.contract.environment["NAH_COMPONENT_PATH"] = parsed.component_path;
    
    if (!parsed.query.empty()) {
        result.contract.environment["NAH_COMPONENT_QUERY"] = parsed.query;
    }
    if (!parsed.fragment.empty()) {
        result.contract.environment["NAH_COMPONENT_FRAGMENT"] = parsed.fragment;
    }
    if (!referrer_uri.empty()) {
        result.contract.environment["NAH_COMPONENT_REFERRER"] = referrer_uri;
    }
    
    return result;
}

inline int NahHost::launchComponent(
    const std::string& uri,
    const std::string& referrer_uri,
    const std::vector<std::string>& args,
    std::function<void(const std::string&)> output_handler) const {
    
    auto result = composeComponentLaunch(uri, referrer_uri);
    if (!result.ok) {
        if (output_handler) {
            output_handler("Error: " + result.critical_error_context);
        }
        return 1;
    }
    
    return executeContract(result.contract, args, output_handler);
}

inline bool NahHost::canHandleComponentUri(const std::string& uri) const {
    auto parsed = nah::core::parse_component_uri(uri);
    if (!parsed.valid) {
        return false;
    }
    
    auto app_info = findApplication(parsed.app_id);
    if (!app_info) {
        return false;
    }
    
    auto app_decl = loadAppManifest(app_info->install_root);
    if (!app_decl || app_decl->components.empty()) {
        return false;
    }
    
    // Check if any component matches
    for (const auto& comp : app_decl->components) {
        if (matches_uri_pattern(comp.uri_pattern, uri)) {
            return true;
        }
    }
    
    return false;
}

inline std::vector<std::pair<std::string, nah::core::ComponentDecl>> 
NahHost::listAllComponents() const {
    std::vector<std::pair<std::string, nah::core::ComponentDecl>> result;
    
    auto apps = listApplications();
    for (const auto& app : apps) {
        auto manifest = loadAppManifest(app.install_root);
        if (manifest) {
            for (const auto& comp : manifest->components) {
                result.push_back({app.id, comp});
            }
        }
    }
    
    return result;
}

#endif // NAH_HOST_IMPLEMENTATION

} // namespace host
} // namespace nah

#endif // __cplusplus

#endif // NAH_HOST_H