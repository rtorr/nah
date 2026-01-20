/**
 * NAH CLI - which command
 * 
 * Print installation paths for a package.
 */

#include "../common.hpp"
#include <CLI/CLI.hpp>

namespace nah::cli::commands {

namespace {

struct WhichOptions {
    std::string target;
};

int cmd_which(const GlobalOptions& opts, const WhichOptions& which_opts) {
    init_warning_collector(opts.json, opts.quiet);
    
    std::string nah_root = resolve_nah_root(
        opts.root.empty() ? std::nullopt : std::make_optional(opts.root));
    auto paths = get_nah_paths(nah_root);
    
    auto parsed = parse_target(which_opts.target);
    
    // Try to find as app first (files named: id@version.json)
    std::string record_path;
    bool is_app = false;
    
    if (parsed.version) {
        record_path = paths.registry_apps + "/" + parsed.id + "@" + *parsed.version + ".json";
        if (nah::fs::exists(record_path)) {
            is_app = true;
        }
    } else {
        auto files = nah::fs::list_directory(paths.registry_apps);
        for (const auto& filepath : files) {
            std::string f = filepath;
            size_t last_slash = filepath.rfind('/');
            if (last_slash != std::string::npos) {
                f = filepath.substr(last_slash + 1);
            }
            if (f.find(parsed.id + "@") == 0 && f.size() > 5 && f.substr(f.size() - 5) == ".json") {
                record_path = filepath;
                is_app = true;
                break;
            }
        }
    }
    
    // Try NAK if not found as app
    if (!is_app) {
        if (parsed.version) {
            record_path = paths.registry_naks + "/" + parsed.id + "@" + *parsed.version + ".json";
            if (!nah::fs::exists(record_path)) {
                record_path = "";
            }
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
                    break;
                }
            }
        }
    }
    
    if (record_path.empty() || !nah::fs::exists(record_path)) {
        print_error("Package not found: " + which_opts.target, opts.json);
        return 1;
    }
    
    auto content = nah::fs::read_file(record_path);
    if (!content) {
        print_error("Failed to read record", opts.json);
        return 1;
    }
    
    try {
        auto record = nlohmann::json::parse(*content);
        
        if (opts.json) {
            nlohmann::json j;
            j["record"] = record_path;
            if (is_app) {
                j["type"] = "app";
                j["id"] = record.value("id", "");
                j["version"] = record.value("version", "");
                j["install_root"] = record["paths"]["install_root"];
            } else {
                j["type"] = "nak";
                j["id"] = record["nak"]["id"];
                j["version"] = record["nak"]["version"];
                j["root"] = record["paths"]["root"];
                if (record["paths"].contains("lib_dirs")) {
                    j["lib_dirs"] = record["paths"]["lib_dirs"];
                }
            }
            output_json(j);
        } else {
            if (is_app) {
                std::cout << "App: " << record.value("id", "") << "@" << record.value("version", "") << std::endl;
                std::cout << "Record: " << record_path << std::endl;
                std::cout << "Install root: " << record["paths"]["install_root"].get<std::string>() << std::endl;
            } else {
                std::cout << "NAK: " << record["nak"]["id"].get<std::string>() << "@" 
                          << record["nak"]["version"].get<std::string>() << std::endl;
                std::cout << "Record: " << record_path << std::endl;
                std::cout << "Root: " << record["paths"]["root"].get<std::string>() << std::endl;
                if (record["paths"].contains("lib_dirs")) {
                    std::cout << "Library dirs:" << std::endl;
                    for (const auto& dir : record["paths"]["lib_dirs"]) {
                        std::cout << "  " << dir.get<std::string>() << std::endl;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        print_error("Failed to parse record: " + std::string(e.what()), opts.json);
        return 1;
    }
    
    return 0;
}

} // anonymous namespace

void setup_which(CLI::App* app, GlobalOptions& opts) {
    static WhichOptions which_opts;
    
    app->add_option("target", which_opts.target, "Package to find (id or id@version)")->required();
    
    app->callback([&opts]() {
        std::exit(cmd_which(opts, which_opts));
    });
}

} // namespace nah::cli::commands
