/** Safely remove an installed app or NAK. */

#include "../common.hpp"
#include <CLI/CLI.hpp>
#include <filesystem>

namespace nah::cli::commands {
namespace {
namespace fs = std::filesystem;

struct UninstallOptions {
    std::string target;
    bool as_app = false;
    bool as_nak = false;
    bool force = false;
};

int cmd_uninstall(const GlobalOptions& opts, const UninstallOptions& options) {
    init_warning_collector(opts.json, opts.quiet);
    const auto root = resolve_nah_root(opts.root.empty() ? std::nullopt : std::optional(opts.root));
    const auto paths = get_nah_paths(root);
    const auto target = parse_target(options.target);
    if (!is_valid_package_id(target.id) || (target.version && !is_valid_version(*target.version)) || (options.as_app && options.as_nak)) {
        print_error("invalid package target or conflicting type flags", opts.json); return 1;
    }

    auto app_record = find_record(paths.registry_apps, target);
    auto nak_record = find_record(paths.registry_naks, target);
    if (options.as_app) nak_record.reset();
    if (options.as_nak) app_record.reset();
    if (app_record && nak_record) { print_error("target exists as both app and NAK; use --app or --nak", opts.json); return 1; }
    const bool is_app = app_record.has_value();
    const auto record_path = is_app ? app_record : nak_record;
    if (!record_path) { print_error("package not found: " + options.target, opts.json); return 1; }

    const auto content = nah::fs::read_file(record_path->string());
    nlohmann::json record;
    try { record = nlohmann::json::parse(content.value_or("")); }
    catch (...) { print_error("registry record is invalid; refusing to remove files", opts.json); return 1; }
    const std::string version = is_app ? record["app"].value("version", "") : record["nak"].value("version", "");
    if (!is_valid_version(version)) { print_error("registry record contains an invalid version", opts.json); return 1; }

    if (!is_app && !options.force) {
        std::vector<std::string> references;
        for (const auto& app_path : nah::fs::list_directory(paths.registry_apps)) {
            const auto app_content = nah::fs::read_file(app_path);
            if (!app_content) continue;
            try {
                const auto app = nlohmann::json::parse(*app_content);
                if (app.contains("nak") && app["nak"].value("record_ref", "") == record_path->filename().string()) {
                    references.push_back(fs::path(app_path).stem().string());
                }
            } catch (...) {}
        }
        if (!references.empty()) {
            std::string list;
            for (const auto& reference : references) list += (list.empty() ? "" : ", ") + reference;
            print_error("NAK is used by: " + list + "; use --force to remove it anyway", opts.json); return 1;
        }
    }

    std::string stored_path;
    try { stored_path = record.at("paths").at(is_app ? "install_root" : "root").get<std::string>(); }
    catch (...) { print_error("registry record has no installation path", opts.json); return 1; }
    const auto installed_path = resolve_record_path(root, stored_path);
    const auto expected_base = is_app ? fs::path(paths.apps) : fs::path(paths.naks);
    if (!installed_path || !is_path_within(expected_base, *installed_path)) {
        print_error("registry path escapes its managed installation directory", opts.json); return 1;
    }

    const nah::store::Store store(root);
    const auto removed = store.remove(fs::path(*installed_path).lexically_relative(root),
                                      record_path->lexically_relative(root));
    if (!removed.ok) { print_error(removed.message, opts.json); return 1; }
    std::error_code ec;
    if (!is_app) {
        const auto parent = fs::path(*installed_path).parent_path();
        if (is_path_within(paths.naks, parent) && fs::is_empty(parent, ec)) fs::remove(parent, ec);
    }

    nlohmann::json result{{"ok", true}, {is_app ? "app" : "nak", {{"id", target.id}, {"version", version}}}};
    if (opts.json) output_json(result); else std::cout << "Uninstalled " << (is_app ? "" : "NAK ") << target.id << '@' << version << '\n';
    return 0;
}

} // namespace

void setup_uninstall(CLI::App* app, GlobalOptions& opts) {
    static UninstallOptions options;
    app->add_option("target", options.target, "Installed id or id@version")->required();
    app->add_flag("--app", options.as_app, "Require an app");
    app->add_flag("--nak", options.as_nak, "Require a NAK");
    app->add_flag("-f,--force", options.force, "Remove a referenced NAK");
    app->callback([&opts]() { std::exit(cmd_uninstall(opts, options)); });
}

} // namespace nah::cli::commands
