/** Print the resolved path for an installed package. */

#include "../common.hpp"
#include <CLI/CLI.hpp>
#include <filesystem>

namespace nah::cli::commands {
namespace {

struct WhichOptions { std::string target; };

int cmd_which(const GlobalOptions& opts, const WhichOptions& options) {
    init_warning_collector(opts.json, opts.quiet);
    const auto root = resolve_nah_root(opts.root.empty() ? std::nullopt : std::optional(opts.root));
    const auto paths = get_nah_paths(root);
    const auto target = parse_target(options.target);
    if (!is_valid_package_id(target.id) || (target.version && !is_valid_version(*target.version))) {
        print_error("invalid package target", opts.json); return 1;
    }
    auto record_path = find_record(paths.registry_apps, target);
    const bool is_app = record_path.has_value();
    if (!record_path) record_path = find_record(paths.registry_naks, target);
    if (!record_path) { print_error("package not found: " + options.target, opts.json); return 1; }

    try {
        const auto record = nlohmann::json::parse(nah::fs::read_file(record_path->string()).value_or(""));
        const auto& identity = record.at(is_app ? "app" : "nak");
        const auto stored = record.at("paths").at(is_app ? "install_root" : "root").get<std::string>();
        const auto resolved = resolve_record_path(root, stored);
        if (!resolved) { print_error("registry path escapes the NAH root", opts.json); return 1; }
        if (opts.json) {
            output_json({{"type", is_app ? "app" : "nak"}, {"id", identity.at("id")}, {"version", identity.at("version")},
                         {"record", record_path->string()}, {is_app ? "install_root" : "root", *resolved}});
        } else {
            std::cout << (is_app ? "App: " : "NAK: ") << identity.at("id").get<std::string>() << '@'
                      << identity.at("version").get<std::string>() << '\n'
                      << "Record: " << record_path->string() << '\n'
                      << (is_app ? "Install root: " : "Root: ") << *resolved << '\n';
        }
    } catch (const std::exception& error) {
        print_error("invalid registry record: " + std::string(error.what()), opts.json); return 1;
    }
    return 0;
}

} // namespace

void setup_which(CLI::App* app, GlobalOptions& opts) {
    static WhichOptions options;
    app->add_option("target", options.target, "Installed id or id@version")->required();
    app->callback([&opts]() { std::exit(cmd_which(opts, options)); });
}

} // namespace nah::cli::commands
