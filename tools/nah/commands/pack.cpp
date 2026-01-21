/**
 * NAH CLI - pack command
 * 
 * Create a .nap or .nak package from a directory.
 */

#include "../common.hpp"
#include <CLI/CLI.hpp>
#include <fstream>

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
    
    if (nah::fs::exists(source_dir + "/nap.json")) {
        manifest_path = source_dir + "/nap.json";
        manifest_type = "nap";
    } else if (nah::fs::exists(source_dir + "/nak.json")) {
        manifest_path = source_dir + "/nak.json";
        manifest_type = "nak";
    } else {
        print_error("No manifest found (expected nap.json or nak.json)", opts.json);
        return 1;
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
        if (!manifest.contains("app") || !manifest["app"].contains("identity")) {
            print_error("Invalid app manifest: missing app.identity section", opts.json);
            return 1;
        }
        id = manifest["app"]["identity"].value("id", "");
        version = manifest["app"]["identity"].value("version", "");
    } else { // nak
        if (!manifest.contains("nak") || !manifest["nak"].contains("identity")) {
            print_error("Invalid NAK manifest: missing nak.identity section", opts.json);
            return 1;
        }
        id = manifest["nak"]["identity"].value("id", "");
        version = manifest["nak"]["identity"].value("version", "");
    }
    
    if (id.empty() || version.empty()) {
        print_error("Manifest must contain id and version", opts.json);
        return 1;
    }
    
    std::string ext = "." + manifest_type; // .nap or .nak
    
    // Determine output path
    std::string output_path = pack_opts.output;
    if (output_path.empty()) {
        output_path = id + "-" + version + ext;
    }
    
    // Create tar.gz package using system tar command
    // Use deterministic flags for reproducible builds
    std::string tar_cmd = "tar --sort=name --owner=0 --group=0 --numeric-owner "
                          "--mtime='1970-01-01' -czf " + output_path + 
                          " -C " + source_dir + " .";
    
    int result = std::system(tar_cmd.c_str());
    
    if (result == 0) {
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
        print_error("Failed to create package (tar command failed)", opts.json);
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
