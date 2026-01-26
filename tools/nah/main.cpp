/**
 * NAH CLI - Entry Point
 * 
 * Native Application Host command-line interface.
 */

#include <CLI/CLI.hpp>
#include "common.hpp"

// Forward declarations for commands
namespace nah::cli::commands {
    void setup_init(CLI::App* app, GlobalOptions& opts);
    void setup_install(CLI::App* app, GlobalOptions& opts);
    void setup_uninstall(CLI::App* app, GlobalOptions& opts);
    void setup_run(CLI::App* app, GlobalOptions& opts);
    void setup_list(CLI::App* app, GlobalOptions& opts);
    void setup_show(CLI::App* app, GlobalOptions& opts);
    void setup_which(CLI::App* app, GlobalOptions& opts);
    void setup_pack(CLI::App* app, GlobalOptions& opts);
    void register_launch_command(CLI::App& app, GlobalOptions& opts);
    void register_components_command(CLI::App& app, GlobalOptions& opts);
}

int main(int argc, char** argv) {
    using namespace nah::cli;
    
    CLI::App app{"nah - Native Application Host"};
    app.set_version_flag("-V,--version", NAH_VERSION);
    app.require_subcommand(0, 1);
    
    GlobalOptions opts;
    
    // Global options
    app.add_option("--root", opts.root, "NAH root directory");
    app.add_flag("--json", opts.json, "Machine-readable output");
    app.add_flag("--trace", opts.trace, "Include provenance info");
    app.add_flag("-v,--verbose", opts.verbose, "Detailed progress");
    app.add_flag("-q,--quiet", opts.quiet, "Minimal output");
    
    // Commands
    auto* init_cmd = app.add_subcommand("init", "Scaffold a new project");
    commands::setup_init(init_cmd, opts);
    
    auto* install_cmd = app.add_subcommand("install", "Install an app or NAK");
    commands::setup_install(install_cmd, opts);
    
    auto* uninstall_cmd = app.add_subcommand("uninstall", "Remove an installed package");
    commands::setup_uninstall(uninstall_cmd, opts);
    
    auto* run_cmd = app.add_subcommand("run", "Launch an application");
    commands::setup_run(run_cmd, opts);
    
    auto* list_cmd = app.add_subcommand("list", "List installed packages");
    commands::setup_list(list_cmd, opts);
    
    auto* show_cmd = app.add_subcommand("show", "Debug and inspect");
    commands::setup_show(show_cmd, opts);
    
    auto* which_cmd = app.add_subcommand("which", "Print installation paths");
    commands::setup_which(which_cmd, opts);
    
    auto* pack_cmd = app.add_subcommand("pack", "Create a .nap or .nak package");
    commands::setup_pack(pack_cmd, opts);
    
    // Component commands
    commands::register_launch_command(app, opts);
    commands::register_components_command(app, opts);

    CLI11_PARSE(app, argc, argv);
    
    // If no subcommand, show help
    if (app.get_subcommands().empty()) {
        std::cout << app.help() << std::endl;
    }
    
    return 0;
}
