/**
 * NAH CLI - launch command
 *
 * Launch a component via protocol URL.
 */

#define NAH_HOST_IMPLEMENTATION
#include "../common.hpp"
#include <nah/nah_host.h>
#include <CLI/CLI.hpp>

namespace nah::cli::commands {

namespace {

struct LaunchOptions {
    std::string uri;
    std::vector<std::string> args;
    std::string referrer;
};

int cmd_launch(const GlobalOptions& opts, const LaunchOptions& launch_opts) {
    init_warning_collector(opts.json, opts.quiet);
    
    std::string nah_root = resolve_nah_root(
        opts.root.empty() ? std::nullopt : std::make_optional(opts.root));
    
    // Create NahHost instance
    auto host = nah::host::NahHost::create(nah_root);
    if (!host) {
        print_error("Failed to initialize NAH host", opts.json);
        return 1;
    }
    
    // Check if URI can be handled
    if (!host->canHandleComponentUri(launch_opts.uri)) {
        print_error("No component found for URI: " + launch_opts.uri, opts.json);
        if (!opts.json && !opts.quiet) {
            std::cerr << "Use 'nah components' to see available components\n";
        }
        return 1;
    }
    
    // Launch component
    int exit_code = host->launchComponent(
        launch_opts.uri,
        launch_opts.referrer,
        launch_opts.args
    );
    
    return exit_code;
}

} // namespace

void register_launch_command(CLI::App& app, GlobalOptions& opts) {
    LaunchOptions launch_opts;
    
    auto cmd = app.add_subcommand("launch", "Launch a component via protocol URL");
    cmd->add_option("uri", launch_opts.uri, "Component URI (e.g., com.suite://editor)")
        ->required();
    cmd->add_option("args", launch_opts.args, "Additional arguments");
    cmd->add_option("--referrer", launch_opts.referrer, "Referrer URI (for context)");
    
    cmd->callback([&]() {
        std::exit(cmd_launch(opts, launch_opts));
    });
}

} // namespace nah::cli::commands
