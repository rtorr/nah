/**
 * NAH CLI - run command (refactored to use NahHost)
 *
 * Launch an application using the NahHost library.
 */

// Enable host implementation in this translation unit
#define NAH_HOST_IMPLEMENTATION

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
    std::string loader;  // Runtime loader override
};

int cmd_run(const GlobalOptions& opts, const RunOptions& run_opts) {
    init_warning_collector(opts.json, opts.quiet);

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
            if (warning.action == "error") {
                // This was escalated to an error by policy
                print_error(warning_msg + "escalated to error by policy", opts.json);
            } else {
                // Regular warning
                for (const auto& [field, value] : warning.fields) {
                    warning_msg += field + "=" + value + " ";
                }
                print_warning(warning_msg, opts.json);
            }
        }
    }

    // Check trust state if enforcement is enabled
    std::string require_trust_value = safe_getenv("NAH_REQUIRE_TRUST");
    if (!require_trust_value.empty()) {
        auto trust_state = result.contract.trust.state;
        if (trust_state != nah::core::TrustState::Verified) {
            std::string trust_msg = std::string("Trust verification failed: state is ") +
                nah::core::trust_state_to_string(trust_state);
            if (trust_state == nah::core::TrustState::Failed) {
                print_error(trust_msg, opts.json);
                return 1;
            } else if (trust_state == nah::core::TrustState::Unknown ||
                       trust_state == nah::core::TrustState::Unverified) {
                if (require_trust_value == "1" || require_trust_value == "true") {
                    print_error(trust_msg + ". Set NAH_REQUIRE_TRUST=0 to bypass.", opts.json);
                    return 1;
                } else {
                    print_warning(trust_msg, opts.json);
                }
            }
        }
    }

    // Apply overrides from environment
    auto host_env = host->getHostEnvironment();
    nah::overrides::apply_overrides(result, host_env);

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
    if (opts.json) {
        nlohmann::json j;
        j["ok"] = exec_result.ok;
        j["exit_code"] = exec_result.exit_code;
        if (!exec_result.error.empty()) {
            j["error"] = exec_result.error;
        }
        output_json(j);
    } else if (!exec_result.ok && !opts.quiet) {
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

    // Allow -- to separate nah args from app args
    app->allow_extras();

    app->callback([&opts]() {
        std::exit(cmd_run(opts, run_opts));
    });
}

} // namespace nah::cli::commands