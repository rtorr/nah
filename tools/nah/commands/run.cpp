/**
 * NAH CLI - run command (refactored to use NahHost)
 *
 * Launch an application using the NahHost library.
 */

#include "../common.hpp"
#include <nah/nah_host.h>
#include <CLI/CLI.hpp>
#include <cerrno>
#include <cstring>
#include <cstdlib>

namespace nah::cli::commands {

namespace {

struct RunOptions {
    std::string target;
    std::vector<std::string> args;
    std::string loader;
    bool require_verified = false;
};

int cmd_run(const GlobalOptions& opts, const RunOptions& run_opts) {
    init_warning_collector(opts.json, opts.quiet);

    if (opts.json) {
        print_error("--json is not supported by run; use show to inspect a contract", true);
        return 1;
    }

    std::string nah_root = resolve_nah_root(
        opts.root.empty() ? std::nullopt : std::make_optional(opts.root));

    // Create NahHost instance
    auto host = nah::host::NahHost::create(nah_root);
    if (!host) {
        print_error("Failed to initialize NAH host", opts.json);
        return 1;
    }

    // Parse target (app_id or app_id@version)
    std::string app_id = run_opts.target;
    std::string version;
    size_t at_pos = run_opts.target.find('@');
    if (at_pos != std::string::npos) {
        app_id = run_opts.target.substr(0, at_pos);
        version = run_opts.target.substr(at_pos + 1);
    }

    // Get the launch contract from NahHost
    nah::core::CompositionOptions comp_opts;
    comp_opts.enable_trace = opts.trace;
    comp_opts.now = nah::core::get_current_timestamp();
    if (!run_opts.loader.empty()) {
        comp_opts.loader_override = run_opts.loader;
    }
    auto result = host->getLaunchContract(app_id, version, comp_opts);

    if (!result.ok) {
        print_error("Composition failed: " + result.critical_error_context, opts.json);
        return 1;
    }

    // Display any warnings from composition
    if (!opts.json && !opts.quiet && !result.warnings.empty()) {
        for (const auto& warning : result.warnings) {
            std::string warning_msg = "Warning [" + warning.key + "]: ";
            for (const auto& [field, value] : warning.fields) {
                warning_msg += field + "=" + value + " ";
            }
            print_warning(warning_msg, opts.json);
        }
    }

    if (run_opts.require_verified && result.contract.trust.state != nah::core::TrustState::Verified) {
        print_error(std::string("launch requires verified artifacts; state is ") +
                    nah::core::trust_state_to_string(result.contract.trust.state), opts.json);
        return 1;
    }

    // Add any extra args from command line
    for (const auto& arg : run_opts.args) {
        result.contract.execution.arguments.push_back(arg);
    }

    if (!opts.quiet) {
        std::cout << "Running " << result.contract.app.id
                  << "@" << result.contract.app.version << "..." << std::endl;
    }

    // Execute using exec_replace (which replaces the current process)
    auto exec_result = nah::exec::exec_replace(result.contract);

    // If exec_replace succeeds, we won't get here (process is replaced)
    // If we're here, it means exec failed
    if (!exec_result.ok && !opts.quiet) {
        print_error("Failed to execute: " + exec_result.error, false);
    }

    return exec_result.exit_code;
}

} // anonymous namespace

void setup_run(CLI::App* app, GlobalOptions& opts) {
    static RunOptions run_opts;

    app->add_option("target", run_opts.target, "App to run (id or id@version)")->required();
    app->add_option("args", run_opts.args, "Arguments to pass to the app");
    app->add_option("--loader", run_opts.loader, "Loader to use (overrides install record)");
    app->add_flag("--require-verified", run_opts.require_verified,
                  "Refuse to launch unless all artifacts were digest-verified");

    // Allow -- to separate nah args from app args
    app->allow_extras();

    app->callback([&opts]() {
        std::exit(cmd_run(opts, run_opts));
    });
}

} // namespace nah::cli::commands
