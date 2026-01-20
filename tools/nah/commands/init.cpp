/**
 * NAH CLI - init command
 * 
 * Scaffold a new NAH project.
 */

#include "../common.hpp"
#include <CLI/CLI.hpp>
#include <fstream>
#include <filesystem>

namespace nah::cli::commands {

namespace {

struct InitOptions {
    bool as_app = false;
    bool as_nak = false;
    bool as_host = false;
    std::string id;
    std::string name;
    std::string dir = ".";
};

int cmd_init(const GlobalOptions& opts, const InitOptions& init_opts) {
    init_warning_collector(opts.json, opts.quiet);
    
    std::string target_dir = init_opts.dir;
    
    // Determine type
    std::string type = "app"; // Default
    if (init_opts.as_nak) type = "nak";
    if (init_opts.as_host) type = "host";
    
    // Derive ID from directory name if not provided
    std::string id = init_opts.id;
    if (id.empty()) {
        std::filesystem::path dir_path = std::filesystem::absolute(target_dir);
        std::string dirname = dir_path.filename().string();
        if (dirname.empty() || dirname == ".") {
            dirname = dir_path.parent_path().filename().string();
        }
        id = "com.example." + dirname;
    }
    
    std::string manifest_path = target_dir + "/nah.json";
    
    if (nah::fs::exists(manifest_path)) {
        print_error("nah.json already exists in " + target_dir, opts.json);
        return 1;
    }
    
    // Create directory if needed
    std::filesystem::create_directories(target_dir);
    
    nlohmann::json manifest;
    
    if (type == "app") {
        manifest["id"] = id;
        manifest["version"] = "0.1.0";
        manifest["entrypoint_path"] = "bin/app";
        if (!init_opts.name.empty()) {
            manifest["name"] = init_opts.name;
        }
        
        // Create bin directory with placeholder
        std::filesystem::create_directories(target_dir + "/bin");
        std::ofstream app_file(target_dir + "/bin/app");
        app_file << "#!/bin/bash\necho \"Hello from " << id << "\"\n";
        app_file.close();
        std::filesystem::permissions(target_dir + "/bin/app", 
            std::filesystem::perms::owner_exec | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::add);
            
    } else if (type == "nak") {
        manifest["id"] = id;
        manifest["version"] = "0.1.0";
        manifest["lib_dirs"] = nlohmann::json::array({"lib"});
        if (!init_opts.name.empty()) {
            manifest["name"] = init_opts.name;
        }
        
        // Create lib directory
        std::filesystem::create_directories(target_dir + "/lib");
        
    } else if (type == "host") {
        manifest["root"] = "./nah_root";
        manifest["install"] = nlohmann::json::array();
        
        // Create host directory with empty host.json
        std::filesystem::create_directories(target_dir + "/host");
        
        nlohmann::json host_env;
        host_env["environment"] = nlohmann::json::object();
        
        std::ofstream host_file(target_dir + "/host/host.json");
        host_file << host_env.dump(2);
        host_file.close();
    }
    
    // Write manifest
    std::ofstream manifest_file(manifest_path);
    manifest_file << manifest.dump(2);
    manifest_file.close();
    
    if (opts.json) {
        nlohmann::json j;
        j["ok"] = true;
        j["type"] = type;
        j["id"] = id;
        j["path"] = manifest_path;
        output_json(j);
    } else {
        std::cout << "Created nah.json for " << type << ": " << id << std::endl;
        std::cout << std::endl;
        std::cout << "Next steps:" << std::endl;
        if (type == "app") {
            std::cout << "  nah run .              # Run from source (dev mode)" << std::endl;
            std::cout << "  nah pack .             # Create .nap package" << std::endl;
            std::cout << "  nah install .          # Pack and install" << std::endl;
        } else if (type == "nak") {
            std::cout << "  nah pack .             # Create .nak package" << std::endl;
            std::cout << "  nah install .          # Pack and install" << std::endl;
        } else {
            std::cout << "  nah host install .     # Set up NAH root" << std::endl;
        }
    }
    
    return 0;
}

} // anonymous namespace

void setup_init(CLI::App* app, GlobalOptions& opts) {
    static InitOptions init_opts;
    
    app->add_flag("--app", init_opts.as_app, "Create an app project");
    app->add_flag("--nak", init_opts.as_nak, "Create a NAK project");
    app->add_flag("--host", init_opts.as_host, "Create a host setup directory");
    app->add_option("--id", init_opts.id, "Package identifier");
    app->add_option("--name", init_opts.name, "Human-readable name");
    app->add_option("dir", init_opts.dir, "Target directory (default: current)");
    
    app->callback([&opts]() {
        std::exit(cmd_init(opts, init_opts));
    });
}

} // namespace nah::cli::commands
