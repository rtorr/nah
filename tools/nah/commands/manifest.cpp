/**
 * NAH CLI - manifest command
 *
 * Manifest tools for generation and validation.
 */

#include "../common.hpp"
#include <CLI/CLI.hpp>
#include <fstream>
#include <sstream>

namespace nah::cli::commands {

namespace {

struct GenerateOptions {
    std::string input;
    std::string output;
};

// Simple binary manifest format (TLV-like)
// Format: [type:1][length:2][data:length]
enum class ManifestFieldType : uint8_t {
    END = 0x00,
    ID = 0x01,
    VERSION = 0x02,
    NAK_ID = 0x03,
    NAK_VERSION_REQ = 0x04,
    ENTRYPOINT = 0x05,
    LIB_DIRS = 0x06,
    ASSET_DIRS = 0x07,
    ENV_VARS = 0x08,
    PERMISSIONS = 0x09,
    NAK_LOADER = 0x0A,
    DESCRIPTION = 0x0B,
    NAME = 0x0C,
};

void write_field(std::vector<uint8_t>& buffer, ManifestFieldType type, const std::string& value) {
    if (value.empty()) return;

    buffer.push_back(static_cast<uint8_t>(type));
    uint16_t len = static_cast<uint16_t>(value.size());
    buffer.push_back(len & 0xFF);
    buffer.push_back((len >> 8) & 0xFF);
    buffer.insert(buffer.end(), value.begin(), value.end());
}

void write_array_field(std::vector<uint8_t>& buffer, ManifestFieldType type, const std::vector<std::string>& values) {
    if (values.empty()) return;

    std::string joined;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) joined += "\0";
        joined += values[i];
    }
    write_field(buffer, type, joined);
}

int cmd_generate(const GlobalOptions& opts, const GenerateOptions& gen_opts) {
    init_warning_collector(opts.json, opts.quiet);
    
    // Read input JSON
    std::string json_content;
    if (gen_opts.input == "-") {
        std::string line;
        while (std::getline(std::cin, line)) {
            json_content += line + "\n";
        }
    } else {
        auto content = nah::fs::read_file(gen_opts.input);
        if (!content) {
            print_error("Failed to read input file: " + gen_opts.input, opts.json);
            return 1;
        }
        json_content = *content;
    }

    // Parse JSON
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json_content);
    } catch (const std::exception& e) {
        print_error("Invalid JSON: " + std::string(e.what()), opts.json);
        return 1;
    }

    // Generate binary manifest
    std::vector<uint8_t> buffer;

    // Magic header: "NAH\x02" (version 2)
    buffer.push_back('N');
    buffer.push_back('A');
    buffer.push_back('H');
    buffer.push_back(0x02);

    // Handle both old schema (nested under "app") and new flat schema
    nlohmann::json app_data = j;
    if (j.contains("app") && j["app"].is_object()) {
        app_data = j["app"];
    }

    // Required fields
    if (app_data.contains("id") && app_data["id"].is_string()) {
        write_field(buffer, ManifestFieldType::ID, app_data["id"]);
    }

    if (app_data.contains("version") && app_data["version"].is_string()) {
        write_field(buffer, ManifestFieldType::VERSION, app_data["version"]);
    }

    // Optional fields
    if (app_data.contains("name") && app_data["name"].is_string()) {
        write_field(buffer, ManifestFieldType::NAME, app_data["name"]);
    }

    if (app_data.contains("description") && app_data["description"].is_string()) {
        write_field(buffer, ManifestFieldType::DESCRIPTION, app_data["description"]);
    }

    if (app_data.contains("nak_id") && app_data["nak_id"].is_string()) {
        write_field(buffer, ManifestFieldType::NAK_ID, app_data["nak_id"]);
    }

    if (app_data.contains("nak_version_req") && app_data["nak_version_req"].is_string()) {
        write_field(buffer, ManifestFieldType::NAK_VERSION_REQ, app_data["nak_version_req"]);
    }

    if (app_data.contains("nak_loader") && app_data["nak_loader"].is_string()) {
        write_field(buffer, ManifestFieldType::NAK_LOADER, app_data["nak_loader"]);
    }

    // Handle both "entrypoint" and "entrypoint_path"
    if (app_data.contains("entrypoint") && app_data["entrypoint"].is_string()) {
        write_field(buffer, ManifestFieldType::ENTRYPOINT, app_data["entrypoint"]);
    } else if (app_data.contains("entrypoint_path") && app_data["entrypoint_path"].is_string()) {
        write_field(buffer, ManifestFieldType::ENTRYPOINT, app_data["entrypoint_path"]);
    }

    // Arrays
    if (app_data.contains("lib_dirs") && app_data["lib_dirs"].is_array()) {
        std::vector<std::string> dirs;
        for (const auto& dir : app_data["lib_dirs"]) {
            if (dir.is_string()) {
                dirs.push_back(dir);
            }
        }
        write_array_field(buffer, ManifestFieldType::LIB_DIRS, dirs);
    }

    if (app_data.contains("asset_dirs") && app_data["asset_dirs"].is_array()) {
        std::vector<std::string> dirs;
        for (const auto& dir : app_data["asset_dirs"]) {
            if (dir.is_string()) {
                dirs.push_back(dir);
            }
        }
        write_array_field(buffer, ManifestFieldType::ASSET_DIRS, dirs);
    }

    if (app_data.contains("env_vars") && app_data["env_vars"].is_array()) {
        std::vector<std::string> vars;
        for (const auto& var : app_data["env_vars"]) {
            if (var.is_string()) {
                vars.push_back(var);
            }
        }
        write_array_field(buffer, ManifestFieldType::ENV_VARS, vars);
    }

    // Permissions (serialize as JSON string for now)
    if (app_data.contains("permissions") && app_data["permissions"].is_object()) {
        write_field(buffer, ManifestFieldType::PERMISSIONS, app_data["permissions"].dump());
    }

    // End marker
    buffer.push_back(static_cast<uint8_t>(ManifestFieldType::END));

    // Write output
    std::string output = gen_opts.output.empty() ? (gen_opts.input + ".nah") : gen_opts.output;

    std::ofstream out(output, std::ios::binary);
    if (!out) {
        print_error("Failed to open output file: " + output, opts.json);
        return 1;
    }

    out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    out.close();

    if (!opts.quiet) {
        std::cout << "Generated manifest: " << output << " (" << buffer.size() << " bytes)" << std::endl;
    }

    return 0;
}

} // anonymous namespace

void setup_manifest(CLI::App* app, GlobalOptions& opts) {
    // app is already the manifest subcommand
    app->require_subcommand(1);

    // generate subcommand
    static GenerateOptions gen_opts;
    auto generate_cmd = app->add_subcommand("generate", "Generate binary manifest from JSON");
    generate_cmd->add_option("input", gen_opts.input, "Input JSON manifest (or - for stdin)")->required();
    generate_cmd->add_option("-o,--output", gen_opts.output, "Output binary manifest path");

    generate_cmd->callback([&opts]() {
        std::exit(cmd_generate(opts, gen_opts));
    });
}

} // namespace nah::cli::commands