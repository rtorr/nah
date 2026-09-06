/**
 * NAH CLI - pack command
 *
 * Create a .nap or .nak package from a directory.
 */

#include "../common.hpp"
#include <nah/nah_archive.h>
#include <CLI/CLI.hpp>

namespace nah::cli::commands {

namespace {

struct PackOptions {
    std::string dir;
    std::string output;
};

int cmd_pack(const GlobalOptions& opts, const PackOptions& pack_opts) {
    init_warning_collector(opts.json, opts.quiet);

    std::string source_dir = pack_opts.dir;

    // Detect manifest type by filename (names match package extensions)
    std::string manifest_path;
    std::string manifest_type;

    const bool has_app = nah::fs::exists(source_dir + "/nap.json");
    const bool has_nak = nah::fs::exists(source_dir + "/nak.json");
    if (has_app == has_nak) {
        print_error("Directory must contain exactly one of nap.json or nak.json", opts.json);
        return 1;
    }
    if (has_app) {
        manifest_path = source_dir + "/nap.json";
        manifest_type = "nap";
    } else if (has_nak) {
        manifest_path = source_dir + "/nak.json";
        manifest_type = "nak";
    }

    // Read and parse manifest
    auto manifest_content = nah::fs::read_file(manifest_path);
    if (!manifest_content) {
        print_error("Failed to read manifest: " + manifest_path, opts.json);
        return 1;
    }

    nlohmann::json manifest;
    try {
        manifest = nlohmann::json::parse(*manifest_content);
    } catch (const std::exception& e) {
        print_error("Invalid manifest JSON: " + std::string(e.what()), opts.json);
        return 1;
    }

    // Extract identity based on type
    std::string id, version;
    if (manifest_type == "nap") {
        const auto app = nah::json::parse_app_declaration(*manifest_content);
        if (!app.ok) {
            print_error("Invalid app manifest: " + app.error, opts.json);
            return 1;
        }
        id = app.value.id;
        version = app.value.version;
        const auto entrypoint = std::filesystem::path(source_dir) / app.value.entrypoint_path;
        if (std::filesystem::path(app.value.entrypoint_path).is_absolute() ||
            !is_path_within(source_dir, entrypoint) || !std::filesystem::is_regular_file(entrypoint)) {
            print_error("App entrypoint must be a packaged regular file", opts.json);
            return 1;
        }
    } else { // nak
        if (!manifest.contains("nak") || !manifest["nak"].contains("identity")) {
            print_error("Invalid NAK manifest: missing nak.identity section", opts.json);
            return 1;
        }
        id = manifest["nak"]["identity"].value("id", "");
        version = manifest["nak"]["identity"].value("version", "");
    }

    if (!is_valid_package_id(id) || !is_valid_version(version)) {
        print_error("Manifest id or version is invalid", opts.json);
        return 1;
    }

    std::string ext = "." + manifest_type; // .nap or .nak

    // Determine output path
    std::string output_path = pack_opts.output;
    if (output_path.empty()) {
        output_path = id + "-" + version + ext;
    }

    const auto result = nah::archive::create(source_dir, output_path);
    if (result.ok) {
        if (opts.json) {
            nlohmann::json j;
            j["ok"] = true;
            j["type"] = manifest_type;
            j["id"] = id;
            j["version"] = version;
            j["package"] = output_path;
            output_json(j);
        } else {
            std::cout << "Created " << manifest_type << " package: " << output_path << std::endl;
            std::cout << "  ID: " << id << std::endl;
            std::cout << "  Version: " << version << std::endl;
        }
        return 0;
    } else {
        print_error("Failed to create package: " + result.error, opts.json);
        return 1;
    }
}

} // anonymous namespace

void setup_pack(CLI::App* app, GlobalOptions& opts) {
    static PackOptions pack_opts;

    app->add_option("dir", pack_opts.dir, "Directory to pack")->required();
    app->add_option("-o,--output", pack_opts.output, "Output file path");

    app->callback([&opts]() {
        std::exit(cmd_pack(opts, pack_opts));
    });
}

} // namespace nah::cli::commands
