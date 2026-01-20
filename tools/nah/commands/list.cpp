/**
 * NAH CLI - list command (refactored to use NahHost)
 *
 * List installed apps and NAKs.
 */

// Enable host implementation in this translation unit
#define NAH_HOST_IMPLEMENTATION

#include "../common.hpp"
#include <nah/nah_host.h>
#include <CLI/CLI.hpp>
#include <algorithm>

namespace nah::cli::commands {

namespace {

struct ListOptions {
    bool apps = false;
    bool naks = false;
};

// Helper to list NAKs (NahHost doesn't provide this yet)
nlohmann::json list_naks(const std::string& registry_naks_path, const GlobalOptions& opts) {
    nlohmann::json naks = nlohmann::json::array();

    auto nak_files = nah::fs::list_directory(registry_naks_path);
    for (const auto& filepath : nak_files) {
        // Extract just the filename from the full path
        std::string file = filepath;
        size_t last_slash = filepath.rfind('/');
        if (last_slash != std::string::npos) {
            file = filepath.substr(last_slash + 1);
        }

        if (file.size() > 5 && file.substr(file.size() - 5) == ".json") {
            auto content = nah::fs::read_file(filepath);
            if (content) {
                try {
                    auto record = nlohmann::json::parse(*content);
                    nlohmann::json nak_info;
                    if (record.contains("nak")) {
                        nak_info["id"] = record["nak"].value("id", "unknown");
                        nak_info["version"] = record["nak"].value("version", "unknown");
                    } else {
                        nak_info["id"] = record.value("id", "unknown");
                        nak_info["version"] = record.value("version", "unknown");
                    }
                    naks.push_back(nak_info);
                } catch (const std::exception& e) {
                    print_verbose_warning("Skipping invalid NAK record " + file + ": " + e.what(),
                                          opts.json, opts.verbose);
                }
            }
        }
    }

    return naks;
}

int cmd_list(const GlobalOptions& opts, const ListOptions& list_opts) {
    init_warning_collector(opts.json, opts.quiet);

    std::string nah_root = resolve_nah_root(
        opts.root.empty() ? std::nullopt : std::make_optional(opts.root));

    // Create NahHost instance
    auto host = nah::host::NahHost::create(nah_root);
    if (!host) {
        print_error("Failed to initialize NAH host", opts.json);
        return 1;
    }

    // If neither flag set, show both
    bool show_apps = list_opts.apps || (!list_opts.apps && !list_opts.naks);
    bool show_naks = list_opts.naks || (!list_opts.apps && !list_opts.naks);

    nlohmann::json result;
    result["apps"] = nlohmann::json::array();
    result["naks"] = nlohmann::json::array();

    // List apps using NahHost
    if (show_apps) {
        auto apps = host->listApplications();
        for (const auto& app : apps) {
            nlohmann::json app_info;
            app_info["id"] = app.id;
            app_info["version"] = app.version;

            // Read the full record to get NAK info if needed
            auto content = nah::fs::read_file(app.record_path);
            if (content) {
                try {
                    auto record = nlohmann::json::parse(*content);
                    if (record.contains("app") && record["app"].contains("nak_id")) {
                        app_info["nak_id"] = record["app"]["nak_id"];
                    }
                } catch (...) {
                    // Ignore parse errors for NAK info
                }
            }

            result["apps"].push_back(app_info);
        }
    }

    // List NAKs (NahHost doesn't have this method yet, so we keep the original logic)
    if (show_naks) {
        auto paths = get_nah_paths(nah_root);
        result["naks"] = list_naks(paths.registry_naks, opts);
    }

    if (opts.json) {
        output_json(result);
    } else {
        // Human-readable output
        if (show_apps) {
            auto& apps = result["apps"];
            if (apps.empty()) {
                std::cout << "No apps installed." << std::endl;
            } else {
                std::cout << "Apps:" << std::endl;
                for (const auto& app : apps) {
                    std::cout << "  " << app["id"].get<std::string>()
                              << "@" << app["version"].get<std::string>();
                    if (app.contains("nak_id")) {
                        std::cout << " (nak: " << app["nak_id"].get<std::string>() << ")";
                    }
                    std::cout << std::endl;
                }
            }
        }

        if (show_naks) {
            if (show_apps) std::cout << std::endl;
            auto& naks = result["naks"];
            if (naks.empty()) {
                std::cout << "No NAKs installed." << std::endl;
            } else {
                std::cout << "NAKs:" << std::endl;
                for (const auto& nak : naks) {
                    std::cout << "  " << nak["id"].get<std::string>()
                              << "@" << nak["version"].get<std::string>() << std::endl;
                }
            }
        }
    }

    return 0;
}

} // anonymous namespace

void setup_list(CLI::App* app, GlobalOptions& opts) {
    static ListOptions list_opts;

    app->add_flag("--apps", list_opts.apps, "List only apps");
    app->add_flag("--naks", list_opts.naks, "List only NAKs");

    app->callback([&opts]() {
        std::exit(cmd_list(opts, list_opts));
    });
}

} // namespace nah::cli::commands