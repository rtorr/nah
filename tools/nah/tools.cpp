#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include "nah/nahhost.hpp"
#include "nah/manifest.hpp"
#include "nah/manifest_builder.hpp"
#include "nah/manifest_generate.hpp"
#include "nah/platform.hpp"
#include "nah/contract.hpp"
#include "nah/packaging.hpp"
#include "nah/nak_selection.hpp"
#include "nah/materializer.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>
#include <optional>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// Package Type Detection
// ============================================================================

enum class PackageType {
    Unknown,
    App,
    Nak
};

// Detect package type from file extension or directory contents
PackageType detect_package_type(const std::string& source) {
    // Check file extension first
    if (source.size() >= 4) {
        std::string ext = source.substr(source.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".nap") return PackageType::App;
        if (ext == ".nak") return PackageType::Nak;
    }
    
    // Check URL extension
    std::string url_path = source;
    auto query_pos = url_path.find('?');
    if (query_pos != std::string::npos) {
        url_path = url_path.substr(0, query_pos);
    }
    if (url_path.size() >= 4) {
        std::string ext = url_path.substr(url_path.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".nap") return PackageType::App;
        if (ext == ".nak") return PackageType::Nak;
    }
    
    // For directories, check contents
    if (fs::exists(source) && fs::is_directory(source)) {
        // NAK: has META/nak.json
        if (fs::exists(fs::path(source) / "META" / "nak.json")) {
            return PackageType::Nak;
        }
        // App: has manifest.json or embedded manifest in binary
        if (fs::exists(fs::path(source) / "manifest.json")) {
            return PackageType::App;
        }
        // Check for binary with embedded manifest
        for (const auto& entry : fs::directory_iterator(source)) {
            if (entry.is_regular_file()) {
                auto result = nah::read_manifest_section(entry.path().string());
                if (result.ok) {
                    return PackageType::App;
                }
            }
        }
        // Check bin/ subdirectory
        fs::path bin_dir = fs::path(source) / "bin";
        if (fs::exists(bin_dir) && fs::is_directory(bin_dir)) {
            for (const auto& entry : fs::directory_iterator(bin_dir)) {
                if (entry.is_regular_file()) {
                    auto result = nah::read_manifest_section(entry.path().string());
                    if (result.ok) {
                        return PackageType::App;
                    }
                }
            }
        }
    }
    
    return PackageType::Unknown;
}

// Detect if an installed target is an app or NAK
PackageType detect_installed_type(const std::string& nah_root, const std::string& id, const std::string& version) {
    // Check app registry
    auto host = nah::NahHost::create(nah_root);
    auto app_result = host->findApplication(id, version);
    bool is_app = app_result.isOk();
    
    // Check NAK registry
    auto nak_entries = nah::scan_nak_registry(nah_root);
    bool is_nak = false;
    for (const auto& entry : nak_entries) {
        if (entry.id == id && (version.empty() || entry.version == version)) {
            is_nak = true;
            break;
        }
    }
    
    if (is_app && is_nak) {
        return PackageType::Unknown; // Ambiguous
    }
    if (is_app) return PackageType::App;
    if (is_nak) return PackageType::Nak;
    return PackageType::Unknown;
}

// ============================================================================
// Root Auto-Detection
// ============================================================================

// Track whether we're using the default root
bool g_using_default_root = false;
bool g_created_default_root = false;

std::string get_home_dir() {
    const char* home = std::getenv("HOME");
    if (home) return home;
#ifdef _WIN32
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) return userprofile;
#endif
    return "";
}

std::string get_default_nah_root() {
    std::string home = get_home_dir();
    if (home.empty()) return "";
    return home + "/.nah";
}

// Check if a directory looks like a valid NAH root
// Requires host/ directory AND at least one of: apps/, naks/, or .nah marker
bool looks_like_nah_root(const fs::path& dir) {
    std::error_code ec;
    
    // Must have host/ directory
    if (!fs::exists(dir / "host", ec) || !fs::is_directory(dir / "host", ec)) {
        return false;
    }
    
    // Must also have apps/ or naks/ or .nah marker to distinguish from random directories
    if (fs::exists(dir / "apps", ec) && fs::is_directory(dir / "apps", ec)) {
        return true;
    }
    if (fs::exists(dir / "naks", ec) && fs::is_directory(dir / "naks", ec)) {
        return true;
    }
    if (fs::exists(dir / ".nah", ec)) {
        return true;
    }
    
    return false;
}

std::string auto_detect_nah_root(const std::string& explicit_root) {
    g_using_default_root = false;
    
    // 1. Explicit --root flag or NAH_ROOT env var
    if (!explicit_root.empty()) {
        return explicit_root;
    }
    
    // 2. Walk up from cwd looking for a valid NAH root
    std::error_code ec;
    fs::path current = fs::current_path(ec);
    if (!ec) {
        while (!current.empty() && current != current.root_path()) {
            // Check for .nah marker directory (explicit marker)
            if (fs::exists(current / ".nah", ec)) {
                return current.string();
            }
            // Check for valid NAH root structure (host/ + apps/ or naks/)
            if (looks_like_nah_root(current)) {
                return current.string();
            }
            current = current.parent_path();
        }
    }
    
    // 3. Default to ~/.nah
    g_using_default_root = true;
    return get_default_nah_root();
}

// ============================================================================
// Global Options
// ============================================================================

struct GlobalOptions {
    std::string root;  // Empty = auto-detect
    std::string profile;
    bool json = false;
    bool trace = false;
    bool verbose = false;
    bool quiet = false;
};

// ============================================================================
// Helper Functions
// ============================================================================

// Format root path for display, adding "(default)" suffix if using default
std::string format_root_path(const std::string& root) {
    if (g_using_default_root) {
        return root + " (default)";
    }
    return root;
}

// Ensure default root exists, create if needed
bool ensure_default_root_exists(const std::string& root) {
    if (!g_using_default_root) return true;
    
    std::error_code ec;
    if (fs::exists(root, ec)) return true;
    
    // Create the default root structure
    fs::create_directories(fs::path(root) / "host" / "profiles", ec);
    if (ec) return false;
    
    fs::create_directories(fs::path(root) / "apps", ec);
    fs::create_directories(fs::path(root) / "naks", ec);
    fs::create_directories(fs::path(root) / "registry" / "installs", ec);
    fs::create_directories(fs::path(root) / "registry" / "naks", ec);
    
    // Create default profile
    std::ofstream profile(fs::path(root) / "host" / "profiles" / "default.json");
    profile << R"({
  "nak": {
    "binding_mode": "canonical"
  }
})";
    profile.close();
    
    // Create profile.current symlink (use platform helper for cross-platform)
    std::string link_path = (fs::path(root) / "host" / "profile.current").string();
    nah::atomic_update_symlink(link_path, "profiles/default.json");
    
    g_created_default_root = true;
    std::cerr << "Created default NAH root at " << root << std::endl;
    std::cerr << "(Configure with NAH_ROOT or --root)" << std::endl << std::endl;
    
    return true;
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// ANSI color codes (disabled if not a terminal)
namespace color {
    bool enabled = true;
    
    void init() {
        #ifdef _WIN32
        enabled = false;  // Simplified: disable on Windows
        #else
        enabled = isatty(fileno(stderr));
        #endif
    }
    
    std::string red(const std::string& s)    { return enabled ? "\033[31m" + s + "\033[0m" : s; }
    std::string green(const std::string& s)  { return enabled ? "\033[32m" + s + "\033[0m" : s; }
    std::string yellow(const std::string& s) { return enabled ? "\033[33m" + s + "\033[0m" : s; }
    std::string blue(const std::string& s)   { return enabled ? "\033[34m" + s + "\033[0m" : s; }
    std::string bold(const std::string& s)   { return enabled ? "\033[1m" + s + "\033[0m" : s; }
    std::string dim(const std::string& s)    { return enabled ? "\033[2m" + s + "\033[0m" : s; }
}

// Levenshtein distance for command suggestions
int levenshtein_distance(const std::string& s1, const std::string& s2) {
    const size_t m = s1.size(), n = s2.size();
    if (m == 0) return static_cast<int>(n);
    if (n == 0) return static_cast<int>(m);
    
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));
    for (size_t i = 0; i <= m; ++i) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= n; ++j) dp[0][j] = static_cast<int>(j);
    
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            dp[i][j] = std::min({dp[i-1][j] + 1, dp[i][j-1] + 1, dp[i-1][j-1] + cost});
        }
    }
    return dp[m][n];
}

// Find similar commands for suggestions
std::vector<std::string> find_similar_commands(const std::string& input, 
                                                const std::vector<std::string>& valid_commands,
                                                int max_distance = 3) {
    std::vector<std::pair<int, std::string>> candidates;
    for (const auto& cmd : valid_commands) {
        int dist = levenshtein_distance(input, cmd);
        if (dist <= max_distance) {
            candidates.push_back({dist, cmd});
        }
    }
    std::sort(candidates.begin(), candidates.end());
    
    std::vector<std::string> result;
    for (const auto& [dist, cmd] : candidates) {
        result.push_back(cmd);
        if (result.size() >= 3) break;  // Max 3 suggestions
    }
    return result;
}

// Error context for better messages
struct ErrorContext {
    std::string file_path;
    int line_number = -1;
    std::string line_content;
    std::string field_name;
    std::vector<std::string> valid_values;
    std::string hint;
    std::string help_command;
};

void print_error(const std::string& msg, bool json_mode, const ErrorContext& ctx = {}) {
    if (json_mode) {
        nlohmann::json j;
        j["error"] = msg;
        if (!ctx.file_path.empty()) j["file"] = ctx.file_path;
        if (ctx.line_number > 0) j["line"] = ctx.line_number;
        if (!ctx.hint.empty()) j["hint"] = ctx.hint;
        if (!ctx.valid_values.empty()) j["valid_values"] = ctx.valid_values;
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cerr << color::red("error: ") << msg << std::endl;
        
        // Show file location if available
        if (!ctx.file_path.empty()) {
            std::cerr << color::dim("  --> ") << ctx.file_path;
            if (ctx.line_number > 0) {
                std::cerr << ":" << ctx.line_number;
            }
            std::cerr << std::endl;
        }
        
        // Show the problematic line with context
        if (ctx.line_number > 0 && !ctx.line_content.empty()) {
            std::string line_num = std::to_string(ctx.line_number);
            std::string padding(line_num.size(), ' ');
            std::cerr << color::dim(padding + " |") << std::endl;
            std::cerr << color::dim(line_num + " | ") << ctx.line_content << std::endl;
            std::cerr << color::dim(padding + " |") << std::endl;
        }
        
        // Show valid values if available
        if (!ctx.valid_values.empty()) {
            std::cerr << std::endl << "Valid values: ";
            for (size_t i = 0; i < ctx.valid_values.size(); ++i) {
                if (i > 0) std::cerr << ", ";
                std::cerr << color::green(ctx.valid_values[i]);
            }
            std::cerr << std::endl;
        }
        
        // Show hint if available
        if (!ctx.hint.empty()) {
            std::cerr << std::endl << color::blue("hint: ") << ctx.hint << std::endl;
        }
        
        // Show help command if available
        if (!ctx.help_command.empty()) {
            std::cerr << std::endl << "For more information, try: " 
                      << color::bold(ctx.help_command) << std::endl;
        }
    }
}

void print_warning(const std::string& msg, bool json_mode) {
    if (json_mode) return;  // Warnings go in the JSON structure
    std::cerr << color::yellow("warning: ") << msg << std::endl;
}

// Check if NAH root exists and is valid
bool check_nah_root(const std::string& root, bool json_mode) {
    if (!fs::exists(root)) {
        ErrorContext ctx;
        ctx.hint = "Initialize a new NAH root with: nah profile init " + root;
        print_error("NAH root directory does not exist: " + root, json_mode, ctx);
        return false;
    }
    
    if (!fs::exists(root + "/host")) {
        ErrorContext ctx;
        ctx.hint = "This directory exists but is not a valid NAH root.\n"
                   "       Initialize it with: nah profile init " + root;
        print_error("Invalid NAH root (missing host/ directory): " + root, json_mode, ctx);
        return false;
    }
    
    return true;
}

// Parse target string (id[@version]) with helpful errors
bool parse_target(const std::string& target, std::string& id, std::string& version,
                  bool json_mode, const std::string& entity_type = "application") {
    if (target.empty()) {
        ErrorContext ctx;
        ctx.hint = "Specify a target as: <id> or <id>@<version>\n"
                   "       Example: com.example.myapp or com.example.myapp@1.0.0";
        print_error(entity_type + " target is required", json_mode, ctx);
        return false;
    }
    
    id = target;
    version.clear();
    
    auto at_pos = target.find('@');
    if (at_pos != std::string::npos) {
        id = target.substr(0, at_pos);
        version = target.substr(at_pos + 1);
        
        if (id.empty()) {
            ErrorContext ctx;
            ctx.hint = "The format is: <id>@<version>, not @<version>";
            print_error("Invalid target format: missing ID before '@'", json_mode, ctx);
            return false;
        }
        
        if (version.empty()) {
            ErrorContext ctx;
            ctx.hint = "Either specify a version after '@' or omit '@' entirely";
            print_error("Invalid target format: missing version after '@'", json_mode, ctx);
            return false;
        }
    }
    
    // Validate ID format (reverse domain notation)
    if (id.find('.') == std::string::npos) {
        print_warning("ID '" + id + "' is not in reverse domain notation (e.g., com.example.app)", json_mode);
    }
    
    return true;
}

// Suggest available apps/NAKs when target not found
void suggest_available_targets(const std::string& nah_root, const std::string& missing_id,
                               const std::string& entity_type, bool json_mode) {
    if (json_mode) return;
    
    std::vector<std::string> available;
    
    bool include_apps = (entity_type == "application" || entity_type == "app" || entity_type == "package");
    bool include_naks = (entity_type == "NAK" || entity_type == "nak" || entity_type == "package");
    
    if (include_apps) {
        auto host = nah::NahHost::create(nah_root);
        for (const auto& app : host->listApplications()) {
            available.push_back(app.id);
        }
    }
    if (include_naks) {
        for (const auto& entry : nah::scan_nak_registry(nah_root)) {
            available.push_back(entry.id + "@" + entry.version);
        }
    }
    
    if (available.empty()) {
        std::cerr << std::endl << "No packages are currently installed." << std::endl;
        std::cerr << "Install with: " << color::bold("nah install <package>") << std::endl;
        return;
    }
    
    // Find similar IDs
    auto suggestions = find_similar_commands(missing_id, available, 5);
    
    if (!suggestions.empty()) {
        std::cerr << std::endl << "Did you mean?" << std::endl;
        for (const auto& s : suggestions) {
            std::cerr << "  " << color::green(s) << std::endl;
        }
    } else if (available.size() <= 10) {
        std::cerr << std::endl << "Available:" << std::endl;
        for (const auto& s : available) {
            std::cerr << "  " << s << std::endl;
        }
    } else {
        std::cerr << std::endl << "Run " << color::bold("nah list") 
                  << " to see all installed packages." << std::endl;
    }
}

// ============================================================================
// App Commands
// ============================================================================

int cmd_app_list(const GlobalOptions& opts) {
    if (!check_nah_root(opts.root, opts.json)) {
        return 1;
    }
    
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
    if (!check_nah_root(opts.root, opts.json)) {
        return 1;
    }
    
    auto host = nah::NahHost::create(opts.root);
    
    // Parse target: id[@version]
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "application")) {
        return 1;
    }
    
    auto result = host->findApplication(id, version);
    if (result.isErr()) {
        print_error("Application not found: " + target, opts.json);
        suggest_available_targets(opts.root, id, "application", opts.json);
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

int cmd_app_install(const GlobalOptions& opts, const std::string& source, bool force) {
    // Ensure root exists (creates default if needed)
    if (!ensure_default_root_exists(opts.root)) {
        print_error("Failed to create NAH root at " + opts.root, opts.json);
        return 1;
    }
    
    nah::AppInstallOptions install_opts;
    install_opts.nah_root = opts.root;
    install_opts.profile_name = opts.profile;
    install_opts.force = force;
    install_opts.installed_by = "nah-cli";
    
    // Use unified install_app which handles file paths, file: URLs, and https:// URLs
    auto result = nah::install_app(source, install_opts);
    
    if (!result.ok) {
        print_error(result.error, opts.json);
        return 1;
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["success"] = true;
        j["app_id"] = result.app_id;
        j["app_version"] = result.app_version;
        j["install_root"] = result.install_root;
        j["record_path"] = result.record_path;
        j["instance_id"] = result.instance_id;
        if (!result.nak_id.empty()) {
            j["nak_id"] = result.nak_id;
            j["nak_version"] = result.nak_version;
        }
        if (!result.package_hash.empty()) {
            j["package_hash"] = result.package_hash;
        }
        std::cout << j.dump(2) << std::endl;
    } else if (!opts.quiet) {
        std::cout << "Installed: " << result.app_id << "@" << result.app_version 
                  << " → " << format_root_path(opts.root) << std::endl;
        if (result.nak_id.empty()) {
            std::cout << "  (standalone app, no NAK dependency)" << std::endl;
        }
        if (opts.verbose) {
            std::cout << "  Path: " << result.install_root << std::endl;
            std::cout << "  Instance: " << result.instance_id << std::endl;
            if (!result.nak_id.empty()) {
                std::cout << "  NAK: " << result.nak_id << "@" << result.nak_version << std::endl;
            }
            if (!result.package_hash.empty()) {
                std::cout << "  Hash: " << result.package_hash << std::endl;
            }
        }
    }
    
    return 0;
}

int cmd_app_uninstall(const GlobalOptions& opts, const std::string& target) {
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "application")) {
        return 1;
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
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "application")) {
        return 1;
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
    
    // Create a simple main.cpp that compiles without dependencies
    std::string main_cpp = R"cpp(#include <iostream>
#include <cstdlib>

int main() {
    // NAH sets these environment variables at launch
    const char* app_id = std::getenv("NAH_APP_ID");
    const char* app_version = std::getenv("NAH_APP_VERSION");
    const char* app_root = std::getenv("NAH_APP_ROOT");
    
    std::cout << "Hello from " << (app_id ? app_id : "NAH app") << std::endl;
    
    if (app_version) {
        std::cout << "Version: " << app_version << std::endl;
    }
    if (app_root) {
        std::cout << "App root: " << app_root << std::endl;
    }
    
    return 0;
}
)cpp";
    
    std::ofstream file(dir + "/main.cpp");
    file << main_cpp;
    file.close();
    
    // Create manifest.json for the app
    std::string manifest = R"({
  "app": {
    "id": "com.example.myapp",
    "version": "1.0.0",
    "entrypoint": "bin/myapp"
  }
})";
    
    std::ofstream manifest_file(dir + "/manifest.json");
    manifest_file << manifest;
    manifest_file.close();
    
    // Create README.md
    std::string readme = R"(# NAH Application

## Build

```bash
g++ -o bin/myapp main.cpp
```

## Package

```bash
nah manifest generate manifest.json -o manifest.nah
nah pack .
```

## Install

```bash
nah install myapp-1.0.0.nap
```
)";
    
    std::ofstream readme_file(dir + "/README.md");
    readme_file << readme;
    readme_file.close();
    
    if (!opts.quiet) {
        std::cout << "Created app skeleton in " << dir << std::endl;
        std::cout << "Files created:" << std::endl;
        std::cout << "  " << dir << "/main.cpp" << std::endl;
        std::cout << "  " << dir << "/manifest.json" << std::endl;
        std::cout << "  " << dir << "/README.md" << std::endl;
        std::cout << std::endl;
        std::cout << "Next steps:" << std::endl;
        std::cout << "  1. Edit manifest.json with your app details" << std::endl;
        std::cout << "  2. g++ -o bin/myapp main.cpp" << std::endl;
        std::cout << "  3. nah pack " << dir << std::endl;
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
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "NAK")) {
        return 1;
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
                j["has_loaders"] = result.record.has_loaders();
                if (result.record.has_loaders()) {
                    nlohmann::json loaders_json;
                    for (const auto& [name, loader] : result.record.loaders) {
                        loaders_json[name] = {
                            {"exec_path", loader.exec_path},
                            {"args_template", loader.args_template}
                        };
                    }
                    j["loaders"] = loaders_json;
                }
                std::cout << j.dump(2) << std::endl;
            } else {
                std::cout << "NAK: " << result.record.nak.id << " v" << result.record.nak.version << std::endl;
                std::cout << "Root: " << result.record.paths.root << std::endl;
                std::cout << "Resource Root: " << result.record.paths.resource_root << std::endl;
                std::cout << "Lib Dirs:" << std::endl;
                for (const auto& lib : result.record.paths.lib_dirs) {
                    std::cout << "  " << lib << std::endl;
                }
                if (result.record.has_loaders()) {
                    std::cout << "Loaders:" << std::endl;
                    for (const auto& [name, loader] : result.record.loaders) {
                        std::cout << "  " << name << ": " << loader.exec_path << std::endl;
                    }
                }
            }
            return 0;
        }
    }
    
    print_error("NAK not found: " + target, opts.json);
    suggest_available_targets(opts.root, target, "NAK", opts.json);
    return 1;
}

int cmd_nak_install(const GlobalOptions& opts, const std::string& source, bool force) {
    // Ensure root exists (creates default if needed)
    if (!ensure_default_root_exists(opts.root)) {
        print_error("Failed to create NAH root at " + opts.root, opts.json);
        return 1;
    }
    
    nah::NakInstallOptions install_opts;
    install_opts.nah_root = opts.root;
    install_opts.force = force;
    install_opts.installed_by = "nah-cli";
    
    // Use unified install_nak which handles file paths, file: URLs, and https:// URLs
    auto result = nah::install_nak(source, install_opts);
    
    if (!result.ok) {
        print_error(result.error, opts.json);
        return 1;
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["success"] = true;
        j["nak_id"] = result.nak_id;
        j["nak_version"] = result.nak_version;
        j["install_root"] = result.install_root;
        j["record_path"] = result.record_path;
        if (!result.package_hash.empty()) {
            j["package_hash"] = result.package_hash;
        }
        std::cout << j.dump(2) << std::endl;
    } else if (!opts.quiet) {
        std::cout << "Installed: " << result.nak_id << "@" << result.nak_version 
                  << " → " << format_root_path(opts.root) << std::endl;
        if (opts.verbose) {
            std::cout << "  Path: " << result.install_root << std::endl;
            if (!result.package_hash.empty()) {
                std::cout << "  Hash: " << result.package_hash << std::endl;
            }
        }
    }
    
    return 0;
}

int cmd_nak_path(const GlobalOptions& opts, const std::string& target) {
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "NAK")) {
        return 1;
    }
    
    if (version.empty()) {
        ErrorContext ctx;
        ctx.hint = "The path command requires an exact version.\n"
                   "       Example: nah nak path com.example.sdk@1.0.0";
        print_error("NAK version is required for path lookup", opts.json, ctx);
        return 1;
    }
    
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
    suggest_available_targets(opts.root, target, "NAK", opts.json);
    return 1;
}

int cmd_nak_init(const GlobalOptions& opts, const std::string& dir) {
    fs::create_directories(dir + "/META");
    fs::create_directories(dir + "/lib");
    fs::create_directories(dir + "/resources");
    fs::create_directories(dir + "/bin");
    
    std::string nak_json = R"({
  "nak": {
    "id": "com.example.nak",
    "version": "1.0.0"
  },
  "paths": {
    "resource_root": "resources",
    "lib_dirs": ["lib"]
  },
  "environment": {},
  "loader": {},
  "execution": {
    "cwd": "{NAH_APP_ROOT}"
  }
})";
    
    std::ofstream file(dir + "/META/nak.json");
    file << nak_json;
    file.close();
    
    // Create README.md
    std::string readme = R"(# NAH NAK (Native App Kit)

This is a NAK skeleton for building an SDK or framework.

## Next Steps

1. Edit `META/nak.json` to update:
   - `nak.id`: Your NAK's unique identifier (e.g., `com.yourcompany.mysdk`)
   - `nak.version`: Your NAK's version
   - `paths.lib_dirs`: Directories containing shared libraries

2. Add your libraries to `lib/`:
   - `lib/libmysdk.so` (Linux)
   - `lib/libmysdk.dylib` (macOS)

3. Optional: Add a loader binary to `bin/` and configure `loader`

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
        std::cout << "  " << dir << "/META/nak.json" << std::endl;
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
// Forward Declarations
// ============================================================================

// These are defined later but needed by unified commands
int cmd_profile_init(const GlobalOptions& opts, const std::string& dir);
int cmd_app_init(const GlobalOptions& opts, const std::string& dir);
int cmd_nak_init(const GlobalOptions& opts, const std::string& dir);
int cmd_doctor(const GlobalOptions& opts, const std::string& target, bool fix);

// ============================================================================
// Unified Commands (New CLI)
// ============================================================================

// Unified install command with auto-detection
int cmd_install(const GlobalOptions& opts, const std::string& source, bool force,
                std::optional<PackageType> force_type) {
    // Ensure root exists (creates default if needed)
    if (!ensure_default_root_exists(opts.root)) {
        print_error("Failed to create NAH root at " + opts.root, opts.json);
        return 1;
    }
    
    // Detect package type
    PackageType pkg_type = force_type.value_or(detect_package_type(source));
    
    if (pkg_type == PackageType::Unknown) {
        ErrorContext ctx;
        ctx.hint = "The source doesn't have a recognized extension (.nap or .nak)\n"
                   "       and couldn't be detected from contents.\n\n"
                   "       For apps: use .nap extension or create manifest.json\n"
                   "       For NAKs: use .nak extension or create META/nak.json\n\n"
                   "       To force a type: nah install " + source + " --app\n"
                   "                         nah install " + source + " --nak";
        print_error("Cannot detect package type for: " + source, opts.json, ctx);
        return 1;
    }
    
    if (pkg_type == PackageType::App) {
        // Check if source is a directory - need to pack first
        if (fs::exists(source) && fs::is_directory(source)) {
            // Pack to temp file, then install
            auto pack_result = nah::pack_nap(source);
            if (!pack_result.ok) {
                print_error("Failed to pack app: " + pack_result.error, opts.json);
                return 1;
            }
            
            // Create temp file
            std::string temp_path = (fs::temp_directory_path() / ("nah_install_" + std::to_string(std::time(nullptr)) + ".nap")).string();
            std::ofstream temp_file(temp_path, std::ios::binary);
            if (!temp_file) {
                print_error("Failed to create temp file", opts.json);
                return 1;
            }
            temp_file.write(reinterpret_cast<const char*>(pack_result.archive_data.data()),
                           static_cast<std::streamsize>(pack_result.archive_data.size()));
            temp_file.close();
            
            // Install from temp file
            nah::AppInstallOptions install_opts;
            install_opts.nah_root = opts.root;
            install_opts.profile_name = opts.profile;
            install_opts.force = force;
            install_opts.installed_by = "nah-cli";
            
            auto result = nah::install_app(temp_path, install_opts);
            fs::remove(temp_path); // Clean up
            
            if (!result.ok) {
                print_error(result.error, opts.json);
                return 1;
            }
            
            if (opts.json) {
                nlohmann::json j;
                j["success"] = true;
                j["type"] = "app";
                j["app_id"] = result.app_id;
                j["app_version"] = result.app_version;
                j["install_root"] = result.install_root;
                if (!result.nak_id.empty()) {
                    j["nak_id"] = result.nak_id;
                    j["nak_version"] = result.nak_version;
                }
                std::cout << j.dump(2) << std::endl;
            } else if (!opts.quiet) {
                std::cout << "Installed: " << result.app_id << "@" << result.app_version 
                          << " → " << format_root_path(opts.root) << std::endl;
                if (result.nak_id.empty()) {
                    std::cout << "  (standalone app, no NAK dependency)" << std::endl;
                }
            }
            return 0;
        }
        
        // Install app from file or URL
        nah::AppInstallOptions install_opts;
        install_opts.nah_root = opts.root;
        install_opts.profile_name = opts.profile;
        install_opts.force = force;
        install_opts.installed_by = "nah-cli";
        
        auto result = nah::install_app(source, install_opts);
        
        if (!result.ok) {
            print_error(result.error, opts.json);
            return 1;
        }
        
        if (opts.json) {
            nlohmann::json j;
            j["success"] = true;
            j["type"] = "app";
            j["app_id"] = result.app_id;
            j["app_version"] = result.app_version;
            j["install_root"] = result.install_root;
            j["instance_id"] = result.instance_id;
            if (!result.nak_id.empty()) {
                j["nak_id"] = result.nak_id;
                j["nak_version"] = result.nak_version;
            }
            if (!result.package_hash.empty()) {
                j["package_hash"] = result.package_hash;
            }
            std::cout << j.dump(2) << std::endl;
        } else if (!opts.quiet) {
            std::cout << "Installed: " << result.app_id << "@" << result.app_version 
                      << " → " << format_root_path(opts.root) << std::endl;
            if (result.nak_id.empty()) {
                std::cout << "  (standalone app, no NAK dependency)" << std::endl;
            }
        }
        return 0;
    } else {
        // Install NAK
        // Check if source is a directory - need to pack first
        if (fs::exists(source) && fs::is_directory(source)) {
            auto pack_result = nah::pack_nak(source);
            if (!pack_result.ok) {
                print_error("Failed to pack NAK: " + pack_result.error, opts.json);
                return 1;
            }
            
            std::string temp_path = (fs::temp_directory_path() / ("nah_install_" + std::to_string(std::time(nullptr)) + ".nak")).string();
            std::ofstream temp_file(temp_path, std::ios::binary);
            if (!temp_file) {
                print_error("Failed to create temp file", opts.json);
                return 1;
            }
            temp_file.write(reinterpret_cast<const char*>(pack_result.archive_data.data()),
                           static_cast<std::streamsize>(pack_result.archive_data.size()));
            temp_file.close();
            
            nah::NakInstallOptions install_opts;
            install_opts.nah_root = opts.root;
            install_opts.force = force;
            install_opts.installed_by = "nah-cli";
            
            auto result = nah::install_nak(temp_path, install_opts);
            fs::remove(temp_path);
            
            if (!result.ok) {
                print_error(result.error, opts.json);
                return 1;
            }
            
            if (opts.json) {
                nlohmann::json j;
                j["success"] = true;
                j["type"] = "nak";
                j["nak_id"] = result.nak_id;
                j["nak_version"] = result.nak_version;
                j["install_root"] = result.install_root;
                std::cout << j.dump(2) << std::endl;
            } else if (!opts.quiet) {
                std::cout << "Installed: " << result.nak_id << "@" << result.nak_version 
                          << " → " << format_root_path(opts.root) << std::endl;
            }
            return 0;
        }
        
        // Install NAK from file or URL
        nah::NakInstallOptions install_opts;
        install_opts.nah_root = opts.root;
        install_opts.force = force;
        install_opts.installed_by = "nah-cli";
        
        auto result = nah::install_nak(source, install_opts);
        
        if (!result.ok) {
            print_error(result.error, opts.json);
            return 1;
        }
        
        if (opts.json) {
            nlohmann::json j;
            j["success"] = true;
            j["type"] = "nak";
            j["nak_id"] = result.nak_id;
            j["nak_version"] = result.nak_version;
            j["install_root"] = result.install_root;
            if (!result.package_hash.empty()) {
                j["package_hash"] = result.package_hash;
            }
            std::cout << j.dump(2) << std::endl;
        } else if (!opts.quiet) {
            std::cout << "Installed: " << result.nak_id << "@" << result.nak_version 
                      << " → " << format_root_path(opts.root) << std::endl;
        }
        return 0;
    }
}

// Unified uninstall command with auto-detection
int cmd_uninstall(const GlobalOptions& opts, const std::string& target,
                  std::optional<PackageType> force_type) {
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "package")) {
        return 1;
    }
    
    // Detect type from registry
    PackageType pkg_type = force_type.value_or(detect_installed_type(opts.root, id, version));
    
    if (pkg_type == PackageType::Unknown) {
        // Check if both exist (ambiguous) or neither exists
        auto host = nah::NahHost::create(opts.root);
        auto app_result = host->findApplication(id, version);
        auto nak_entries = nah::scan_nak_registry(opts.root);
        
        bool app_exists = app_result.isOk();
        bool nak_exists = false;
        for (const auto& entry : nak_entries) {
            if (entry.id == id && (version.empty() || entry.version == version)) {
                nak_exists = true;
                break;
            }
        }
        
        if (app_exists && nak_exists) {
            ErrorContext ctx;
            ctx.hint = "Both an app and a NAK exist with this ID.\n"
                       "       Use --app or --nak to specify which to uninstall:\n"
                       "         nah uninstall " + target + " --app\n"
                       "         nah uninstall " + target + " --nak";
            print_error("Ambiguous ID: " + target, opts.json, ctx);
            return 1;
        } else {
            ErrorContext ctx;
            ctx.hint = "Run 'nah list' to see installed packages.";
            print_error("Not installed: " + target, opts.json, ctx);
            suggest_available_targets(opts.root, id, "package", opts.json);
            return 1;
        }
    }
    
    if (pkg_type == PackageType::App) {
        auto result = nah::uninstall_app(opts.root, id, version);
        if (!result.ok) {
            print_error(result.error, opts.json);
            return 1;
        }
        
        if (opts.json) {
            nlohmann::json j;
            j["success"] = true;
            j["type"] = "app";
            j["uninstalled"] = target;
            std::cout << j.dump(2) << std::endl;
        } else if (!opts.quiet) {
            std::cout << "Uninstalled app: " << target << std::endl;
        }
        return 0;
    } else {
        // Uninstall NAK - need to implement or use existing function
        // For now, we need to add NAK uninstall capability
        ErrorContext ctx;
        ctx.hint = "NAK uninstall is not yet implemented.\n"
                   "       You can manually remove the NAK from the registry.";
        print_error("Cannot uninstall NAK: " + target, opts.json, ctx);
        return 1;
    }
}

// Unified list command
int cmd_list(const GlobalOptions& opts, bool apps_only, bool naks_only) {
    if (!check_nah_root(opts.root, opts.json)) {
        return 1;
    }
    
    auto host = nah::NahHost::create(opts.root);
    auto apps = host->listApplications();
    auto nak_entries = nah::scan_nak_registry(opts.root);
    
    // Build NAK usage map
    std::map<std::string, int> nak_usage; // nak_id@version -> count
    for (const auto& app : apps) {
        std::string record_content = read_file(app.record_path);
        if (!record_content.empty()) {
            auto record_result = nah::parse_app_install_record_full(record_content, app.record_path);
            if (record_result.ok && !record_result.record.nak.version.empty()) {
                std::string nak_key = record_result.record.app.nak_id + "@" + record_result.record.nak.version;
                nak_usage[nak_key]++;
            }
        }
    }
    
    if (opts.json) {
        nlohmann::json j;
        
        if (!apps_only || (!apps_only && !naks_only)) {
            nlohmann::json apps_arr = nlohmann::json::array();
            for (const auto& app : apps) {
                nlohmann::json a;
                a["id"] = app.id;
                a["version"] = app.version;
                a["instance_id"] = app.instance_id;
                a["install_root"] = app.install_root;
                
                // Get NAK info
                std::string record_content = read_file(app.record_path);
                if (!record_content.empty()) {
                    auto record_result = nah::parse_app_install_record_full(record_content, app.record_path);
                    if (record_result.ok && !record_result.record.nak.version.empty()) {
                        a["nak_id"] = record_result.record.app.nak_id;
                        a["nak_version"] = record_result.record.nak.version;
                    }
                }
                apps_arr.push_back(a);
            }
            j["apps"] = apps_arr;
        }
        
        if (!naks_only || (!apps_only && !naks_only)) {
            nlohmann::json naks_arr = nlohmann::json::array();
            for (const auto& entry : nak_entries) {
                nlohmann::json n;
                n["id"] = entry.id;
                n["version"] = entry.version;
                n["record_ref"] = entry.record_ref;
                std::string nak_key = entry.id + "@" + entry.version;
                n["used_by_apps"] = nak_usage[nak_key];
                naks_arr.push_back(n);
            }
            j["naks"] = naks_arr;
        }
        
        std::cout << j.dump(2) << std::endl;
    } else {
        bool show_apps = !naks_only;
        bool show_naks = !apps_only;
        
        if (show_apps) {
            std::cout << "Apps:" << std::endl;
            if (apps.empty()) {
                std::cout << "  (none installed)" << std::endl;
            } else {
                for (const auto& app : apps) {
                    std::cout << "  " << app.id << "@" << app.version;
                    
                    // Get NAK info
                    std::string record_content = read_file(app.record_path);
                    if (!record_content.empty()) {
                        auto record_result = nah::parse_app_install_record_full(record_content, app.record_path);
                        if (record_result.ok && !record_result.record.nak.version.empty()) {
                            std::cout << " -> " << record_result.record.app.nak_id 
                                      << "@" << record_result.record.nak.version;
                        }
                    }
                    std::cout << std::endl;
                }
            }
        }
        
        if (show_apps && show_naks) {
            std::cout << std::endl;
        }
        
        if (show_naks) {
            std::cout << "NAKs:" << std::endl;
            if (nak_entries.empty()) {
                std::cout << "  (none installed)" << std::endl;
            } else {
                for (const auto& entry : nak_entries) {
                    std::cout << "  " << entry.id << "@" << entry.version;
                    std::string nak_key = entry.id + "@" + entry.version;
                    int usage = nak_usage[nak_key];
                    if (usage > 0) {
                        std::cout << " (used by " << usage << " app" << (usage > 1 ? "s" : "") << ")";
                    } else {
                        std::cout << " (unused)";
                    }
                    std::cout << std::endl;
                }
            }
        }
    }
    
    return 0;
}

// Unified pack command with auto-detection
int cmd_pack(const GlobalOptions& opts, const std::string& dir, const std::string& output,
             std::optional<PackageType> force_type) {
    // Detect package type from directory contents
    PackageType pkg_type = force_type.value_or(detect_package_type(dir));
    
    if (pkg_type == PackageType::Unknown) {
        ErrorContext ctx;
        ctx.hint = "The directory doesn't contain a recognized manifest.\n\n"
                   "       For apps: create manifest.json or embed manifest in binary\n"
                   "       For NAKs: create META/nak.json\n\n"
                   "       To force a type: nah pack " + dir + " --app -o output.nap\n"
                   "                         nah pack " + dir + " --nak -o output.nak";
        print_error("Cannot detect package type for: " + dir, opts.json, ctx);
        return 1;
    }
    
    std::string output_path = output;
    
    // Generate default output filename if not specified
    if (output_path.empty()) {
        fs::path dir_path(dir);
        std::string base_name = dir_path.filename().string();
        if (base_name.empty() || base_name == ".") {
            base_name = "package";
        }
        output_path = base_name + (pkg_type == PackageType::App ? ".nap" : ".nak");
    }
    
    if (pkg_type == PackageType::App) {
        auto result = nah::pack_nap(dir);
        if (!result.ok) {
            print_error(result.error, opts.json);
            return 1;
        }
        
        std::ofstream file(output_path, std::ios::binary);
        if (!file) {
            print_error("Failed to create output file: " + output_path, opts.json);
            return 1;
        }
        
        file.write(reinterpret_cast<const char*>(result.archive_data.data()),
                   static_cast<std::streamsize>(result.archive_data.size()));
        file.close();
        
        if (opts.json) {
            nlohmann::json j;
            j["success"] = true;
            j["type"] = "app";
            j["output"] = output_path;
            j["size"] = result.archive_data.size();
            std::cout << j.dump(2) << std::endl;
        } else if (!opts.quiet) {
            std::cout << "Created: " << output_path << " (" << result.archive_data.size() << " bytes)" << std::endl;
        }
        return 0;
    } else {
        auto result = nah::pack_nak(dir);
        if (!result.ok) {
            print_error(result.error, opts.json);
            return 1;
        }
        
        std::ofstream file(output_path, std::ios::binary);
        if (!file) {
            print_error("Failed to create output file: " + output_path, opts.json);
            return 1;
        }
        
        file.write(reinterpret_cast<const char*>(result.archive_data.data()),
                   static_cast<std::streamsize>(result.archive_data.size()));
        file.close();
        
        if (opts.json) {
            nlohmann::json j;
            j["success"] = true;
            j["type"] = "nak";
            j["output"] = output_path;
            j["size"] = result.archive_data.size();
            std::cout << j.dump(2) << std::endl;
        } else if (!opts.quiet) {
            std::cout << "Created: " << output_path << " (" << result.archive_data.size() << " bytes)" << std::endl;
        }
        return 0;
    }
}

// Unified init command
// Create a new profile by copying from an existing one
int cmd_init_profile(const GlobalOptions& opts, const std::string& name, 
                      const std::string& from_profile) {
    // Ensure root exists
    if (!check_nah_root(opts.root, opts.json)) {
        return 1;
    }
    
    fs::path profiles_dir = fs::path(opts.root) / "host" / "profiles";
    fs::path new_profile_path = profiles_dir / (name + ".json");
    
    // Check if profile already exists
    if (fs::exists(new_profile_path)) {
        ErrorContext ctx;
        ctx.hint = "Use a different name or delete the existing profile first.";
        print_error("Profile already exists: " + name, opts.json, ctx);
        return 1;
    }
    
    // Determine source profile
    std::string source_name = from_profile.empty() ? "default" : from_profile;
    fs::path source_path = profiles_dir / (source_name + ".json");
    
    if (!fs::exists(source_path)) {
        // Try to find the active profile if source not found
        if (from_profile.empty()) {
            fs::path active_path = fs::path(opts.root) / "host" / "profile.current";
            if (fs::exists(active_path)) {
                std::error_code ec;
                source_path = fs::read_symlink(active_path, ec);
                if (!ec) {
                    // Make it absolute relative to host/
                    if (source_path.is_relative()) {
                        source_path = fs::path(opts.root) / "host" / source_path;
                    }
                }
            }
        }
        
        if (!fs::exists(source_path)) {
            ErrorContext ctx;
            ctx.hint = "Available profiles can be listed with: nah profile list";
            print_error("Source profile not found: " + source_name, opts.json, ctx);
            return 1;
        }
    }
    
    // Copy the profile
    std::error_code ec;
    fs::copy_file(source_path, new_profile_path, ec);
    if (ec) {
        print_error("Failed to create profile: " + ec.message(), opts.json);
        return 1;
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["created"] = name;
        j["path"] = new_profile_path.string();
        j["copied_from"] = source_name;
        std::cout << j.dump(2) << std::endl;
    } else if (!opts.quiet) {
        std::cout << "Created profile: " << name << std::endl;
        std::cout << "  Path: " << new_profile_path.string() << std::endl;
        std::cout << "  Copied from: " << source_name << std::endl;
        std::cout << std::endl;
        std::cout << "To activate: nah profile set " << name << std::endl;
        std::cout << "To edit: $EDITOR " << new_profile_path.string() << std::endl;
    }
    
    return 0;
}

int cmd_init(const GlobalOptions& opts, const std::string& type, const std::string& dir,
             const std::string& from_profile) {
    if (type == "app") {
        return cmd_app_init(opts, dir);
    } else if (type == "nak") {
        return cmd_nak_init(opts, dir);
    } else if (type == "root") {
        return cmd_profile_init(opts, dir);
    } else if (type == "profile") {
        return cmd_init_profile(opts, dir, from_profile);
    } else {
        ErrorContext ctx;
        ctx.hint = "Valid types: app, nak, root, profile\n\n"
                   "       nah init app ./myapp      Create app project\n"
                   "       nah init nak ./mysdk      Create NAK project\n"
                   "       nah init root ./my-nah    Create NAH root directory\n"
                   "       nah init profile dev      Create new profile from default";
        print_error("Unknown init type: " + type, opts.json, ctx);
        return 1;
    }
}

// Unified status command - combines contract show, doctor, validate, app show, nak show
int cmd_status(const GlobalOptions& opts, const std::string& target, bool fix,
               const std::string& diff_profile, const std::string& /*overrides_file*/) {
    // If no target, show overview
    if (target.empty()) {
        if (!check_nah_root(opts.root, opts.json)) {
            return 1;
        }
        
        auto host = nah::NahHost::create(opts.root);
        auto apps = host->listApplications();
        auto nak_entries = nah::scan_nak_registry(opts.root);
        auto profiles = host->listProfiles();
        
        // Get active profile
        std::string active_profile = "default";
        auto profile_result = host->getActiveHostProfile();
        if (profile_result.isOk()) {
            // Try to determine which profile is active
            for (const auto& p : profiles) {
                auto test = host->loadProfile(p);
                if (test.isOk()) {
                    // Simple heuristic - would need proper implementation
                    active_profile = p;
                    break;
                }
            }
        }
        
        if (opts.json) {
            nlohmann::json j;
            j["root"] = opts.root;
            j["active_profile"] = active_profile;
            j["app_count"] = apps.size();
            j["nak_count"] = nak_entries.size();
            j["profile_count"] = profiles.size();
            std::cout << j.dump(2) << std::endl;
        } else {
            std::cout << "NAH Status" << std::endl;
            std::cout << "  Root: " << opts.root << std::endl;
            std::cout << "  Active Profile: " << active_profile << std::endl;
            std::cout << "  Apps: " << apps.size() << " installed" << std::endl;
            std::cout << "  NAKs: " << nak_entries.size() << " installed" << std::endl;
            std::cout << "  Profiles: " << profiles.size() << " available" << std::endl;
            
            if (!opts.quiet) {
                std::cout << std::endl;
                std::cout << "Run 'nah status <app-id>' to check a specific app." << std::endl;
                std::cout << "Run 'nah list' to see all installed packages." << std::endl;
            }
        }
        return 0;
    }
    
    // Check if target is a file (validate mode)
    if (fs::exists(target) && fs::is_regular_file(target)) {
        // Detect file type and validate
        std::string content = read_file(target);
        if (content.empty()) {
            print_error("Failed to read file: " + target, opts.json);
            return 1;
        }
        
        // Try to parse as JSON first
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(content);
        } catch (const nlohmann::json::parse_error& e) {
            // Not JSON - might be a binary with embedded manifest
            auto manifest_result = nah::read_manifest_section(target);
            if (manifest_result.ok) {
                // Show embedded manifest
                auto parse_result = nah::parse_manifest(manifest_result.data);
                if (!parse_result.ok) {
                    print_error("Manifest parse failed: " + parse_result.error, opts.json);
                    return 1;
                }
                
                const auto& m = parse_result.manifest;
                if (opts.json) {
                    nlohmann::json out;
                    out["type"] = "embedded_manifest";
                    out["file"] = target;
                    out["id"] = m.id;
                    out["version"] = m.version;
                    out["nak_id"] = m.nak_id;
                    out["entrypoint"] = m.entrypoint_path;
                    out["valid"] = true;
                    std::cout << out.dump(2) << std::endl;
                } else {
                    std::cout << "Embedded Manifest: " << target << std::endl;
                    std::cout << "  ID: " << m.id << std::endl;
                    std::cout << "  Version: " << m.version << std::endl;
                    std::cout << "  NAK ID: " << m.nak_id << std::endl;
                    std::cout << "  Entrypoint: " << m.entrypoint_path << std::endl;
                    std::cout << std::endl << color::green("Valid") << std::endl;
                }
                return 0;
            }
            
            print_error("Cannot parse file: " + std::string(e.what()), opts.json);
            return 1;
        }
        
        // Detect JSON file type
        bool valid = true;
        std::string error;
        std::vector<std::string> warnings;
        std::string file_type;
        
        if (j.contains("nak") && j["nak"].contains("binding_mode")) {
            // Host profile
            file_type = "profile";
            auto result = nah::parse_host_profile_full(content, target);
            valid = result.ok;
            error = result.error;
            warnings = result.warnings;
        } else if (j.contains("app") && j.contains("nak") && j["nak"].contains("record_ref")) {
            // App install record
            file_type = "install_record";
            auto result = nah::parse_app_install_record_full(content, target);
            valid = result.ok;
            error = result.error;
            warnings = result.warnings;
        } else if (j.contains("nak") && j.contains("paths")) {
            // NAK manifest or record
            file_type = "nak_record";
            auto result = nah::parse_nak_install_record_full(content, target);
            valid = result.ok;
            error = result.error;
            warnings = result.warnings;
        } else if (j.contains("app") && j["app"].contains("id")) {
            // App manifest input
            file_type = "manifest_input";
            valid = true; // Basic structure check
        } else {
            file_type = "unknown";
            valid = true; // Can't validate unknown format
        }
        
        // Handle --fix for formatting
        if (fix && valid) {
            std::string formatted = j.dump(2) + "\n";
            if (formatted != content) {
                std::ofstream out(target);
                if (out) {
                    out << formatted;
                    out.close();
                    if (!opts.quiet && !opts.json) {
                        std::cout << "Formatted: " << target << std::endl;
                    }
                }
            }
        }
        
        if (opts.json) {
            nlohmann::json out;
            out["type"] = file_type;
            out["file"] = target;
            out["valid"] = valid;
            if (!valid) out["error"] = error;
            if (!warnings.empty()) out["warnings"] = warnings;
            std::cout << out.dump(2) << std::endl;
        } else {
            std::cout << target << " (" << file_type << "): ";
            if (valid) {
                std::cout << color::green("valid") << std::endl;
            } else {
                std::cout << color::red("invalid") << " - " << error << std::endl;
            }
            for (const auto& w : warnings) {
                std::cout << "  " << color::yellow("warning: ") << w << std::endl;
            }
        }
        
        return valid ? 0 : 1;
    }
    
    // Check if target is a directory (packability check)
    if (fs::exists(target) && fs::is_directory(target)) {
        // Detect package type
        PackageType pkg_type = detect_package_type(target);
        
        if (pkg_type == PackageType::Unknown) {
            if (opts.json) {
                nlohmann::json out;
                out["type"] = "directory";
                out["path"] = target;
                out["packable"] = false;
                out["error"] = "Cannot determine package type. Need manifest.nah (app) or META/nak.json (NAK).";
                std::cout << out.dump(2) << std::endl;
            } else {
                std::cout << target << " (directory): ";
                std::cout << color::red("not packable") << std::endl;
                std::cout << "  Cannot determine package type." << std::endl;
                std::cout << "  For an app: add manifest.nah or embed manifest in bin/" << std::endl;
                std::cout << "  For a NAK: add META/nak.json" << std::endl;
            }
            return 1;
        }
        
        // Try to pack (dry-run style validation)
        nah::PackResult pack_result;
        std::string pkg_type_str;
        
        if (pkg_type == PackageType::App) {
            pkg_type_str = "app";
            pack_result = nah::pack_nap(target);
        } else {
            pkg_type_str = "nak";
            pack_result = nah::pack_nak(target);
        }
        
        if (opts.json) {
            nlohmann::json out;
            out["type"] = "directory";
            out["path"] = target;
            out["package_type"] = pkg_type_str;
            out["packable"] = pack_result.ok;
            if (!pack_result.ok) {
                out["error"] = pack_result.error;
            } else {
                out["archive_size"] = pack_result.archive_data.size();
            }
            std::cout << out.dump(2) << std::endl;
        } else {
            std::cout << target << " (" << pkg_type_str << "): ";
            if (pack_result.ok) {
                std::cout << color::green("packable") << std::endl;
                if (!opts.quiet) {
                    std::cout << "  Archive size: " << pack_result.archive_data.size() << " bytes" << std::endl;
                    std::cout << std::endl;
                    std::cout << "Run 'nah pack " << target << "' to create the package." << std::endl;
                }
            } else {
                std::cout << color::red("not packable") << std::endl;
                // Format multi-line errors nicely
                std::string error = pack_result.error;
                size_t pos = 0;
                bool first = true;
                while ((pos = error.find('\n')) != std::string::npos) {
                    std::string line = error.substr(0, pos);
                    if (first) {
                        std::cout << "  " << line << std::endl;
                        first = false;
                    } else {
                        std::cout << "  " << line << std::endl;
                    }
                    error.erase(0, pos + 1);
                }
                if (!error.empty()) {
                    std::cout << "  " << error << std::endl;
                }
            }
        }
        
        return pack_result.ok ? 0 : 1;
    }
    
    // Target is an app or NAK ID - show contract/details
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "package")) {
        return 1;
    }
    
    // Detect if it's an app or NAK
    PackageType pkg_type = detect_installed_type(opts.root, id, version);
    
    if (pkg_type == PackageType::Unknown) {
        ErrorContext ctx;
        ctx.hint = "Run 'nah list' to see installed packages.\n"
                   "       Run 'nah status --trace' to diagnose issues.";
        print_error("Not found: " + target, opts.json, ctx);
        suggest_available_targets(opts.root, id, "package", opts.json);
        return 1;
    }
    
    if (pkg_type == PackageType::Nak) {
        // Show NAK details
        auto entries = nah::scan_nak_registry(opts.root);
        for (const auto& entry : entries) {
            if (entry.id == id && (version.empty() || entry.version == version)) {
                std::string content = read_file(entry.record_path);
                auto result = nah::parse_nak_install_record_full(content, entry.record_path);
                
                if (opts.json) {
                    nlohmann::json j;
                    j["type"] = "nak";
                    j["id"] = result.record.nak.id;
                    j["version"] = result.record.nak.version;
                    j["root"] = result.record.paths.root;
                    j["resource_root"] = result.record.paths.resource_root;
                    j["lib_dirs"] = result.record.paths.lib_dirs;
                    j["has_loaders"] = result.record.has_loaders();
                    std::cout << j.dump(2) << std::endl;
                } else {
                    std::cout << "NAK: " << result.record.nak.id << " v" << result.record.nak.version << std::endl;
                    std::cout << "  Root: " << result.record.paths.root << std::endl;
                    std::cout << "  Resource Root: " << result.record.paths.resource_root << std::endl;
                    std::cout << "  Lib Dirs:" << std::endl;
                    for (const auto& lib : result.record.paths.lib_dirs) {
                        std::cout << "    " << lib << std::endl;
                    }
                    if (result.record.has_loaders()) {
                        std::cout << "  Loaders:" << std::endl;
                        for (const auto& [name, loader] : result.record.loaders) {
                            std::cout << "    " << name << ": " << loader.exec_path << std::endl;
                        }
                    }
                }
                return 0;
            }
        }
        print_error("NAK not found: " + target, opts.json);
        suggest_available_targets(opts.root, target, "NAK", opts.json);
        return 1;
    }
    
    // Show app contract (the main use case)
    auto host = nah::NahHost::create(opts.root);
    
    // Handle --diff mode
    if (!diff_profile.empty()) {
        auto result_a = host->getLaunchContract(id, version, opts.profile, opts.trace);
        auto result_b = host->getLaunchContract(id, version, diff_profile, opts.trace);
        
        if (result_a.isErr()) {
            print_error("Profile A: " + result_a.error().message(), opts.json);
            return 1;
        }
        if (result_b.isErr()) {
            print_error("Profile B (" + diff_profile + "): " + result_b.error().message(), opts.json);
            return 1;
        }
        
        const auto& c_a = result_a.value().contract;
        const auto& c_b = result_b.value().contract;
        
        std::vector<std::tuple<std::string, std::string, std::string>> diffs;
        
        if (c_a.execution.binary != c_b.execution.binary) {
            diffs.emplace_back("execution.binary", c_a.execution.binary, c_b.execution.binary);
        }
        if (c_a.nak.version != c_b.nak.version) {
            diffs.emplace_back("nak.version", c_a.nak.version, c_b.nak.version);
        }
        
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
            j["target"] = target;
            j["profile_a"] = opts.profile.empty() ? "active" : opts.profile;
            j["profile_b"] = diff_profile;
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
            std::cout << "  Current profile vs " << diff_profile << std::endl;
            std::cout << std::endl;
            
            if (diffs.empty()) {
                std::cout << "No differences found." << std::endl;
            } else {
                for (const auto& [path, val_a, val_b] : diffs) {
                    std::cout << "  " << path << ":" << std::endl;
                    std::cout << "    current: " << val_a << std::endl;
                    std::cout << "    " << diff_profile << ": " << val_b << std::endl;
                }
            }
        }
        
        return diffs.empty() ? 0 : 2;
    }
    
    // Normal contract show
    auto result = host->getLaunchContract(id, version, opts.profile, opts.trace);
    
    if (result.isErr()) {
        if (opts.json) {
            nlohmann::json j;
            j["schema"] = "nah.launch.contract.v1";
            j["critical_error"] = result.error().message();
            j["warnings"] = nlohmann::json::array();
            std::cout << j.dump(2) << std::endl;
        } else {
            print_error(result.error().message(), opts.json);
            
            // Add hint about --trace
            if (!opts.trace) {
                std::cerr << std::endl << color::blue("hint: ") 
                          << "Run with --trace for detailed diagnostics" << std::endl;
            }
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
        } else {
            std::cout << "NAK: (none - standalone app)" << std::endl;
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
        
        std::cout << std::endl << "Environment (NAH_*):" << std::endl;
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
        
        // Show trace hint if not already using trace
        if (!opts.trace && !opts.quiet) {
            std::cout << std::endl << color::dim("Run with --trace to see where each value comes from.") << std::endl;
        }
    }
    
    // Handle --fix
    if (fix) {
        // Run doctor-style fixes
        return cmd_doctor(opts, target, true);
    }
    
    // Exit codes per SPEC
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
    
    // Create default.json
    std::string default_profile = R"({
  "nak": {
    "binding_mode": "canonical",
    "allow_versions": [],
    "deny_versions": []
  },
  "environment": {},
  "warnings": {},
  "capabilities": {},
  "overrides": {
    "mode": "deny"
  }
})";
    
    std::ofstream profile_file(root_path / "host" / "profiles" / "default.json");
    if (!profile_file) {
        print_error("failed to write default.json", opts.json);
        return 1;
    }
    profile_file << default_profile;
    profile_file.close();
    
    // Create profile.current symlink
    fs::path symlink_path = root_path / "host" / "profile.current";
    fs::create_symlink("profiles/default.json", symlink_path, ec);
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
│   │   └── default.json    # Host profile configuration
│   └── profile.current     # Symlink to active profile
├── apps/                   # Installed applications
├── naks/                   # Installed NAK packs
└── registry/
    ├── installs/           # App install records
    └── naks/               # NAK install records
```

## Next Steps

1. Edit `host/profiles/default.json` for your environment
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
        std::cout << "  " << dir << "/host/profiles/default.json" << std::endl;
        std::cout << "  " << dir << "/host/profile.current -> profiles/default.json" << std::endl;
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
        j["binding_mode"] = nah::binding_mode_to_string(profile.nak.binding_mode);
        j["allow_versions"] = profile.nak.allow_versions;
        j["deny_versions"] = profile.nak.deny_versions;
        
        nlohmann::ordered_json env;
        std::vector<std::string> env_keys;
        for (const auto& [k, _] : profile.environment) env_keys.push_back(k);
        std::sort(env_keys.begin(), env_keys.end());
        for (const auto& k : env_keys) {
            const auto& ev = profile.environment.at(k);
            if (ev.op == nah::EnvOp::Set) {
                env[k] = ev.value;
            } else {
                nlohmann::json op_obj;
                op_obj["op"] = nah::env_op_to_string(ev.op);
                if (ev.op != nah::EnvOp::Unset) {
                    op_obj["value"] = ev.value;
                    if (ev.separator != ":") op_obj["separator"] = ev.separator;
                }
                env[k] = op_obj;
            }
        }
        j["environment"] = env;
        
        std::cout << j.dump(2) << std::endl;
    } else {
        std::cout << "Binding Mode: " << nah::binding_mode_to_string(profile.nak.binding_mode) << std::endl;
        if (!profile.nak.allow_versions.empty()) {
            std::cout << "Allow Versions: ";
            for (const auto& v : profile.nak.allow_versions) std::cout << v << " ";
            std::cout << std::endl;
        }
        if (!profile.environment.empty()) {
            std::cout << "Environment:" << std::endl;
            for (const auto& [k, ev] : profile.environment) {
                if (ev.op == nah::EnvOp::Set) {
                    std::cout << "  " << k << "=" << ev.value << std::endl;
                } else {
                    std::cout << "  " << k << " (" << nah::env_op_to_string(ev.op) << "): " << ev.value << std::endl;
                }
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
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "application")) {
        return 1;
    }
    
    auto host = nah::NahHost::create(opts.root);
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
        } else {
            std::cout << "NAK: (none - standalone app)" << std::endl;
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
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "application")) {
        return 1;
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
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "application")) {
        return 1;
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
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "application")) {
        return 1;
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
    // Read JSON file
    std::string json_content = read_file(input_path);
    if (json_content.empty()) {
        print_error("failed to read input file: " + input_path, opts.json);
        return 1;
    }
    
    // Use the manifest generation library (expects nah.manifest.input.v2 schema)
    auto result = nah::generate_manifest(json_content);
    
    if (!result.ok) {
        ErrorContext ctx;
        ctx.file_path = input_path;
        ctx.hint = "The input file must have an \"app\" section with required fields.\n"
                   "       See 'nah manifest generate --help' for the expected format.";
        print_error(result.error, opts.json, ctx);
        return 1;
    }
    
    // Print any warnings
    for (const auto& warning : result.warnings) {
        print_warning(warning, opts.json);
    }
    
    // Write output
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        print_error("failed to create output file: " + output_path, opts.json);
        return 1;
    }
    
    out.write(reinterpret_cast<const char*>(result.manifest_bytes.data()),
              static_cast<std::streamsize>(result.manifest_bytes.size()));
    out.close();
    
    if (opts.json) {
        nlohmann::json j;
        j["success"] = true;
        j["input"] = input_path;
        j["output"] = output_path;
        j["size"] = result.manifest_bytes.size();
        if (!result.warnings.empty()) {
            j["warnings"] = result.warnings;
        }
        std::cout << j.dump(2) << std::endl;
    } else if (!opts.quiet) {
        std::cout << "Generated: " << output_path << " (" << result.manifest_bytes.size() << " bytes)" << std::endl;
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
        std::string id, version;
        if (!parse_target(target, id, version, opts.json, "application")) {
            return 1;
        }
        
        auto host = nah::NahHost::create(opts.root);
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
            j["nak_version_req"] = m.nak_version_req->selection_key();
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
            std::cout << "NAK Version Req: " << m.nak_version_req->selection_key() << std::endl;
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
        std::string id, version;
        if (!parse_target(target, id, version, opts.json, "application")) {
            return 1;
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
                    fs::path default_profile = fs::path(opts.root) / "host" / "profiles" / "default.json";
                    if (fs::exists(default_profile)) {
                        fs::create_symlink("profiles/default.json", profile_current, ec);
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
    
    // Parse as JSON
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(content);
    } catch (const nlohmann::json::parse_error& e) {
        print_error("JSON parse error: " + std::string(e.what()), opts.json);
        return 1;
    }
    
    // Format to canonical JSON (2-space indent)
    std::string formatted_str = j.dump(2) + "\n";
    
    // Check if different
    bool differs = (content != formatted_str);
    
    if (check) {
        // Just check, don't modify
        if (opts.json) {
            nlohmann::json jout;
            jout["path"] = path;
            jout["formatted"] = !differs;
            std::cout << jout.dump(2) << std::endl;
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
        nlohmann::json jout;
        jout["path"] = path;
        jout["formatted"] = true;
        jout["changed"] = differs;
        std::cout << jout.dump(2) << std::endl;
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
// Run Command
// ============================================================================

int cmd_run(const GlobalOptions& opts, const std::string& target,
            const std::vector<std::string>& app_args) {
    std::string id, version;
    if (!parse_target(target, id, version, opts.json, "application")) {
        return 1;
    }
    
    auto host = nah::NahHost::create(opts.root);
    auto result = host->getLaunchContract(id, version, opts.profile, false);
    
    if (result.isErr()) {
        print_error(result.error().message(), opts.json);
        suggest_available_targets(opts.root, id, "application", opts.json);
        return 1;
    }
    
    const auto& envelope = result.value();
    const auto& c = envelope.contract;
    
    // Check for critical warnings that would prevent launch
    for (const auto& w : envelope.warnings) {
        if (w.action == "error") {
            print_error("Cannot launch: " + w.key, opts.json);
            return 1;
        }
    }
    
    // Verbose output before launch
    if (opts.verbose && !opts.json) {
        std::cerr << "Launching " << c.app.id << " v" << c.app.version << std::endl;
        std::cerr << "  Binary: " << c.execution.binary << std::endl;
        if (!c.nak.id.empty()) {
            std::cerr << "  NAK: " << c.nak.id << "@" << c.nak.version << std::endl;
        } else {
            std::cerr << "  NAK: (none - standalone app)" << std::endl;
        }
        std::cerr << "  CWD: " << c.execution.cwd << std::endl;
        std::cerr << std::endl;
    }
    
    // Build environment
    std::vector<std::string> env_strings;
    for (const auto& [key, value] : c.environment) {
        env_strings.push_back(key + "=" + value);
    }
    
    // Add library path
    if (!c.execution.library_paths.empty()) {
        std::string lib_path;
        for (size_t i = 0; i < c.execution.library_paths.size(); ++i) {
            if (i > 0) lib_path += ":";
            lib_path += c.execution.library_paths[i];
        }
        env_strings.push_back(c.execution.library_path_env_key + "=" + lib_path);
    }
    
    // Inherit some essential environment variables
    const char* term = std::getenv("TERM");
    if (term) env_strings.push_back(std::string("TERM=") + term);
    const char* home = std::getenv("HOME");
    if (home) env_strings.push_back(std::string("HOME=") + home);
    const char* path = std::getenv("PATH");
    if (path) env_strings.push_back(std::string("PATH=") + path);
    const char* user = std::getenv("USER");
    if (user) env_strings.push_back(std::string("USER=") + user);
    const char* shell = std::getenv("SHELL");
    if (shell) env_strings.push_back(std::string("SHELL=") + shell);
    
    // Build argv
    std::vector<std::string> argv_strings;
    argv_strings.push_back(c.execution.binary);
    for (const auto& arg : c.execution.arguments) {
        argv_strings.push_back(arg);
    }
    for (const auto& arg : app_args) {
        argv_strings.push_back(arg);
    }
    
    // Change to the working directory
    if (!c.execution.cwd.empty()) {
        if (chdir(c.execution.cwd.c_str()) != 0) {
            print_error("Failed to change to directory: " + c.execution.cwd, opts.json);
            return 1;
        }
    }
    
#ifdef _WIN32
    // Windows: Set environment variables and use _spawnv
    for (const auto& env_str : env_strings) {
        _putenv(env_str.c_str());
    }
    
    // Convert to C-style array for _spawnv
    std::vector<const char*> argv_ptrs;
    for (const auto& s : argv_strings) {
        argv_ptrs.push_back(s.c_str());
    }
    argv_ptrs.push_back(nullptr);
    
    // Use _spawnv with _P_WAIT to run and wait for completion
    intptr_t result = _spawnv(_P_WAIT, c.execution.binary.c_str(), argv_ptrs.data());
    if (result == -1) {
        print_error("Failed to execute: " + c.execution.binary + " - " + std::strerror(errno), opts.json);
        return 1;
    }
    return static_cast<int>(result);
#else
    // POSIX: Use execve to replace the current process
    std::vector<char*> argv_ptrs;
    for (auto& s : argv_strings) {
        argv_ptrs.push_back(&s[0]);
    }
    argv_ptrs.push_back(nullptr);
    
    std::vector<char*> env_ptrs;
    for (auto& s : env_strings) {
        env_ptrs.push_back(&s[0]);
    }
    env_ptrs.push_back(nullptr);
    
    execve(c.execution.binary.c_str(), argv_ptrs.data(), env_ptrs.data());
    
    // If we get here, execve failed
    print_error("Failed to execute: " + c.execution.binary + " - " + std::strerror(errno), opts.json);
    return 1;
#endif
}

// ============================================================================
// Inspect Command
// ============================================================================

int cmd_inspect(const GlobalOptions& opts, const std::string& package_path, bool show_files) {
    // Check file exists
    if (!fs::exists(package_path)) {
        print_error("File not found: " + package_path, opts.json);
        return 1;
    }
    
    // Detect package type from extension
    PackageType pkg_type = detect_package_type(package_path);
    
    if (pkg_type == PackageType::Unknown) {
        ErrorContext ctx;
        ctx.hint = "Expected .nap (app) or .nak (NAK) file extension";
        print_error("Cannot determine package type: " + package_path, opts.json, ctx);
        return 1;
    }
    
    // Get file size
    auto file_size = fs::file_size(package_path);
    
    if (pkg_type == PackageType::App) {
        // Use existing inspect function
        auto info = nah::inspect_nap_package(package_path);
        
        if (!info.ok) {
            print_error("Failed to inspect package: " + info.error, opts.json);
            return 1;
        }
        
        if (opts.json) {
            nlohmann::json j;
            j["file"] = package_path;
            j["size"] = file_size;
            j["type"] = "app";
            j["id"] = info.app_id;
            j["version"] = info.app_version;
            j["entrypoint"] = info.entrypoint;
            j["nak_id"] = info.nak_id;
            j["nak_version_req"] = info.nak_version_req;
            j["manifest_source"] = info.manifest_source;
            
            if (show_files) {
                nlohmann::json files_arr = nlohmann::json::array();
                for (const auto& b : info.binaries) {
                    nlohmann::json f;
                    f["name"] = b;
                    f["type"] = "binary";
                    files_arr.push_back(f);
                }
                for (const auto& l : info.libraries) {
                    nlohmann::json f;
                    f["name"] = l;
                    f["type"] = "library";
                    files_arr.push_back(f);
                }
                for (const auto& a : info.assets) {
                    nlohmann::json f;
                    f["name"] = a;
                    f["type"] = "asset";
                    files_arr.push_back(f);
                }
                j["files"] = files_arr;
            }
            
            std::cout << j.dump(2) << std::endl;
        } else {
            std::cout << "Package: " << package_path << " (" << file_size << " bytes)" << std::endl;
            std::cout << "Type: App (NAP)" << std::endl;
            std::cout << std::endl;
            
            std::cout << "Manifest:" << std::endl;
            std::cout << "  ID: " << info.app_id << std::endl;
            std::cout << "  Version: " << info.app_version << std::endl;
            std::cout << "  Entrypoint: " << info.entrypoint << std::endl;
            if (info.nak_id.empty()) {
                std::cout << "  NAK: (none - standalone app)" << std::endl;
            } else {
                std::cout << "  NAK: " << info.nak_id;
                if (!info.nak_version_req.empty()) {
                    std::cout << " (" << info.nak_version_req << ")";
                }
                std::cout << std::endl;
            }
            std::cout << "  Source: " << info.manifest_source << std::endl;
            
            std::cout << std::endl;
            
            if (show_files) {
                if (!info.binaries.empty()) {
                    std::cout << "Binaries:" << std::endl;
                    for (const auto& b : info.binaries) {
                        std::cout << "  " << b << std::endl;
                    }
                }
                if (!info.libraries.empty()) {
                    std::cout << "Libraries:" << std::endl;
                    for (const auto& l : info.libraries) {
                        std::cout << "  " << l << std::endl;
                    }
                }
                if (!info.assets.empty()) {
                    std::cout << "Assets:" << std::endl;
                    for (const auto& a : info.assets) {
                        std::cout << "  " << a << std::endl;
                    }
                }
            } else {
                std::cout << "Contents: " << info.binaries.size() << " binaries, " 
                          << info.libraries.size() << " libraries, "
                          << info.assets.size() << " assets" << std::endl;
                std::cout << std::endl;
                std::cout << "Run with --files to see full file listing." << std::endl;
            }
        }
    } else {
        // NAK package
        auto info = nah::inspect_nak_pack(package_path);
        
        if (!info.ok) {
            print_error("Failed to inspect package: " + info.error, opts.json);
            return 1;
        }
        
        if (opts.json) {
            nlohmann::json j;
            j["file"] = package_path;
            j["size"] = file_size;
            j["type"] = "nak";
            j["id"] = info.nak_id;
            j["version"] = info.nak_version;
            j["resource_root"] = info.resource_root;
            j["lib_dirs"] = info.lib_dirs;
            j["has_loaders"] = info.has_loaders();
            
            if (show_files) {
                nlohmann::json files_arr = nlohmann::json::array();
                for (const auto& r : info.resources) {
                    nlohmann::json f;
                    f["name"] = r;
                    f["type"] = "resource";
                    files_arr.push_back(f);
                }
                for (const auto& l : info.libraries) {
                    nlohmann::json f;
                    f["name"] = l;
                    f["type"] = "library";
                    files_arr.push_back(f);
                }
                for (const auto& b : info.binaries) {
                    nlohmann::json f;
                    f["name"] = b;
                    f["type"] = "binary";
                    files_arr.push_back(f);
                }
                j["files"] = files_arr;
            }
            
            std::cout << j.dump(2) << std::endl;
        } else {
            std::cout << "Package: " << package_path << " (" << file_size << " bytes)" << std::endl;
            std::cout << "Type: NAK" << std::endl;
            std::cout << std::endl;
            
            std::cout << "Metadata:" << std::endl;
            std::cout << "  ID: " << info.nak_id << std::endl;
            std::cout << "  Version: " << info.nak_version << std::endl;
            std::cout << "  Resource Root: " << info.resource_root << std::endl;
            std::cout << "  Lib Dirs: ";
            for (size_t i = 0; i < info.lib_dirs.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << info.lib_dirs[i];
            }
            std::cout << std::endl;
            
            if (info.has_loaders()) {
                std::cout << "  Loaders:" << std::endl;
                for (const auto& [name, loader] : info.loaders) {
                    std::cout << "    " << name << ": " << loader.exec_path << std::endl;
                }
            }
            
            std::cout << std::endl;
            
            if (show_files) {
                if (!info.binaries.empty()) {
                    std::cout << "Binaries:" << std::endl;
                    for (const auto& b : info.binaries) {
                        std::cout << "  " << b << std::endl;
                    }
                }
                if (!info.libraries.empty()) {
                    std::cout << "Libraries:" << std::endl;
                    for (const auto& l : info.libraries) {
                        std::cout << "  " << l << std::endl;
                    }
                }
                if (!info.resources.empty()) {
                    std::cout << "Resources:" << std::endl;
                    for (const auto& r : info.resources) {
                        std::cout << "  " << r << std::endl;
                    }
                }
            } else {
                std::cout << "Contents: " << info.binaries.size() << " binaries, " 
                          << info.libraries.size() << " libraries, "
                          << info.resources.size() << " resources" << std::endl;
                std::cout << std::endl;
                std::cout << "Run with --files to see full file listing." << std::endl;
            }
        }
    }
    
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    // Initialize color output
    color::init();
    
    CLI::App app{"nah - Native Application Host CLI v" NAH_VERSION "\n\n"
                 "Manage native applications and NAKs with auto-detection."};
    app.require_subcommand(0, 1);
    app.set_version_flag("-V,--version", NAH_VERSION);
    app.footer("\nRun 'nah <command> --help' for more information on a command.\n"
               "Documentation: https://github.com/rtorr/nah");
    
    GlobalOptions opts;
    
    // Global flags (on parent app)
    app.add_option("--root", opts.root, 
        "NAH root directory (auto-detected from cwd, NAH_ROOT, or ~/.nah)")->envname("NAH_ROOT");
    app.add_option("--profile", opts.profile, 
        "Use a specific profile instead of the active one");
    app.add_flag("--json", opts.json, 
        "Output in JSON format for machine parsing");
    app.add_flag("--trace", opts.trace, 
        "Include trace information showing where each value came from");
    app.add_flag("-v,--verbose", opts.verbose, 
        "Show detailed progress information");
    app.add_flag("-q,--quiet", opts.quiet, 
        "Suppress non-essential output");
    
    // Helper to add common flags to subcommands (allows flags after subcommand name)
    auto add_common_flags = [&opts](CLI::App* cmd) {
        cmd->add_flag("--json", opts.json, "Output in JSON format");
        cmd->add_flag("--trace", opts.trace, "Include trace/provenance information");
        cmd->add_flag("-v,--verbose", opts.verbose, "Show detailed progress");
        cmd->add_flag("-q,--quiet", opts.quiet, "Suppress non-essential output");
    };
    
    // Apply root auto-detection after parsing
    app.parse_complete_callback([&]() {
        opts.root = auto_detect_nah_root(opts.root);
    });
    
    // Helper for subcommand failure messages with suggestions
    auto make_failure_handler = []() {
        return [](const CLI::App* failed_app, const CLI::Error& e) {
            std::string error_msg = e.what();
            std::string result;
            
            // Get available subcommands for THIS app
            std::vector<std::string> valid_cmds;
            for (const auto* sub : failed_app->get_subcommands({})) {
                valid_cmds.push_back(sub->get_name());
            }
            
            // Check for unknown argument/subcommand errors
            if (error_msg.find("was not expected") != std::string::npos ||
                error_msg.find("could not be matched") != std::string::npos) {
                
                // Extract the unknown command
                std::string unknown_cmd;
                auto colon_pos = error_msg.rfind(':');
                if (colon_pos != std::string::npos && colon_pos + 2 < error_msg.size()) {
                    unknown_cmd = error_msg.substr(colon_pos + 2);
                    while (!unknown_cmd.empty() && std::isspace(unknown_cmd.back())) {
                        unknown_cmd.pop_back();
                    }
                }
                
                result = error_msg + "\n";
                
                // Find similar commands
                if (!unknown_cmd.empty() && !valid_cmds.empty()) {
                    auto suggestions = find_similar_commands(unknown_cmd, valid_cmds, 3);
                    if (!suggestions.empty()) {
                        result += "\nDid you mean?\n";
                        for (const auto& s : suggestions) {
                            result += "  " + s + "\n";
                        }
                    }
                }
                
                // Show available commands if no close match
                if (!valid_cmds.empty() && result.find("Did you mean") == std::string::npos) {
                    result += "\nAvailable commands:\n";
                    for (const auto& cmd : valid_cmds) {
                        result += "  " + cmd + "\n";
                    }
                }
            } else if (error_msg.find("subcommand is required") != std::string::npos) {
                // Missing subcommand - show available options
                result = error_msg + "\n";
                if (!valid_cmds.empty()) {
                    result += "\nAvailable commands:\n";
                    for (const auto& cmd : valid_cmds) {
                        result += "  " + cmd + "\n";
                    }
                }
            } else {
                result = CLI::FailureMessage::simple(failed_app, e);
            }
            
            result += "\nRun '" + failed_app->get_name() + " --help' for usage information.\n";
            return result;
        };
    };
    
    // ========== install - Unified install command ==========
    std::string install_source;
    bool install_force = false;
    bool install_as_app = false;
    bool install_as_nak = false;
    auto install_cmd = app.add_subcommand("install", "Install an app or NAK (auto-detected)");
    install_cmd->add_option("source", install_source, 
        "Source to install from:\n"
        "  - .nap file: installs as app\n"
        "  - .nak file: installs as NAK\n"
        "  - directory: packs and installs (type auto-detected)\n"
        "  - URL: fetches and installs")->required();
    install_cmd->add_flag("-f,--force", install_force, 
        "Overwrite existing installation");
    install_cmd->add_flag("--app", install_as_app, 
        "Force install as app (skip auto-detection)");
    install_cmd->add_flag("--nak", install_as_nak, 
        "Force install as NAK (skip auto-detection)");
    install_cmd->footer("\nExamples:\n"
                        "  nah install myapp.nap              # Install app\n"
                        "  nah install mysdk.nak              # Install NAK\n"
                        "  nah install ./myapp/               # Pack and install\n"
                        "  nah install https://example.com/app.nap");
    add_common_flags(install_cmd);
    install_cmd->callback([&]() {
        std::optional<PackageType> force_type;
        if (install_as_app) force_type = PackageType::App;
        if (install_as_nak) force_type = PackageType::Nak;
        std::exit(cmd_install(opts, install_source, install_force, force_type));
    });
    
    // ========== uninstall - Unified uninstall command ==========
    std::string uninstall_target;
    bool uninstall_as_app = false;
    bool uninstall_as_nak = false;
    auto uninstall_cmd = app.add_subcommand("uninstall", "Remove an installed app or NAK");
    uninstall_cmd->add_option("target", uninstall_target, 
        "Package to uninstall (id or id@version)")->required();
    uninstall_cmd->add_flag("--app", uninstall_as_app, 
        "Force uninstall as app");
    uninstall_cmd->add_flag("--nak", uninstall_as_nak, 
        "Force uninstall as NAK");
    uninstall_cmd->footer("\nExamples:\n"
                          "  nah uninstall com.example.app\n"
                          "  nah uninstall com.example.sdk@1.0.0");
    add_common_flags(uninstall_cmd);
    uninstall_cmd->callback([&]() {
        std::optional<PackageType> force_type;
        if (uninstall_as_app) force_type = PackageType::App;
        if (uninstall_as_nak) force_type = PackageType::Nak;
        std::exit(cmd_uninstall(opts, uninstall_target, force_type));
    });
    
    // ========== list - Unified list command ==========
    bool list_apps_only = false;
    bool list_naks_only = false;
    auto list_cmd = app.add_subcommand("list", "List installed apps and NAKs");
    list_cmd->add_flag("--apps", list_apps_only, 
        "Show only apps");
    list_cmd->add_flag("--naks", list_naks_only, 
        "Show only NAKs");
    list_cmd->footer("\nExamples:\n"
                     "  nah list               # Show all\n"
                     "  nah list --apps        # Apps only\n"
                     "  nah list --naks        # NAKs only");
    add_common_flags(list_cmd);
    list_cmd->callback([&]() {
        std::exit(cmd_list(opts, list_apps_only, list_naks_only));
    });
    
    // ========== run - Launch an installed app ==========
    std::string run_target;
    std::vector<std::string> run_args;
    auto run_cmd = app.add_subcommand("run", "Launch an installed application");
    run_cmd->add_option("target", run_target,
        "App to run (id or id@version)")->required();
    run_cmd->add_option("args", run_args,
        "Arguments to pass to the app")->allow_extra_args();
    run_cmd->footer("\nExamples:\n"
                    "  nah run com.example.myapp\n"
                    "  nah run com.example.myapp@1.0.0\n"
                    "  nah run com.example.myapp -- --arg1 --arg2");
    add_common_flags(run_cmd);
    run_cmd->callback([&]() {
        std::exit(cmd_run(opts, run_target, run_args));
    });
    
    // ========== inspect - Inspect a package ==========
    std::string inspect_path;
    bool inspect_files = false;
    auto inspect_cmd = app.add_subcommand("inspect", "Inspect a .nap or .nak package");
    inspect_cmd->add_option("file", inspect_path,
        "Package file to inspect")->required()->check(CLI::ExistingFile);
    inspect_cmd->add_flag("--files", inspect_files,
        "Show full file listing");
    inspect_cmd->footer("\nExamples:\n"
                        "  nah inspect myapp.nap\n"
                        "  nah inspect myapp.nap --files\n"
                        "  nah inspect mysdk.nak");
    add_common_flags(inspect_cmd);
    inspect_cmd->callback([&]() {
        std::exit(cmd_inspect(opts, inspect_path, inspect_files));
    });
    
    // ========== pack - Unified pack command ==========
    std::string pack_dir, pack_output;
    bool pack_as_app = false;
    bool pack_as_nak = false;
    auto pack_cmd = app.add_subcommand("pack", "Create a .nap or .nak package");
    pack_cmd->add_option("dir", pack_dir, 
        "Directory to pack")->required()->check(CLI::ExistingDirectory);
    pack_cmd->add_option("-o,--output", pack_output, 
        "Output file path (optional, auto-generated if omitted)");
    pack_cmd->add_flag("--app", pack_as_app, 
        "Force pack as app");
    pack_cmd->add_flag("--nak", pack_as_nak, 
        "Force pack as NAK");
    pack_cmd->footer("\nExamples:\n"
                     "  nah pack ./myapp/                    # Auto-detect type\n"
                     "  nah pack ./myapp/ -o myapp-1.0.0.nap");
    add_common_flags(pack_cmd);
    pack_cmd->callback([&]() {
        std::optional<PackageType> force_type;
        if (pack_as_app) force_type = PackageType::App;
        if (pack_as_nak) force_type = PackageType::Nak;
        std::exit(cmd_pack(opts, pack_dir, pack_output, force_type));
    });
    
    // ========== status - Unified diagnostic command ==========
    std::string status_target;
    bool status_fix = false;
    std::string status_diff_profile;
    std::string status_overrides;
    auto status_cmd = app.add_subcommand("status", 
        "Show app contracts, validate files, or diagnose issues\n\n"
        "This is the main debugging command. Use it to:\n"
        "  - See what environment an app will run with\n"
        "  - Validate profile or manifest JSON files\n"
        "  - Diagnose NAK resolution issues\n"
        "  - Compare contracts across profiles");
    status_cmd->add_option("target", status_target, 
        "App/NAK ID, file path, or omit for overview");
    status_cmd->add_flag("--fix", status_fix, 
        "Attempt to fix issues (also formats files)");
    status_cmd->add_option("--diff", status_diff_profile, 
        "Compare contract with another profile");
    status_cmd->add_option("--overrides", status_overrides, 
        "Apply overrides file to contract");
    status_cmd->footer("\nExamples:\n"
                       "  nah status                          # Overview\n"
                       "  nah status com.example.app          # App contract\n"
                       "  nah status com.example.app --trace  # With provenance\n"
                       "  nah status profile.json             # Validate file\n"
                       "  nah status profile.json --fix       # Validate and format\n"
                       "  nah status ./myapp/                 # Check if packable\n"
                       "  nah status com.example.app --diff staging\n\n"
                       "Aliases: nah doctor, nah info");
    add_common_flags(status_cmd);
    status_cmd->callback([&]() {
        std::exit(cmd_status(opts, status_target, status_fix, status_diff_profile, status_overrides));
    });
    
    // ========== doctor - Alias for status ==========
    std::string doctor_target;
    auto doctor_cmd = app.add_subcommand("doctor", "Diagnose issues (alias for 'status')");
    doctor_cmd->add_option("target", doctor_target, 
        "App/NAK ID, file path, or omit for overview");
    add_common_flags(doctor_cmd);
    doctor_cmd->callback([&]() {
        std::exit(cmd_status(opts, doctor_target, false, "", ""));
    });
    
    // ========== info - Alias for status ==========
    std::string info_target;
    auto info_cmd = app.add_subcommand("info", "Show info (alias for 'status')");
    info_cmd->add_option("target", info_target, 
        "App/NAK ID, file path, or omit for overview");
    add_common_flags(info_cmd);
    info_cmd->callback([&]() {
        std::exit(cmd_status(opts, info_target, false, "", ""));
    });
    
    // ========== init - Project initialization ==========
    std::string init_type, init_dir;
    std::string init_from_profile;
    auto init_cmd = app.add_subcommand("init", "Create a new project or profile");
    init_cmd->add_option("type", init_type, 
        "Type of project: app, nak, root, or profile")->required()
        ->check(CLI::IsMember({"app", "nak", "root", "profile"}));
    init_cmd->add_option("name", init_dir, 
        "Directory to create (or profile name for 'profile' type)")->required();
    init_cmd->add_option("--from", init_from_profile,
        "Source profile to copy from (only for 'profile' type)");
    init_cmd->footer("\nExamples:\n"
                     "  nah init app ./myapp       # Create app project\n"
                     "  nah init nak ./mysdk       # Create NAK project\n"
                     "  nah init root ./my-nah     # Create NAH root\n"
                     "  nah init profile dev       # Create profile from default\n"
                     "  nah init profile prod --from staging");
    add_common_flags(init_cmd);
    init_cmd->callback([&]() {
        std::exit(cmd_init(opts, init_type, init_dir, init_from_profile));
    });
    
    // ========== profile - Profile management (simplified) ==========
    auto profile_cmd = app.add_subcommand("profile", "Manage host profiles");
    profile_cmd->require_subcommand(1);
    profile_cmd->failure_message(make_failure_handler());
    
    auto profile_list = profile_cmd->add_subcommand("list", "List available profiles");
    add_common_flags(profile_list);
    profile_list->callback([&]() { std::exit(cmd_profile_list(opts)); });
    
    std::string profile_name;
    auto profile_set = profile_cmd->add_subcommand("set", "Set the active profile");
    profile_set->add_option("name", profile_name, 
        "Profile name to activate")->required();
    add_common_flags(profile_set);
    profile_set->callback([&]() { std::exit(cmd_profile_set(opts, profile_name)); });
    
    // ========== manifest generate - Build tool command ==========
    auto manifest_cmd = app.add_subcommand("manifest", "Manifest tools");
    manifest_cmd->require_subcommand(1);
    manifest_cmd->failure_message(make_failure_handler());
    
    std::string manifest_input, manifest_output;
    auto manifest_generate = manifest_cmd->add_subcommand("generate", "Generate binary manifest from JSON");
    manifest_generate->add_option("input", manifest_input, 
        "Input JSON file")->required()->check(CLI::ExistingFile);
    manifest_generate->add_option("-o,--output", manifest_output, 
        "Output binary manifest file (.nah)")->required();
    manifest_generate->footer("\nExample:\n"
                               "  nah manifest generate manifest.json -o manifest.nah");
    add_common_flags(manifest_generate);
    manifest_generate->callback([&]() { std::exit(cmd_manifest_generate(opts, manifest_input, manifest_output)); });
    
    // Custom failure handler for better error messages
    app.failure_message([](const CLI::App* failed_app, const CLI::Error& e) {
        std::string error_msg = e.what();
        std::string result;
        
        // Find the actual app that triggered the error (may be a subcommand)
        const CLI::App* error_app = failed_app;
        std::vector<CLI::App*> parsed_subs = failed_app->get_subcommands();
        if (!parsed_subs.empty()) {
            error_app = parsed_subs.back();
        }
        
        // Get available subcommands from the error source
        std::vector<std::string> valid_cmds;
        for (const auto* sub : error_app->get_subcommands({})) {
            valid_cmds.push_back(sub->get_name());
        }
        
        // Check for missing subcommand errors
        if (error_msg.find("subcommand is required") != std::string::npos) {
            result = error_msg + "\n";
            if (!valid_cmds.empty()) {
                result += "\nAvailable commands:\n";
                for (const auto& cmd : valid_cmds) {
                    result += "  " + cmd + "\n";
                }
            }
            result += "\nRun 'nah " + error_app->get_name() + " --help' for usage.\n";
            return result;
        }
        
        // Check for unknown argument/subcommand errors
        if (error_msg.find("was not expected") != std::string::npos ||
            error_msg.find("could not be matched") != std::string::npos) {
            
            std::string unknown_cmd;
            auto colon_pos = error_msg.rfind(':');
            if (colon_pos != std::string::npos && colon_pos + 2 < error_msg.size()) {
                unknown_cmd = error_msg.substr(colon_pos + 2);
                while (!unknown_cmd.empty() && std::isspace(unknown_cmd.back())) {
                    unknown_cmd.pop_back();
                }
            }
            
            result = error_msg + "\n";
            
            // Find similar commands using Levenshtein distance
            if (!unknown_cmd.empty() && !valid_cmds.empty()) {
                auto suggestions = find_similar_commands(unknown_cmd, valid_cmds, 3);
                if (!suggestions.empty()) {
                    result += "\nDid you mean?\n";
                    for (const auto& s : suggestions) {
                        result += "  " + s + "\n";
                    }
                }
            }
            
            // Show available commands if no close match
            if (!valid_cmds.empty() && result.find("Did you mean") == std::string::npos) {
                result += "\nAvailable commands:\n";
                for (const auto& cmd : valid_cmds) {
                    result += "  " + cmd + "\n";
                }
            }
        } else {
            result = CLI::FailureMessage::simple(failed_app, e);
        }
        
        result += "\nRun '" + error_app->get_name() + " --help' for usage.\n";
        return result;
    });
    
    // Parse and run
    CLI11_PARSE(app, argc, argv);
    
    // If no subcommand, show help
    if (app.get_subcommands().empty()) {
        std::cout << app.help() << std::endl;
    }
    
    return 0;
}
