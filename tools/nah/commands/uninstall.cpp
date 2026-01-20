/**
 * NAH CLI - uninstall command
 * 
 * Remove an installed app or NAK.
 */

#include "../common.hpp"
#include <CLI/CLI.hpp>
#include <filesystem>

namespace nah::cli::commands {

namespace {

struct UninstallOptions {
    std::string target;
    bool as_app = false;
    bool as_nak = false;
    bool force = false;
};

int cmd_uninstall(const GlobalOptions& opts, const UninstallOptions& uninstall_opts) {
    init_warning_collector(opts.json, opts.quiet);
    
    std::string nah_root = resolve_nah_root(
        opts.root.empty() ? std::nullopt : std::make_optional(opts.root));
    auto paths = get_nah_paths(nah_root);
    
    auto parsed = parse_target(uninstall_opts.target);
    
    // Determine type
    bool is_app = false;
    bool is_nak = false;
    
    if (uninstall_opts.as_app) {
        is_app = true;
    } else if (uninstall_opts.as_nak) {
        is_nak = true;
    } else {
        // Auto-detect
        auto app_files = nah::fs::list_directory(paths.registry_apps);
        auto nak_files = nah::fs::list_directory(paths.registry_naks);

        for (const auto& f : app_files) {
            if (f.find(parsed.id + "@") == 0) {
                is_app = true;
                break;
            }
        }
        for (const auto& f : nak_files) {
            if (f.find(parsed.id + "@") == 0) {
                is_nak = true;
                break;
            }
        }
        
        if (is_app && is_nak) {
            print_error("Ambiguous target: " + parsed.id + " exists as both app and NAK. Use --app or --nak.", opts.json);
            return 1;
        }
    }
    
    if (!is_app && !is_nak) {
        print_error("Package not found: " + uninstall_opts.target, opts.json);
        return 1;
    }
    
    if (is_app) {
        // Find the record (files named: id@version.json)
        std::string record_path;
        std::string version;
        
        if (parsed.version) {
            record_path = paths.registry_apps + "/" + parsed.id + "@" + *parsed.version + ".json";
            version = *parsed.version;
        } else {
            // Find any version
            auto files = nah::fs::list_directory(paths.registry_apps);
            for (const auto& filepath : files) {
                std::string f = filepath;
                size_t last_slash = filepath.rfind('/');
                if (last_slash != std::string::npos) {
                    f = filepath.substr(last_slash + 1);
                }
                if (f.find(parsed.id + "@") == 0 && f.size() > 5 && f.substr(f.size() - 5) == ".json") {
                    record_path = filepath;
                    // Extract version from filename: id@version.json
                    auto at_pos = f.find('@');
                    version = f.substr(at_pos + 1, f.size() - at_pos - 6);
                    break;
                }
            }
        }
        
        if (record_path.empty() || !nah::fs::exists(record_path)) {
            print_error("App not installed: " + uninstall_opts.target, opts.json);
            return 1;
        }
        
        // Read record to get install path
        auto content = nah::fs::read_file(record_path);
        if (content) {
            try {
                auto record = nlohmann::json::parse(*content);
                std::string install_dir = record["paths"]["install_root"].get<std::string>();
                
                // Remove install directory
                if (nah::fs::exists(install_dir)) {
                    std::error_code ec;
                    std::filesystem::remove_all(install_dir, ec);
                    if (ec) {
                        print_warning("Could not fully remove install directory: " + ec.message(), opts.json);
                    }
                }
            } catch (const std::exception& e) {
                print_warning("Could not parse install record to find install directory: " + std::string(e.what()), opts.json);
            }
        }
        
        // Remove record
        nah::fs::remove_file(record_path);
        
        if (opts.json) {
            nlohmann::json j;
            j["ok"] = true;
            j["app"]["id"] = parsed.id;
            j["app"]["version"] = version;
            output_json(j);
        } else {
            std::cout << "Uninstalled " << parsed.id << "@" << version << std::endl;
        }
    } else {
        // NAK uninstall (files named: id@version.json)
        std::string record_path;
        std::string version;
        
        if (parsed.version) {
            record_path = paths.registry_naks + "/" + parsed.id + "@" + *parsed.version + ".json";
            version = *parsed.version;
        } else {
            auto files = nah::fs::list_directory(paths.registry_naks);
            for (const auto& filepath : files) {
                std::string f = filepath;
                size_t last_slash = filepath.rfind('/');
                if (last_slash != std::string::npos) {
                    f = filepath.substr(last_slash + 1);
                }
                if (f.find(parsed.id + "@") == 0 && f.size() > 5 && f.substr(f.size() - 5) == ".json") {
                    record_path = filepath;
                    auto at_pos = f.find('@');
                    version = f.substr(at_pos + 1, f.size() - at_pos - 6);
                    break;
                }
            }
        }
        
        if (record_path.empty() || !nah::fs::exists(record_path)) {
            print_error("NAK not installed: " + uninstall_opts.target, opts.json);
            return 1;
        }
        
        // Check if any apps reference this NAK
        if (!uninstall_opts.force) {
            auto app_files = nah::fs::list_directory(paths.registry_apps);
            std::vector<std::string> referencing_apps;

            for (const auto& f : app_files) {
                if (f.substr(f.size() - 5) == ".json") {
                    auto app_content = nah::fs::read_file(paths.registry_apps + "/" + f);
                    if (app_content) {
                        try {
                            auto app_record = nlohmann::json::parse(*app_content);
                            if (app_record.contains("nak") &&
                                app_record["nak"].value("id", "") == parsed.id) {
                                referencing_apps.push_back(f.substr(0, f.size() - 5));
                            }
                        } catch (...) {}
                    }
                }
            }
            
            if (!referencing_apps.empty()) {
                std::string apps_list;
                for (size_t i = 0; i < referencing_apps.size(); i++) {
                    if (i > 0) apps_list += ", ";
                    apps_list += referencing_apps[i];
                }
                print_error("NAK " + parsed.id + "@" + version + " is used by: " + apps_list + 
                           ". Use --force to remove anyway.", opts.json);
                return 1;
            }
        }
        
        // Read record to get install path
        auto content = nah::fs::read_file(record_path);
        if (content) {
            try {
                auto record = nlohmann::json::parse(*content);
                std::string install_dir = record["paths"]["root"].get<std::string>();
                
                if (nah::fs::exists(install_dir)) {
                    std::error_code ec;
                    std::filesystem::remove_all(install_dir, ec);
                    if (ec) {
                        print_warning("Could not fully remove NAK directory: " + ec.message(), opts.json);
                    }
                }
            } catch (const std::exception& e) {
                print_warning("Could not parse NAK record to find install directory: " + std::string(e.what()), opts.json);
            }
        }
        
        // Remove record
        nah::fs::remove_file(record_path);
        
        if (opts.json) {
            nlohmann::json j;
            j["ok"] = true;
            j["nak"]["id"] = parsed.id;
            j["nak"]["version"] = version;
            output_json(j);
        } else {
            std::cout << "Uninstalled NAK " << parsed.id << "@" << version << std::endl;
        }
    }
    
    return 0;
}

} // anonymous namespace

void setup_uninstall(CLI::App* app, GlobalOptions& opts) {
    static UninstallOptions uninstall_opts;
    
    app->add_option("target", uninstall_opts.target, "Package to uninstall (id or id@version)")->required();
    app->add_flag("--app", uninstall_opts.as_app, "Force treat as app");
    app->add_flag("--nak", uninstall_opts.as_nak, "Force treat as NAK");
    app->add_flag("-f,--force", uninstall_opts.force, "Remove even if NAK is referenced by apps");
    
    app->callback([&opts]() {
        std::exit(cmd_uninstall(opts, uninstall_opts));
    });
}

} // namespace nah::cli::commands
