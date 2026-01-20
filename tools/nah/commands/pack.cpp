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
    
    // Read manifest
    auto manifest_content = nah::fs::read_file(source_dir + "/nah.json");
    if (!manifest_content) {
        print_error("Manifest not found: " + source_dir + "/nah.json", opts.json);
        return 1;
    }
    
    nlohmann::json manifest;
    try {
        manifest = nlohmann::json::parse(*manifest_content);
    } catch (const std::exception& e) {
        print_error("Invalid manifest JSON: " + std::string(e.what()), opts.json);
        return 1;
    }
    
    std::string id = manifest.value("id", "");
    std::string version = manifest.value("version", "");
    
    if (id.empty() || version.empty()) {
        print_error("Manifest must contain id and version", opts.json);
        return 1;
    }
    
    // Detect type
    bool is_nak = manifest.contains("loaders") || 
                  (manifest.contains("lib_dirs") && !manifest.contains("entrypoint_path"));
    
    std::string ext = is_nak ? ".nak" : ".nap";
    
    // Determine output path
    std::string output_path = pack_opts.output;
    if (output_path.empty()) {
        output_path = id + "-" + version + ext;
    }
    
    // For now, report that this is not implemented
    // Full implementation would create a tar.gz archive
    
    if (opts.json) {
        nlohmann::json j;
        j["ok"] = false;
        j["error"] = "Pack functionality not yet implemented";
        j["type"] = is_nak ? "nak" : "app";
        j["id"] = id;
        j["version"] = version;
        j["output"] = output_path;
        output_json(j);
    } else {
        std::cerr << "Error: Pack functionality not yet implemented." << std::endl;
        std::cerr << std::endl;
        std::cerr << "Would create " << (is_nak ? "NAK" : "app") << " package:" << std::endl;
        std::cerr << "  ID: " << id << std::endl;
        std::cerr << "  Version: " << version << std::endl;
        std::cerr << "  Output: " << output_path << std::endl;
        std::cerr << std::endl;
        std::cerr << "For now, use 'nah install <dir>' to install directly from directory." << std::endl;
    }
    
    return 1;  // Return error since functionality is not implemented
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
