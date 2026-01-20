/**
 * NAH CLI - Common utilities and types
 */

#pragma once

#include <nah/nah.h>
#include <nlohmann/json.hpp>
#include <string>
#include <optional>
#include <iostream>
#include <cstdlib>
#include <ctime>

namespace nah::cli {

// Portable getenv that avoids MSVC warnings
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

/**
 * Global options available to all commands.
 */
struct GlobalOptions {
    std::string root;              // --root
    bool json = false;             // --json
    bool trace = false;            // --trace
    bool verbose = false;          // -v, --verbose
    bool quiet = false;            // -q, --quiet
};

/**
 * Resolve the NAH root directory.
 * Priority: --root flag > NAH_ROOT env > ~/.nah
 */
inline std::string resolve_nah_root(const std::optional<std::string>& override_root) {
    // 1. Explicit override
    if (override_root && !override_root->empty()) {
        return *override_root;
    }
    
    // 2. Environment variable
    std::string env_root = safe_getenv("NAH_ROOT");
    if (!env_root.empty()) {
        return env_root;
    }
    
    // 3. Default: ~/.nah
    std::string home = safe_getenv("HOME");
    if (!home.empty()) {
        return home + "/.nah";
    }
    
    // Fallback for Windows
    std::string userprofile = safe_getenv("USERPROFILE");
    if (!userprofile.empty()) {
        return userprofile + "/.nah";
    }
    
    return ".nah";
}

/**
 * Get paths within the NAH root.
 */
struct NahPaths {
    std::string root;
    std::string apps;
    std::string naks;
    std::string host;
    std::string registry;
    std::string registry_apps;      // Consistent: registry/apps/ for app installs
    std::string registry_naks;      // Consistent: registry/naks/ for nak installs
    std::string staging;
};

inline NahPaths get_nah_paths(const std::string& nah_root) {
    NahPaths paths;
    paths.root = nah_root;
    paths.apps = nah_root + "/apps";
    paths.naks = nah_root + "/naks";
    paths.host = nah_root + "/host";
    paths.registry = nah_root + "/registry";
    paths.registry_apps = nah_root + "/registry/apps";      // Apps registry
    paths.registry_naks = nah_root + "/registry/naks";      // NAKs registry
    paths.staging = nah_root + "/staging";
    return paths;
}

/**
 * Warning collector for accumulating warnings during command execution.
 * In JSON mode, warnings are collected and output at the end.
 * In text mode, warnings are printed immediately to stderr.
 */
struct WarningCollector {
    std::vector<std::string> warnings;
    bool json_mode = false;
    bool quiet = false;
    
    void add(const std::string& msg) {
        if (json_mode) {
            warnings.push_back(msg);
        } else if (!quiet) {
            std::cerr << "Warning: " << msg << std::endl;
        }
    }
    
    void clear() { warnings.clear(); }
    bool empty() const { return warnings.empty(); }
    
    nlohmann::json to_json() const {
        return nlohmann::json(warnings);
    }
};

// Thread-local warning collector for the current command
inline WarningCollector& get_warning_collector() {
    static thread_local WarningCollector collector;
    return collector;
}

/**
 * Output utilities.
 */
inline void print_error(const std::string& msg, bool json_mode) {
    if (json_mode) {
        nlohmann::json j;
        j["ok"] = false;
        j["error"] = msg;
        auto& collector = get_warning_collector();
        if (!collector.empty()) {
            j["warnings"] = collector.to_json();
        }
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cerr << "Error: " << msg << std::endl;
    }
}

inline void print_warning(const std::string& msg, bool /* json_mode */) {
    get_warning_collector().add(msg);
}

inline void print_verbose_warning(const std::string& msg, bool json_mode, bool verbose) {
    if (verbose) {
        print_warning(msg, json_mode);
    }
}

inline void print_success(const std::string& msg, bool json_mode) {
    if (!json_mode) {
        std::cout << msg << std::endl;
    }
}

inline void output_json(const nlohmann::json& j) {
    // Include any collected warnings in the output
    auto& collector = get_warning_collector();
    if (!collector.empty() && !j.contains("warnings")) {
        nlohmann::json output = j;
        output["warnings"] = collector.to_json();
        std::cout << output.dump(2) << std::endl;
    } else {
        std::cout << j.dump(2) << std::endl;
    }
}

inline void init_warning_collector(bool json_mode, bool quiet) {
    auto& collector = get_warning_collector();
    collector.clear();
    collector.json_mode = json_mode;
    collector.quiet = quiet;
}

/**
 * Load host environment from NAH root's host.json.
 */
inline nah::core::HostEnvironment load_host_environment(const std::string& nah_root) {
    auto paths = get_nah_paths(nah_root);
    std::string host_json_path = paths.host + "/host.json";
    
    auto content = nah::fs::read_file(host_json_path);
    if (!content) {
        // Return empty environment
        return nah::core::HostEnvironment{};
    }
    
    auto result = nah::json::parse_host_environment(*content, host_json_path);
    if (!result.ok) {
        return nah::core::HostEnvironment{};
    }
    
    return result.value;
}

/**
 * Load runtime inventory (all installed NAKs) from NAH root.
 */
inline nah::core::RuntimeInventory load_inventory(const std::string& nah_root) {
    auto paths = get_nah_paths(nah_root);
    return nah::fs::load_inventory_from_directory(paths.registry_naks);
}

/**
 * Parse target string into id and optional version.
 * Format: "id" or "id@version"
 */
struct ParsedTarget {
    std::string id;
    std::optional<std::string> version;
};

inline ParsedTarget parse_target(const std::string& target) {
    ParsedTarget result;
    
    auto at_pos = target.rfind('@');
    if (at_pos != std::string::npos && at_pos > 0) {
        result.id = target.substr(0, at_pos);
        result.version = target.substr(at_pos + 1);
    } else {
        result.id = target;
    }
    
    return result;
}

/**
 * Ensure NAH directory structure exists.
 */
inline bool ensure_nah_structure(const std::string& nah_root) {
    auto paths = get_nah_paths(nah_root);
    
    return nah::fs::create_directories(paths.apps) &&
           nah::fs::create_directories(paths.naks) &&
           nah::fs::create_directories(paths.host) &&
           nah::fs::create_directories(paths.registry_apps) &&
           nah::fs::create_directories(paths.registry_naks) &&
           nah::fs::create_directories(paths.staging);
}

/**
 * Get current timestamp in RFC3339 format.
 */
inline std::string get_current_timestamp() {
    auto now = std::time(nullptr);
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &now);
#else
    gmtime_r(&now, &tm_buf);
#endif
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buffer);
}

} // namespace nah::cli

// Add to core namespace for use in install.cpp
namespace nah::core {
    using nah::cli::get_current_timestamp;
}
