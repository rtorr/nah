/**
 * NAH CLI - show command
 *
 * Debug and inspect NAH state and contracts.
 */

#include "../common.hpp"
#include <CLI/CLI.hpp>

namespace nah::cli::commands
{

    namespace
    {

        struct ShowOptions
        {
            std::string target;
            bool trace_flag = false;
        };

        int cmd_show(const GlobalOptions &opts, const ShowOptions &show_opts)
        {
            init_warning_collector(opts.json, opts.quiet);

            std::string nah_root = resolve_nah_root(
                opts.root.empty() ? std::nullopt : std::make_optional(opts.root));
            auto paths = get_nah_paths(nah_root);

            // If no target, show overview
            if (show_opts.target.empty())
            {
                // Overview mode
                auto app_files = nah::fs::list_directory(paths.registry_apps);
                auto nak_files = nah::fs::list_directory(paths.registry_naks);

                int app_count = 0, nak_count = 0;
                for (const auto &f : app_files)
                {
                    if (f.size() > 5 && f.substr(f.size() - 5) == ".json")
                        app_count++;
                }
                for (const auto &f : nak_files)
                {
                    if (f.size() > 5 && f.substr(f.size() - 5) == ".json")
                        nak_count++;
                }

                // Check if host.json exists
                bool has_host_config = nah::fs::exists(paths.host + "/host.json");

                if (opts.json)
                {
                    nlohmann::json j;
                    j["root"] = nah_root;
                    j["apps"] = app_count;
                    j["naks"] = nak_count;
                    j["host_configured"] = has_host_config;
                    output_json(j);
                }
                else
                {
                    std::cout << "NAH Status" << std::endl;
                    std::cout << "  Root: " << nah_root << std::endl;
                    std::cout << "  Host Config: " << (has_host_config ? "present" : "not configured") << std::endl;
                    std::cout << "  Apps: " << app_count << " installed" << std::endl;
                    std::cout << "  NAKs: " << nak_count << " installed" << std::endl;
                    std::cout << std::endl;
                    std::cout << "Run 'nah show <app-id>' to check a specific app." << std::endl;
                }
                return 0;
            }

            // Contract mode - show launch contract for an app
            auto parsed = parse_target(show_opts.target);

            // Find the app install record (files named: id@version.json)
            std::string record_path;
            if (parsed.version)
            {
                record_path = paths.registry_apps + "/" + parsed.id + "@" + *parsed.version + ".json";
            }
            else
            {
                // Find any version
                auto files = nah::fs::list_directory(paths.registry_apps);
                for (const auto &filepath : files)
                {
                    // Extract filename from full path
                    std::string f = filepath;
                    size_t last_slash = filepath.rfind('/');
                    if (last_slash != std::string::npos)
                    {
                        f = filepath.substr(last_slash + 1);
                    }
                    if (f.find(parsed.id + "@") == 0 && f.size() > 5 && f.substr(f.size() - 5) == ".json")
                    {
                        record_path = filepath;
                        break;
                    }
                }
            }

            if (record_path.empty() || !nah::fs::exists(record_path))
            {
                print_error("App not installed: " + show_opts.target, opts.json);
                return 1;
            }

            // Load install record
            auto record_content = nah::fs::read_file(record_path);
            if (!record_content)
            {
                print_error("Failed to read install record", opts.json);
                return 1;
            }

            auto install_result = nah::json::parse_install_record(*record_content);
            if (!install_result.ok)
            {
                print_error("Invalid install record: " + install_result.error, opts.json);
                return 1;
            }

            // Load app manifest from install directory
            std::string app_dir = install_result.value.paths.install_root;
            auto manifest_content = nah::fs::read_file(app_dir + "/nap.json");

            if (!manifest_content)
            {
                print_error("App manifest (nap.json) not found in " + app_dir, opts.json);
                return 1;
            }

            auto app_result = nah::json::parse_app_declaration(*manifest_content);
            if (!app_result.ok)
            {
                print_error("Invalid app manifest: " + app_result.error, opts.json);
                return 1;
            }

            // Load host environment
            auto host_env = load_host_environment(nah_root);

            // Load inventory
            auto inventory = load_inventory(nah_root);

            // Compose the contract
            nah::core::CompositionOptions compose_opts;
            compose_opts.enable_trace = opts.trace || show_opts.trace_flag;
            auto result = nah::core::nah_compose(
                app_result.value,
                host_env,
                install_result.value,
                inventory,
                compose_opts);

            if (opts.json)
            {
                std::cout << nah::core::serialize_result(result) << std::endl;
            }
            else
            {
                // Human-readable output
                if (!result.ok)
                {
                    if (result.critical_error)
                    {
                        std::cout << "Critical Error: " << nah::core::critical_error_to_string(*result.critical_error) << std::endl;
                    }
                    std::cout << "  " << result.critical_error_context << std::endl;
                    return 1;
                }

                const auto &contract = result.contract;

                std::cout << "Application: " << contract.app.id << " v" << contract.app.version << std::endl;

                if (!contract.nak.id.empty())
                {
                    std::cout << "NAK: " << contract.nak.id << " v" << contract.nak.version << std::endl;
                }

                std::cout << "Binary: " << contract.execution.binary << std::endl;
                std::cout << "CWD: " << contract.execution.cwd << std::endl;

                if (!contract.execution.arguments.empty())
                {
                    std::cout << "Arguments:" << std::endl;
                    for (const auto &arg : contract.execution.arguments)
                    {
                        std::cout << "  " << arg << std::endl;
                    }
                }

                if (!contract.execution.library_paths.empty())
                {
                    std::cout << "\nLibrary Paths (" << contract.execution.library_path_env_key << "):" << std::endl;
                    for (const auto &p : contract.execution.library_paths)
                    {
                        std::cout << "  " << p << std::endl;
                    }
                }

                // Show NAH_* env vars
                std::cout << "\nEnvironment (NAH_*):" << std::endl;
                for (const auto &[key, value] : contract.environment)
                {
                    if (key.substr(0, 4) == "NAH_")
                    {
                        std::cout << "  " << key << "=" << value << std::endl;
                    }
                }

                // Show other env vars
                bool has_other = false;
                for (const auto &[key, value] : contract.environment)
                {
                    if (key.substr(0, 4) != "NAH_")
                    {
                        if (!has_other)
                        {
                            std::cout << "\nEnvironment (other):" << std::endl;
                            has_other = true;
                        }
                        std::string display_value = value.size() > 60 ? value.substr(0, 57) + "..." : value;
                        std::cout << "  " << key << "=" << display_value << std::endl;
                    }
                }

                // Show warnings
                if (!result.warnings.empty())
                {
                    std::cout << "\nWarnings:" << std::endl;
                    for (const auto &warning : result.warnings)
                    {
                        std::string prefix = (warning.action == "error") ? "[ERROR]" : "[WARN]";
                        std::cout << "  " << prefix << " " << warning.key << std::endl;
                    }
                }
                else
                {
                    std::cout << "\nWarnings: none" << std::endl;
                }

                // Show trace if requested
                if (compose_opts.enable_trace && result.trace)
                {
                    std::cout << "\nTrace:" << std::endl;
                    for (const auto &decision : result.trace->decisions)
                    {
                        std::cout << "  " << decision << std::endl;
                    }
                }

                if (!compose_opts.enable_trace)
                {
                    std::cout << "\nRun with --trace to see where each value comes from." << std::endl;
                }
            }

            return result.ok ? 0 : 1;
        }

    } // anonymous namespace

    void setup_show(CLI::App *app, GlobalOptions &opts)
    {
        static ShowOptions show_opts;

        app->add_option("target", show_opts.target, "App to inspect (id, id@version, or directory)");
        app->add_flag("--trace", show_opts.trace_flag, "Include provenance information");

        app->callback([&opts]()
                      { std::exit(cmd_show(opts, show_opts)); });
    }

} // namespace nah::cli::commands
