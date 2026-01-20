/**
 * NAH CLI - profile command
 * 
 * Manage host profiles.
 */

#include "../common.hpp"
#include <CLI/CLI.hpp>
#include <filesystem>

namespace nah::cli::commands {

namespace {

int cmd_profile_list(const GlobalOptions& opts) {
    init_warning_collector(opts.json, opts.quiet);
    
    std::string nah_root = resolve_nah_root(
        opts.root.empty() ? std::nullopt : std::make_optional(opts.root));
    auto paths = get_nah_paths(nah_root);
    
    auto files = nah::fs::list_directory(paths.profiles);
    
    std::vector<std::string> profiles;
    for (const auto& f : files) {
        if (f.size() > 5 && f.substr(f.size() - 5) == ".json") {
            profiles.push_back(f.substr(0, f.size() - 5));
        }
    }
    
    if (profiles.empty()) {
        if (opts.json) {
            nlohmann::json j;
            j["profiles"] = nlohmann::json::array();
            j["active"] = nullptr;
            output_json(j);
        } else {
            std::cout << "No profiles found." << std::endl;
        }
        return 0;
    }
    
    // Get active profile (from symlink)
    std::string active_profile = "default";
    auto current_path = paths.host + "/profile.current";
    if (nah::fs::exists(current_path)) {
        // Would need to resolve symlink to get actual name
        // For now, assume default
    }
    
    std::sort(profiles.begin(), profiles.end());
    
    if (opts.json) {
        nlohmann::json j;
        j["profiles"] = profiles;
        j["active"] = active_profile;
        output_json(j);
    } else {
        std::cout << "Available profiles:" << std::endl;
        for (const auto& p : profiles) {
            std::string marker = (p == active_profile) ? " (active)" : "";
            std::cout << "  " << p << marker << std::endl;
        }
    }
    
    return 0;
}

int cmd_profile_show(const GlobalOptions& opts, const std::string& name) {
    init_warning_collector(opts.json, opts.quiet);
    
    std::string nah_root = resolve_nah_root(
        opts.root.empty() ? std::nullopt : std::make_optional(opts.root));
    auto paths = get_nah_paths(nah_root);
    
    std::string profile_name = name;
    std::string profile_path;
    
    if (profile_name.empty()) {
        // Show active profile
        profile_path = paths.host + "/profile.current";
        if (!nah::fs::exists(profile_path)) {
            profile_path = paths.profiles + "/default.json";
            profile_name = "default";
        }
    } else {
        profile_path = paths.profiles + "/" + profile_name + ".json";
    }
    
    auto content = nah::fs::read_file(profile_path);
    if (!content) {
        print_error("Profile not found: " + profile_name, opts.json);
        return 1;
    }
    
    try {
        auto profile = nlohmann::json::parse(*content);
        
        if (opts.json) {
            profile["name"] = profile_name;
            profile["path"] = profile_path;
            output_json(profile);
        } else {
            std::cout << "Profile: " << profile_name << std::endl;
            std::cout << "Path: " << profile_path << std::endl;
            std::cout << std::endl;
            
            if (profile.contains("nak")) {
                auto& nak = profile["nak"];
                bool has_config = (nak.contains("allow_versions") && !nak["allow_versions"].empty()) ||
                                  (nak.contains("deny_versions") && !nak["deny_versions"].empty());
                if (has_config) {
                    std::cout << "NAK Configuration:" << std::endl;
                    if (nak.contains("allow_versions") && !nak["allow_versions"].empty()) {
                        std::cout << "  Allow: ";
                        for (size_t i = 0; i < nak["allow_versions"].size(); i++) {
                            if (i > 0) std::cout << ", ";
                            std::cout << nak["allow_versions"][i].get<std::string>();
                        }
                        std::cout << std::endl;
                    }
                    if (nak.contains("deny_versions") && !nak["deny_versions"].empty()) {
                        std::cout << "  Deny: ";
                        for (size_t i = 0; i < nak["deny_versions"].size(); i++) {
                            if (i > 0) std::cout << ", ";
                            std::cout << nak["deny_versions"][i].get<std::string>();
                        }
                        std::cout << std::endl;
                    }
                }
            }
            
            if (profile.contains("environment") && !profile["environment"].empty()) {
                std::cout << "\nEnvironment:" << std::endl;
                for (auto& [key, value] : profile["environment"].items()) {
                    std::cout << "  " << key << "=" << value.get<std::string>() << std::endl;
                }
            }
            
            if (profile.contains("warnings") && !profile["warnings"].empty()) {
                std::cout << "\nWarning Policy:" << std::endl;
                for (auto& [key, action] : profile["warnings"].items()) {
                    std::cout << "  " << key << ": " << action.get<std::string>() << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        print_error("Failed to parse profile: " + std::string(e.what()), opts.json);
        return 1;
    }
    
    return 0;
}

int cmd_profile_set(const GlobalOptions& opts, const std::string& name) {
    init_warning_collector(opts.json, opts.quiet);
    
    std::string nah_root = resolve_nah_root(
        opts.root.empty() ? std::nullopt : std::make_optional(opts.root));
    auto paths = get_nah_paths(nah_root);
    
    std::string profile_path = paths.profiles + "/" + name + ".json";
    
    if (!nah::fs::exists(profile_path)) {
        print_error("Profile not found: " + name, opts.json);
        return 1;
    }
    
    std::string current_path = paths.host + "/profile.current";
    
    // Remove existing symlink
    if (nah::fs::exists(current_path)) {
        nah::fs::remove_file(current_path);
    }
    
    // Create new symlink
    try {
        std::filesystem::create_symlink(profile_path, current_path);
    } catch (const std::exception& e) {
        print_error("Failed to set active profile: " + std::string(e.what()), opts.json);
        return 1;
    }
    
    if (opts.json) {
        nlohmann::json j;
        j["active"] = name;
        j["path"] = profile_path;
        output_json(j);
    } else {
        std::cout << "Active profile set to: " << name << std::endl;
    }
    
    return 0;
}

} // anonymous namespace

void setup_profile(CLI::App* app, GlobalOptions& opts) {
    app->require_subcommand(1);
    
    // profile list
    auto* list_cmd = app->add_subcommand("list", "List available profiles");
    list_cmd->callback([&opts]() {
        std::exit(cmd_profile_list(opts));
    });
    
    // profile show [name]
    static std::string show_name;
    auto* show_cmd = app->add_subcommand("show", "Show profile details");
    show_cmd->add_option("name", show_name, "Profile name (defaults to active)");
    show_cmd->callback([&opts]() {
        std::exit(cmd_profile_show(opts, show_name));
    });
    
    // profile set <name>
    static std::string set_name;
    auto* set_cmd = app->add_subcommand("set", "Set active profile");
    set_cmd->add_option("name", set_name, "Profile name")->required();
    set_cmd->callback([&opts]() {
        std::exit(cmd_profile_set(opts, set_name));
    });
}

} // namespace nah::cli::commands
