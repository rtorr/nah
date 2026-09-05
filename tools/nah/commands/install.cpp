/** Install a local app or NAK package into an isolated NAH root. */

#include "../common.hpp"
#include "../package_archive.hpp"
#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>
#include <random>

namespace nah::cli::commands {
namespace {
namespace fs = std::filesystem;

struct InstallOptions {
    std::string source;
    bool force = false;
    bool as_app = false;
    bool as_nak = false;
    bool dry_run = false;
    std::string loader;
};

std::string unique_id() {
    static std::random_device random;
    static std::mt19937_64 generator(random());
    std::uniform_int_distribution<std::uint64_t> distribution;
    char value[33];
    std::snprintf(value, sizeof(value), "%016llx%016llx",
                  static_cast<unsigned long long>(distribution(generator)),
                  static_cast<unsigned long long>(distribution(generator)));
    return value;
}

bool safe_relative(const std::string& value) {
    if (value.empty() || value.find('\\') != std::string::npos) return false;
    fs::path path(value);
    if (path.is_absolute() || path.has_root_name()) return false;
    path = path.lexically_normal();
    if (path.empty() || path == ".") return false;
    for (const auto& part : path) if (part == "..") return false;
    return true;
}

std::optional<std::string> validate_source_tree(const fs::path& source) {
    std::error_code ec;
    if (!fs::is_directory(source, ec)) return "source is not a directory";
    for (fs::recursive_directory_iterator it(source, fs::directory_options::none, ec), end;
         !ec && it != end; it.increment(ec)) {
        const auto status = it->symlink_status(ec);
        if (ec) break;
        if (fs::is_symlink(status)) return "symbolic links are not supported: " + it->path().string();
        if (!fs::is_regular_file(status) && !fs::is_directory(status)) return "unsupported file type: " + it->path().string();
    }
    if (ec) return "cannot inspect source: " + ec.message();
    return std::nullopt;
}

bool write_json(const fs::path& path, const nlohmann::json& value, std::string& error) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) { error = "cannot write registry record"; return false; }
    output << value.dump(2) << '\n';
    if (!output) { error = "cannot finish registry record"; return false; }
    return true;
}

bool commit_install(const fs::path& source, const fs::path& final_path,
                    const fs::path& record_path, const nlohmann::json& record,
                    bool force, const NahPaths& paths, std::string& error) {
    if (!is_path_within(paths.root, final_path) || !is_path_within(paths.registry, record_path)) {
        error = "refusing to install outside the NAH root";
        return false;
    }
    std::error_code ec;
    if ((fs::exists(final_path, ec) || fs::exists(record_path, ec)) && !force) {
        error = "package is already installed; use --force to replace it";
        return false;
    }
    const fs::path stage = fs::path(paths.staging) / unique_id();
    const fs::path record_stage = fs::path(paths.staging) / (unique_id() + ".json");
    const fs::path backup = fs::path(paths.staging) / (unique_id() + ".backup");
    fs::create_directories(stage.parent_path(), ec);
    fs::copy(source, stage, fs::copy_options::recursive, ec);
    if (ec) { fs::remove_all(stage, ec); error = "cannot stage package: " + ec.message(); return false; }
    if (!write_json(record_stage, record, error)) { fs::remove_all(stage, ec); fs::remove(record_stage, ec); return false; }

    const bool had_existing = fs::exists(final_path, ec);
    if (had_existing) {
        fs::rename(final_path, backup, ec);
        if (ec) { fs::remove_all(stage, ec); fs::remove(record_stage, ec); error = "cannot stage existing installation: " + ec.message(); return false; }
    }
    fs::create_directories(final_path.parent_path(), ec);
    fs::rename(stage, final_path, ec);
    if (ec) {
        if (had_existing) fs::rename(backup, final_path, ec);
        fs::remove_all(stage, ec); fs::remove(record_stage, ec);
        error = "cannot activate installation";
        return false;
    }
    fs::create_directories(record_path.parent_path(), ec);
    fs::rename(record_stage, record_path, ec);
    if (ec) {
        fs::remove_all(final_path, ec);
        if (had_existing) fs::rename(backup, final_path, ec);
        fs::remove(record_stage, ec);
        error = "cannot activate registry record";
        return false;
    }
    fs::remove_all(backup, ec);
    return true;
}

std::unordered_map<std::string, nah::core::RuntimeDescriptor> load_runtime_inventory(const NahPaths& paths) {
    std::unordered_map<std::string, nah::core::RuntimeDescriptor> inventory;
    for (const auto& path : nah::fs::list_directory(paths.registry_naks)) {
        if (fs::path(path).extension() != ".json") continue;
        const auto content = nah::fs::read_file(path);
        if (!content) continue;
        auto parsed = nah::json::parse_runtime_descriptor(*content, path);
        if (parsed.ok) inventory[fs::path(path).filename().string()] = std::move(parsed.value);
    }
    return inventory;
}

int install_directory(const GlobalOptions& opts, const InstallOptions& options,
                      const fs::path& source, const std::string& nah_root) {
    if (const auto error = validate_source_tree(source)) { print_error(*error, opts.json); return 1; }
    const bool has_app = fs::is_regular_file(source / "nap.json");
    const bool has_nak = fs::is_regular_file(source / "nak.json");
    if (has_app == has_nak) { print_error("source must contain exactly one of nap.json or nak.json", opts.json); return 1; }
    if ((options.as_app && !has_app) || (options.as_nak && !has_nak) || (options.as_app && options.as_nak)) {
        print_error("package type conflicts with --app/--nak", opts.json); return 1;
    }

    const auto manifest_text = nah::fs::read_file((source / (has_app ? "nap.json" : "nak.json")).string());
    if (!manifest_text) { print_error("cannot read package manifest", opts.json); return 1; }
    nlohmann::json manifest;
    try { manifest = nlohmann::json::parse(*manifest_text); }
    catch (const std::exception& error) { print_error("invalid manifest JSON: " + std::string(error.what()), opts.json); return 1; }

    std::string id;
    std::string version;
    if (has_app) {
        const auto parsed = nah::json::parse_app_declaration(*manifest_text);
        if (!parsed.ok) { print_error("invalid app manifest: " + parsed.error, opts.json); return 1; }
        id = parsed.value.id;
        version = parsed.value.version;
        if (!safe_relative(parsed.value.entrypoint_path) || !fs::is_regular_file(source / parsed.value.entrypoint_path)) {
            print_error("app entrypoint must be a packaged regular file", opts.json); return 1;
        }
        for (const auto& component : parsed.value.components) {
            if (!is_valid_package_id(component.id) || !safe_relative(component.entrypoint) || !fs::is_regular_file(source / component.entrypoint)) {
                print_error("component id or entrypoint is invalid: " + component.id, opts.json); return 1;
            }
        }
    } else {
        try {
            const auto& identity = manifest.at("nak").at("identity");
            id = identity.at("id").get<std::string>();
            version = identity.at("version").get<std::string>();
        } catch (...) { print_error("invalid NAK manifest: missing nak.identity", opts.json); return 1; }
    }
    if (!is_valid_package_id(id) || !is_valid_version(version)) { print_error("manifest id or semantic version is invalid", opts.json); return 1; }

    if (options.dry_run) {
        nlohmann::json result{{"would_install", true}, {"type", has_app ? "app" : "nak"}, {"id", id}, {"version", version}};
        if (opts.json) output_json(result); else std::cout << "Would install " << id << '@' << version << '\n';
        return 0;
    }

    ensure_nah_structure(nah_root);
    const auto paths = get_nah_paths(nah_root);
    const auto installed_at = nah::core::get_current_timestamp();
    nlohmann::json record;
    fs::path final_path;
    fs::path record_path;

    if (has_nak) {
        final_path = fs::path(paths.naks) / id / version;
        record_path = fs::path(paths.registry_naks) / (id + "@" + version + ".json");
        record["nak"] = {{"id", id}, {"version", version}};
        record["paths"]["root"] = (fs::path("naks") / id / version).generic_string();
        const auto& nak = manifest["nak"];
        if (nak.contains("paths")) {
            for (const auto* key : {"resource_root", "lib_dirs"}) if (nak["paths"].contains(key)) record["paths"][key] = nak["paths"][key];
        }
        if (record["paths"].contains("resource_root") &&
            (!record["paths"]["resource_root"].is_string() ||
             !safe_relative(record["paths"]["resource_root"].get<std::string>()) ||
             !fs::is_directory(source / record["paths"]["resource_root"].get<std::string>()))) {
            print_error("NAK resource_root must be a relative packaged path", opts.json); return 1;
        }
        if (record["paths"].contains("lib_dirs")) {
            if (!record["paths"]["lib_dirs"].is_array()) { print_error("NAK lib_dirs must be an array", opts.json); return 1; }
            for (const auto& directory : record["paths"]["lib_dirs"]) {
                if (!directory.is_string() || !safe_relative(directory.get<std::string>()) || !fs::is_directory(source / directory.get<std::string>())) {
                    print_error("NAK lib_dirs must reference packaged directories", opts.json); return 1;
                }
            }
        }
        for (const auto* key : {"environment", "loaders", "execution"}) if (nak.contains(key)) record[key] = nak[key];
        if (record.contains("loaders")) {
            if (!record["loaders"].is_object()) { print_error("NAK loaders must be an object", opts.json); return 1; }
            for (const auto& [name, loader] : record["loaders"].items()) {
                if (!loader.is_object()) { print_error("NAK loader is invalid: " + name, opts.json); return 1; }
                const auto executable = loader.value("exec_path", "");
                if (!safe_relative(executable) || !fs::is_regular_file(source / executable)) {
                    print_error("NAK loader must reference a packaged executable: " + name, opts.json); return 1;
                }
            }
        }
        record["provenance"] = {{"installed_at", installed_at}, {"installed_by", "nah_cli"}, {"source", source.string()}};
    } else {
        const auto app = nah::json::parse_app_declaration(*manifest_text).value;
        final_path = fs::path(paths.apps) / (id + "-" + version);
        record_path = fs::path(paths.registry_apps) / (id + "@" + version + ".json");
        record["install"]["instance_id"] = unique_id();
        record["app"] = {{"id", id}, {"version", version}};
        if (!app.nak_id.empty()) {
            if (!is_valid_package_id(app.nak_id)) { print_error("invalid NAK id in app manifest", opts.json); return 1; }
            const std::string requirement = app.nak_version_req.empty() ? "*" : app.nak_version_req;
            const auto selection = nah::semver::select_nak_from_inventory(load_runtime_inventory(paths), app.nak_id, requirement);
            record["app"]["nak_id"] = app.nak_id;
            record["app"]["nak_version_req"] = requirement;
            if (selection.found) {
                const bool explicit_loader = !options.loader.empty() || !app.nak_loader.empty();
                const auto loader = !options.loader.empty() ? options.loader : (!app.nak_loader.empty() ? app.nak_loader : "default");
                const auto runtime_text = nah::fs::read_file((fs::path(paths.registry_naks) / selection.record_ref).string());
                auto runtime = runtime_text ? nah::json::parse_runtime_descriptor(*runtime_text) : nah::json::ParseResult<nah::core::RuntimeDescriptor>{};
                if (!runtime.ok || (runtime.value.has_loaders()
                        ? runtime.value.loaders.find(loader) == runtime.value.loaders.end()
                        : explicit_loader)) {
                    print_error("loader not found in selected NAK: " + loader, opts.json); return 1;
                }
                record["nak"] = {{"id", app.nak_id}, {"version", selection.nak_version}, {"record_ref", selection.record_ref},
                                   {"loader", loader}, {"selection_reason", selection.selection_reason}};
            } else {
                print_warning(selection.error + "; install a matching NAK before running the app", opts.json);
            }
        }
        if (!app.components.empty()) {
            record["components"] = nlohmann::json::array();
            for (const auto& component : app.components) {
                record["components"].push_back({{"id", component.id}, {"name", component.name}, {"entrypoint", component.entrypoint},
                                                {"uri_pattern", component.uri_pattern}, {"loader", component.loader},
                                                {"standalone", component.standalone}, {"hidden", component.hidden}});
            }
        }
        record["paths"]["install_root"] = (fs::path("apps") / (id + "-" + version)).generic_string();
        record["trust"] = {{"state", "unknown"}, {"source", "local_install"}, {"evaluated_at", installed_at}};
        record["provenance"] = {{"package_hash", ""}, {"installed_at", installed_at}, {"installed_by", "nah_cli"}, {"source", source.string()}};
    }

    std::string error;
    if (!commit_install(source, final_path, record_path, record, options.force, paths, error)) { print_error(error, opts.json); return 1; }
    nlohmann::json result{{"ok", true}, {has_app ? "app" : "nak", {{"id", id}, {"version", version}}},
                          {"paths", {{has_app ? "install_root" : "root", final_path.string()}}}};
    if (opts.json) output_json(result); else std::cout << "Installed " << id << '@' << version << '\n';
    return 0;
}

int cmd_install(const GlobalOptions& opts, const InstallOptions& options) {
    init_warning_collector(opts.json, opts.quiet);
    if (options.source.compare(0, 7, "http://") == 0 || options.source.compare(0, 8, "https://") == 0) {
        print_error("remote installation is outside this CLI's scope; download and verify the package first", opts.json); return 1;
    }
    const fs::path source = fs::absolute(options.source).lexically_normal();
    const auto root = resolve_nah_root(opts.root.empty() ? std::nullopt : std::optional(opts.root));
    if (fs::is_directory(source)) return install_directory(opts, options, source, root);
    if (!fs::is_regular_file(source) || (source.extension() != ".nap" && source.extension() != ".nak")) {
        print_error("source must be a directory, .nap, or .nak file", opts.json); return 1;
    }
    const fs::path temporary = fs::temp_directory_path() / ("nah-install-" + unique_id());
    const auto extracted = package_archive::extract(source, temporary);
    if (!extracted.ok) { print_error("cannot extract package: " + extracted.error, opts.json); return 1; }
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{temporary};
    if ((source.extension() == ".nap" && options.as_nak) || (source.extension() == ".nak" && options.as_app)) {
        print_error("package extension conflicts with --app/--nak", opts.json); return 1;
    }
    if ((source.extension() == ".nap" && !fs::is_regular_file(temporary / "nap.json")) ||
        (source.extension() == ".nak" && !fs::is_regular_file(temporary / "nak.json"))) {
        print_error("package extension does not match its manifest", opts.json); return 1;
    }
    return install_directory(opts, options, temporary, root);
}

} // namespace

void setup_install(CLI::App* app, GlobalOptions& opts) {
    static InstallOptions options;
    app->add_option("source", options.source, "Local directory, .nap file, or .nak file")->required();
    app->add_flag("-f,--force", options.force, "Replace an existing installation");
    app->add_flag("--app", options.as_app, "Require an app package");
    app->add_flag("--nak", options.as_nak, "Require a NAK package");
    app->add_flag("--dry-run", options.dry_run, "Validate without installing");
    app->add_option("--loader", options.loader, "NAK loader for this app");
    app->callback([&opts]() { std::exit(cmd_install(opts, options)); });
}

} // namespace nah::cli::commands
