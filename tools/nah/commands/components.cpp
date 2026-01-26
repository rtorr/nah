/**
 * NAH CLI - components command
 *
 * List all components across all installed applications.
 */

#define NAH_HOST_IMPLEMENTATION
#include "../common.hpp"
#include <nah/nah_host.h>
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

namespace nah::cli::commands {

namespace {

struct ComponentsOptions {
    std::string app_filter;
};

int cmd_components(const GlobalOptions& opts, const ComponentsOptions& comp_opts) {
    init_warning_collector(opts.json, opts.quiet);
    
    std::string nah_root = resolve_nah_root(
        opts.root.empty() ? std::nullopt : std::make_optional(opts.root));
    
    auto host = nah::host::NahHost::create(nah_root);
    if (!host) {
        print_error("Failed to initialize NAH host", opts.json);
        return 1;
    }
    
    auto all_components = host->listAllComponents();
    
    if (all_components.empty()) {
        if (!opts.json && !opts.quiet) {
            std::cout << "No components found\n";
        }
        return 0;
    }
    
    // Filter by app if specified
    std::vector<std::pair<std::string, nah::core::ComponentDecl>> filtered;
    if (!comp_opts.app_filter.empty()) {
        for (const auto& [app_id, comp] : all_components) {
            if (app_id == comp_opts.app_filter) {
                filtered.push_back({app_id, comp});
            }
        }
    } else {
        filtered = all_components;
    }
    
    if (opts.json) {
        // JSON output
        nlohmann::json output = nlohmann::json::array();
        for (const auto& [app_id, comp] : filtered) {
            nlohmann::json comp_json;
            comp_json["app_id"] = app_id;
            comp_json["component_id"] = comp.id;
            comp_json["name"] = comp.name;
            comp_json["uri_pattern"] = comp.uri_pattern;
            comp_json["standalone"] = comp.standalone;
            comp_json["hidden"] = comp.hidden;
            if (!comp.loader.empty()) {
                comp_json["loader"] = comp.loader;
            }
            output.push_back(comp_json);
        }
        std::cout << output.dump(2) << "\n";
    } else {
        // Human-readable output
        std::string current_app;
        for (const auto& [app_id, comp] : filtered) {
            if (app_id != current_app) {
                std::cout << "\n" << app_id << ":\n";
                current_app = app_id;
            }
            
            std::cout << "  " << comp.id;
            if (!comp.name.empty()) {
                std::cout << " (" << comp.name << ")";
            }
            std::cout << "\n";
            std::cout << "    URI: " << comp.uri_pattern << "\n";
            std::cout << "    Standalone: " << (comp.standalone ? "yes" : "no") << "\n";
            if (!comp.loader.empty()) {
                std::cout << "    Loader: " << comp.loader << "\n";
            }
        }
    }
    
    return 0;
}

} // namespace

void register_components_command(CLI::App& app, GlobalOptions& opts) {
    ComponentsOptions comp_opts;
    
    auto cmd = app.add_subcommand("components", "List all components");
    cmd->add_option("--app", comp_opts.app_filter, "Filter by app ID");
    
    cmd->callback([&]() {
        std::exit(cmd_components(opts, comp_opts));
    });
}

} // namespace nah::cli::commands
