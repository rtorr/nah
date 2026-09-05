/**
 * NAH CLI - init command
 *
 * Scaffold a new NAH project.
 */

#include "../common.hpp"
#include <CLI/CLI.hpp>
#include <fstream>
#include <filesystem>

namespace nah::cli::commands
{

    namespace
    {

        struct InitOptions
        {
            bool as_app = false;
            bool as_nak = false;
            bool as_host = false;
            std::string id;
            std::string name;
            std::string dir = ".";
        };

        int cmd_init(const GlobalOptions &opts, const InitOptions &init_opts)
        {
            init_warning_collector(opts.json, opts.quiet);

            std::string target_dir = init_opts.dir;

            const int selected_types = static_cast<int>(init_opts.as_app) +
                                       static_cast<int>(init_opts.as_nak) +
                                       static_cast<int>(init_opts.as_host);
            if (selected_types > 1)
            {
                print_error("Choose only one of --app, --nak, or --host", opts.json);
                return 1;
            }

            // Determine type
            std::string type = "app"; // Default
            if (init_opts.as_nak)
                type = "nak";
            if (init_opts.as_host)
                type = "host";

            // Derive ID from directory name if not provided
            std::string id = init_opts.id;
            if (id.empty())
            {
                std::filesystem::path dir_path = std::filesystem::absolute(target_dir);
                std::string dirname = dir_path.filename().string();
                if (dirname.empty() || dirname == ".")
                {
                    dirname = dir_path.parent_path().filename().string();
                }
                id = "com.example." + dirname;
            }
            if (type != "host" && !is_valid_package_id(id))
            {
                print_error("Package id may contain only letters, digits, '.', '_', and '-'", opts.json);
                return 1;
            }

            // Determine manifest filename based on type
            std::string manifest_filename;
            if (type == "app")
            {
                manifest_filename = "nap.json"; // Native Application Package
            }
            else if (type == "nak")
            {
                manifest_filename = "nak.json"; // Native Application Kit
            }
            else
            {
                manifest_filename = "nah.json"; // Host configuration
            }

            std::string manifest_path = target_dir + "/" + manifest_filename;

            if (nah::fs::exists(manifest_path))
            {
                print_error(manifest_filename + " already exists in " + target_dir, opts.json);
                return 1;
            }

            // Create directory if needed
            std::filesystem::create_directories(target_dir);

            nlohmann::json manifest;

            if (type == "app")
            {
                manifest["$schema"] = "https://nah.rtorr.com/schemas/nap.v1.json";
                manifest["app"]["identity"]["id"] = id;
                manifest["app"]["identity"]["version"] = "0.1.0";
                manifest["app"]["execution"]["entrypoint"] = "bin/app";
                if (!init_opts.name.empty())
                {
                    manifest["app"]["metadata"]["name"] = init_opts.name;
                }

                // Create bin directory with placeholder
                std::filesystem::create_directories(target_dir + "/bin");
                std::ofstream app_file(target_dir + "/bin/app");
                app_file << "#!/bin/bash\necho \"Hello from " << id << "\"\n";
                app_file.close();
                std::filesystem::permissions(target_dir + "/bin/app",
                                             std::filesystem::perms::owner_exec | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                             std::filesystem::perm_options::add);
            }
            else if (type == "nak")
            {
                manifest["$schema"] = "https://nah.rtorr.com/schemas/nak.v1.json";
                manifest["nak"]["identity"]["id"] = id;
                manifest["nak"]["identity"]["version"] = "0.1.0";
                manifest["nak"]["paths"]["lib_dirs"] = nlohmann::json::array({"lib"});
                if (!init_opts.name.empty())
                {
                    manifest["nak"]["metadata"]["name"] = init_opts.name;
                }

                // Create lib directory
                std::filesystem::create_directories(target_dir + "/lib");
            }
            else if (type == "host")
            {
                manifest["$schema"] = "https://nah.rtorr.com/schemas/nah.v1.json";
                manifest["host"]["root"] = "./nah_root";
                manifest["host"]["environment"] = nlohmann::json::object();
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

            if (opts.json)
            {
                nlohmann::json j;
                j["ok"] = true;
                j["type"] = type;
                j["id"] = id;
                j["path"] = manifest_path;
                output_json(j);
            }
            else
            {
                std::cout << "Created " << manifest_filename << " for " << type << ": " << id << std::endl;
                std::cout << std::endl;
                std::cout << "Next steps:" << std::endl;
                if (type == "app")
                {
                    std::cout << "  nah pack .             # Create a .nap package" << std::endl;
                    std::cout << "  nah install .          # Install the app directory" << std::endl;
                }
                else if (type == "nak")
                {
                    std::cout << "  nah pack .             # Create a .nak package" << std::endl;
                    std::cout << "  nah install .          # Install the NAK directory" << std::endl;
                }
                else
                {
                    std::cout << "  Copy host/host.json into an existing NAH root." << std::endl;
                }
            }

            return 0;
        }

    } // anonymous namespace

    void setup_init(CLI::App *app, GlobalOptions &opts)
    {
        static InitOptions init_opts;

        app->add_flag("--app", init_opts.as_app, "Create an app project");
        app->add_flag("--nak", init_opts.as_nak, "Create a NAK project");
        app->add_flag("--host", init_opts.as_host, "Create a host setup directory");
        app->add_option("--id", init_opts.id, "Package identifier");
        app->add_option("--name", init_opts.name, "Human-readable name");
        app->add_option("dir", init_opts.dir, "Target directory (default: current)");

        app->callback([&opts]()
                      { std::exit(cmd_init(opts, init_opts)); });
    }

} // namespace nah::cli::commands
