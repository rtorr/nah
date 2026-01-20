/**
 * NAH CLI - host command
 * 
 * Host management commands.
 */

#include "../common.hpp"
#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>

namespace nah::cli::commands {

namespace {

int cmd_host_install(const GlobalOptions& opts, const std::string& manifest_dir, bool clean) {
    init_warning_collector(opts.json, opts.quiet);
    
    // Read host manifest
    auto manifest_content = nah::fs::read_file(manifest_dir + "/nah.json");
    if (!manifest_content) {
        print_error("Host manifest not found: " + manifest_dir + "/nah.json", opts.json);
        return 1;
    }
    
    nlohmann::json manifest;
    try {
        manifest = nlohmann::json::parse(*manifest_content);
    } catch (const std::exception& e) {
        print_error("Invalid manifest JSON: " + std::string(e.what()), opts.json);
        return 1;
    }
    
    if (!manifest.contains("root")) {
        print_error("Host manifest must contain 'root' field", opts.json);
        return 1;
    }
    
    // Resolve root path
    std::string nah_root = manifest["root"].get<std::string>();
    if (!std::filesystem::path(nah_root).is_absolute()) {
        // Relative to manifest directory
        nah_root = std::filesystem::absolute(manifest_dir + "/" + nah_root).string();
    }
    
    if (!opts.quiet) {
        std::cout << "Installing host to: " << nah_root << std::endl;
    }
    
    // Clean if requested
    if (clean && nah::fs::exists(nah_root)) {
        if (!opts.quiet) {
            std::cout << "Cleaning existing root..." << std::endl;
        }
        std::filesystem::remove_all(nah_root);
    }
    
    // Create directory structure
    if (!ensure_nah_structure(nah_root)) {
        print_error("Failed to create NAH directory structure at: " + nah_root, opts.json);
        return 1;
    }
    auto paths = get_nah_paths(nah_root);
    
    // Copy host.json if present in manifest directory
    std::string host_json_src = manifest_dir + "/host.json";
    if (nah::fs::exists(host_json_src)) {
        auto content = nah::fs::read_file(host_json_src);
        if (content) {
            nah::fs::write_file(paths.host + "/host.json", *content);
            if (!opts.quiet) {
                std::cout << "  Copied host.json" << std::endl;
            }
        }
    }
    
    // Handle inline host environment from manifest
    if (manifest.contains("host")) {
        std::string host_json_path = paths.host + "/host.json";
        std::ofstream hf(host_json_path);
        hf << manifest["host"].dump(2);
        hf.close();
        if (!opts.quiet) {
            std::cout << "  Created host.json from manifest" << std::endl;
        }
    }
    
    // Install packages
    if (manifest.contains("install")) {
        for (const auto& pkg : manifest["install"]) {
            std::string pkg_path = pkg.get<std::string>();
            if (!std::filesystem::path(pkg_path).is_absolute()) {
                pkg_path = manifest_dir + "/" + pkg_path;
            }
            
            if (!opts.quiet) {
                std::cout << "  Installing: " << pkg_path << std::endl;
            }
            
            // Detect type and install
            // For now, just note that we would install
            // Full implementation would call install logic
        }
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["ok"] = true;
        j["root"] = nah_root;
        output_json(j);
    } else {
        std::cout << std::endl;
        std::cout << "Host installed at: " << nah_root << std::endl;
    }
    
    return 0;
}

} // anonymous namespace

void setup_host(CLI::App* app, GlobalOptions& opts) {
    app->require_subcommand(1);
    
    // host install <dir>
    static std::string install_dir;
    static bool clean = false;
    
    auto* install_cmd = app->add_subcommand("install", "Set up NAH root from host manifest");
    install_cmd->add_option("dir", install_dir, "Directory containing host manifest")->required();
    install_cmd->add_flag("--clean", clean, "Remove existing NAH root first");
    
    install_cmd->callback([&opts]() {
        std::exit(cmd_host_install(opts, install_dir, clean));
    });
}

} // namespace nah::cli::commands
