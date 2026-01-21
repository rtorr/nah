/**
 * NAH CLI - install command
 *
 * Install an app or NAK from directory, file, or URL.
 */

#include "../common.hpp"
#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>
#include <cstring>
#include <zlib.h>

namespace nah::cli::commands {

namespace {

struct InstallOptions {
    std::string source;
    bool force = false;
    bool clean = false;
    bool as_app = false;
    bool as_nak = false;
    bool dry_run = false;
};

enum class SourceType {
    Host,
    Directory,
    NapFile,
    NakFile,
    Url
};

std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis32;
    static std::uniform_int_distribution<uint16_t> dis16;

    // Generate UUID v4 according to RFC 4122
    uint32_t time_low = dis32(gen);
    uint16_t time_mid = dis16(gen);
    uint16_t time_hi_version = (dis16(gen) & 0x0FFF) | 0x4000;  // Version 4
    uint16_t clock_seq = (dis16(gen) & 0x3FFF) | 0x8000;  // Variant 10
    uint32_t node_low = dis32(gen);
    uint16_t node_hi = dis16(gen);

    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
             "%08x-%04x-%04x-%04x-%04x%08x",
             time_low,
             time_mid,
             time_hi_version,
             clock_seq,
             node_hi,
             node_low);

    return std::string(uuid_str);
}

// Gzip decompression
std::optional<std::vector<uint8_t>> gzip_decompress(const std::vector<uint8_t>& compressed) {
    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));

    // 16 + MAX_WBITS tells zlib to handle gzip format
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        return std::nullopt;
    }

    stream.next_in = const_cast<Bytef*>(compressed.data());
    stream.avail_in = static_cast<uInt>(compressed.size());

    std::vector<uint8_t> decompressed;
    const size_t CHUNK = 16384;
    uint8_t out[CHUNK];

    int ret;
    do {
        stream.avail_out = CHUNK;
        stream.next_out = out;

        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&stream);
            return std::nullopt;
        }

        size_t have = CHUNK - stream.avail_out;
        decompressed.insert(decompressed.end(), out, out + have);
    } while (stream.avail_out == 0);

    inflateEnd(&stream);

    if (ret != Z_STREAM_END) {
        return std::nullopt;
    }

    return decompressed;
}

// Simple tar extraction
bool extract_tar(const std::vector<uint8_t>& tar_data, const std::string& dest_dir) {
    size_t offset = 0;

    std::filesystem::create_directories(dest_dir);

    while (offset + 512 <= tar_data.size()) {
        // Read header (512 bytes)
        const uint8_t* header = tar_data.data() + offset;
        offset += 512;

        // Check for end of archive (two zero blocks)
        bool all_zero = true;
        for (size_t i = 0; i < 512; ++i) {
            if (header[i] != 0) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) break;

        // Parse header
        char name[101] = {0};
        std::memcpy(name, header, 100);

        char mode_octal[9] = {0};
        std::memcpy(mode_octal, header + 100, 8);
        unsigned int mode = static_cast<unsigned int>(std::stoul(mode_octal, nullptr, 8));

        char size_octal[13] = {0};
        std::memcpy(size_octal, header + 124, 12);
        size_t size = std::stoull(size_octal, nullptr, 8);

        uint8_t typeflag = header[156];

        if (name[0] == '\0') break;

        // Calculate padded size (512-byte blocks)
        size_t padded_size = ((size + 511) / 512) * 512;

        if (offset + padded_size > tar_data.size()) {
            return false; // Corrupted tar
        }

        std::filesystem::path file_path = std::filesystem::path(dest_dir) / name;

        // Type: '0' or '\0' = regular file, '5' = directory
        if (typeflag == '5') {
            // Directory
            std::filesystem::create_directories(file_path);
        } else if (typeflag == '0' || typeflag == '\0') {
            // Regular file
            std::filesystem::create_directories(file_path.parent_path());

            std::ofstream file(file_path, std::ios::binary);
            if (!file) {
                return false;
            }

            file.write(reinterpret_cast<const char*>(tar_data.data() + offset), static_cast<std::streamsize>(size));
            file.close();

            // Apply permissions from tar header
            if (mode == 0) {
                // If mode is 0, tar header might be corrupted or missing permissions
                // Default to 0644 for regular files
                mode = 0644;
            }
            
            std::filesystem::permissions(file_path,
                static_cast<std::filesystem::perms>(mode),
                std::filesystem::perm_options::replace);
        }

        offset += padded_size;
    }

    return true;
}

SourceType detect_source_type(const std::string& source) {
    // URL
    if (source.find("http://") == 0 || source.find("https://") == 0) {
        return SourceType::Url;
    }

    // File extension
    if (source.size() > 4) {
        std::string ext = source.substr(source.size() - 4);
        if (ext == ".nap") return SourceType::NapFile;
        if (ext == ".nak") return SourceType::NakFile;
    }

    // Directory - check manifest type (names match package extensions)
    if (nah::fs::is_directory(source)) {
        // Try app manifest
        if (nah::fs::exists(source + "/nap.json")) {
            return SourceType::Directory;
        }
        
        // Try NAK manifest
        if (nah::fs::exists(source + "/nak.json")) {
            return SourceType::Directory;
        }
        
        // Try host configuration
        auto host_content = nah::fs::read_file(source + "/nah.json");
        if (host_content) {
            try {
                auto j = nlohmann::json::parse(*host_content);
                // Check if it's a host config (has host.root or host section)
                if (j.contains("host") && j["host"].is_object()) {
                    return SourceType::Host;
                }
            } catch (...) {}
        }
        
        return SourceType::Directory;
    }

    return SourceType::Directory; // Default
}

int install_from_directory(const GlobalOptions& opts, const InstallOptions& install_opts,
                           const std::string& source_dir, const std::string& nah_root) {
    init_warning_collector(opts.json, opts.quiet);
    auto paths = get_nah_paths(nah_root);

    // Try NAH-specific manifest files (names match package extensions)
    auto manifest_content = nah::fs::read_file(source_dir + "/nap.json");
    std::string manifest_type = "nap";
    
    if (!manifest_content) {
        manifest_content = nah::fs::read_file(source_dir + "/nak.json");
        manifest_type = "nak";
    }
    if (!manifest_content) {
        manifest_content = nah::fs::read_file(source_dir + "/nah.json");
        manifest_type = "nah";
    }
    
    if (!manifest_content) {
        print_error("No manifest found in: " + source_dir + "\n"
                   "Expected one of: nap.json, nak.json, or nah.json", opts.json);
        return 1;
    }

    // Parse JSON manifest
    nlohmann::json manifest;
    try {
        manifest = nlohmann::json::parse(*manifest_content);
    } catch (const std::exception& e) {
        print_error("Failed to parse manifest JSON: " + std::string(e.what()) + "\n"
                   "Please check the manifest syntax at https://docs.nah.io/manifest", opts.json);
        return 1;
    }

    // Detect manifest type from structure and extract identity
    bool is_app = manifest.contains("app") && manifest["app"].is_object();
    bool is_nak = manifest.contains("nak") && manifest["nak"].is_object();
    bool is_host = manifest.contains("host") && manifest["host"].is_object();
    
    std::string id, version;
    
    if (is_app && !install_opts.as_nak) {
        // Extract from nested app.identity structure
        if (!manifest["app"].contains("identity") || !manifest["app"]["identity"].is_object()) {
            print_error("Invalid app manifest: missing 'app.identity' section", opts.json);
            return 1;
        }
        id = manifest["app"]["identity"].value("id", "");
        version = manifest["app"]["identity"].value("version", "");
    } else if (is_nak || install_opts.as_nak) {
        // Extract from nested nak.identity structure
        if (!manifest["nak"].contains("identity") || !manifest["nak"]["identity"].is_object()) {
            print_error("Invalid NAK manifest: missing 'nak.identity' section", opts.json);
            return 1;
        }
        id = manifest["nak"]["identity"].value("id", "");
        version = manifest["nak"]["identity"].value("version", "");
        is_nak = true;
    } else if (is_host) {
        print_error("Host manifest detected. Use 'nah host install' for host setup.", opts.json);
        return 1;
    } else {
        print_error("Invalid manifest structure: expected 'app', 'nak', or 'host' section", opts.json);
        return 1;
    }

    if (id.empty() || version.empty()) {
        print_error("Invalid manifest: missing required 'id' or 'version' in identity section", opts.json);
        return 1;
    }

    if (install_opts.dry_run) {
        if (opts.json) {
            nlohmann::json j;
            j["would_install"] = true;
            j["type"] = is_nak ? "nak" : "app";
            j["id"] = id;
            j["version"] = version;
            output_json(j);
        } else {
            std::cout << "Would install " << (is_nak ? "NAK" : "app") << ": " << id << "@" << version << std::endl;
        }
        return 0;
    }

    // Ensure NAH structure
    ensure_nah_structure(nah_root);

    if (is_nak) {
        // Install as NAK
        std::string install_dir = nah::fs::absolute_path(paths.naks + "/" + id + "/" + version);
        std::string record_path = paths.registry_naks + "/" + id + "@" + version + ".json";

        // Check existing
        if (nah::fs::exists(record_path) && !install_opts.force) {
            print_error("NAK " + id + "@" + version + " already installed. Use --force to overwrite.", opts.json);
            return 1;
        }

        // Remove existing
        if (nah::fs::exists(install_dir)) {
            std::filesystem::remove_all(install_dir);
        }

        // Copy to install location
        std::filesystem::create_directories(install_dir);
        std::filesystem::copy(source_dir, install_dir,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);

        // Create NAK descriptor (registry record)
        nah::core::RuntimeDescriptor runtime;
        runtime.nak.id = id;
        runtime.nak.version = version;
        runtime.paths.root = install_dir;

        // Extract lib_dirs from nak.paths.lib_dirs
        if (manifest["nak"].contains("paths") && manifest["nak"]["paths"].contains("lib_dirs")) {
            for (const auto& dir : manifest["nak"]["paths"]["lib_dirs"]) {
                runtime.paths.lib_dirs.push_back(install_dir + "/" + dir.get<std::string>());
            }
        }

        // Check for loader in nak.loader
        if (manifest["nak"].contains("loader") && manifest["nak"]["loader"].is_object()) {
            auto& loader_json = manifest["nak"]["loader"];
            nah::core::LoaderConfig loader;
            
            if (loader_json.contains("exec_path")) {
                std::string exec_path = loader_json["exec_path"].get<std::string>();
                if (!std::filesystem::path(exec_path).is_absolute()) {
                    exec_path = install_dir + "/" + exec_path;
                }
                loader.exec_path = exec_path;
            }
            if (loader_json.contains("args_template")) {
                for (const auto& arg : loader_json["args_template"]) {
                    loader.args_template.push_back(arg.get<std::string>());
                }
            }
            runtime.loaders["default"] = loader;
        }

        // Write registry record (manually serialize since no serialization function exists)
        nlohmann::json nak_record;
        nak_record["nak"]["id"] = runtime.nak.id;
        nak_record["nak"]["version"] = runtime.nak.version;
        nak_record["paths"]["root"] = runtime.paths.root;

        if (!runtime.paths.lib_dirs.empty()) {
            nak_record["paths"]["lib_dirs"] = runtime.paths.lib_dirs;
        }

        if (!runtime.loaders.empty()) {
            for (const auto& [name, loader] : runtime.loaders) {
                nlohmann::json loader_json;
                if (!loader.exec_path.empty()) {
                    loader_json["exec_path"] = loader.exec_path;
                }
                if (!loader.args_template.empty()) {
                    loader_json["args_template"] = loader.args_template;
                }
                nak_record["loaders"][name] = loader_json;
            }
        }

        std::ofstream record_file(record_path);
        record_file << nak_record.dump(2);
        record_file.close();

        if (opts.json) {
            nlohmann::json j;
            j["ok"] = true;
            j["nak"]["id"] = id;
            j["nak"]["version"] = version;
            j["paths"]["root"] = install_dir;
            output_json(j);
        } else {
            std::cout << "Installed NAK " << id << "@" << version << std::endl;
        }
    } else {
        // Install as app
        std::string install_dir = nah::fs::absolute_path(paths.apps + "/" + id + "-" + version);
        std::string record_path = paths.registry_apps + "/" + id + "@" + version + ".json";

        // Check existing
        if (nah::fs::exists(record_path) && !install_opts.force) {
            print_error("App " + id + "@" + version + " already installed. Use --force to overwrite.", opts.json);
            return 1;
        }

        // Remove existing
        if (nah::fs::exists(install_dir)) {
            std::filesystem::remove_all(install_dir);
        }

        // Copy to install location
        std::filesystem::create_directories(install_dir);
        std::filesystem::copy(source_dir, install_dir,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);

        // Create install record
        nah::core::InstallRecord record;
        record.install.instance_id = generate_uuid();
        record.app.id = id;
        record.app.version = version;
        record.paths.install_root = install_dir;

        // Handle NAK dependency (for app manifests)
        if (is_app && manifest["app"]["identity"].contains("nak_id")) {
            auto& app_identity = manifest["app"]["identity"];
            record.app.nak_id = app_identity["nak_id"].get<std::string>();
            record.app.nak_version_req = app_identity.value("nak_version_req", "");

            // Try to find and pin NAK
            std::string nak_id = record.app.nak_id;
            auto nak_files = nah::fs::list_directory(paths.registry_naks);
            bool nak_found = false;
            for (const auto& f : nak_files) {
                if (f.find(nak_id + "@") == 0 && f.substr(f.size() - 5) == ".json") {
                    // Extract version from filename
                    std::string nak_version = f.substr(nak_id.size() + 1, f.size() - nak_id.size() - 6);
                    record.nak.id = nak_id;
                    record.nak.version = nak_version;
                    record.nak.record_ref = f;
                    record.nak.loader = "default";  // Will be determined at composition time
                    record.nak.selection_reason = "matched_requirement";
                    nak_found = true;
                    break;
                }
            }
            
            if (!nak_found) {
                print_warning("NAK '" + nak_id + "' not found. App may fail to run until NAK is installed.", opts.json);
            }
        }

        // Trust info
        record.trust.state = nah::core::TrustState::Unknown;
        record.trust.source = "local_install";
        record.trust.evaluated_at = nah::core::get_current_timestamp();

        // Provenance (not metadata)
        record.provenance.package_hash = "";
        record.provenance.installed_at = record.trust.evaluated_at;
        record.provenance.installed_by = "nah_cli";
        record.provenance.source = source_dir;

        // Write registry record (manually serialize since no serialization function exists)
        nlohmann::json install_record;

        // Install info
        install_record["install"]["instance_id"] = record.install.instance_id;

        // App snapshot
        install_record["app"]["id"] = record.app.id;
        install_record["app"]["version"] = record.app.version;
        if (!record.app.nak_id.empty()) {
            install_record["app"]["nak_id"] = record.app.nak_id;
        }
        if (!record.app.nak_version_req.empty()) {
            install_record["app"]["nak_version_req"] = record.app.nak_version_req;
        }

        // Pinned NAK
        if (!record.nak.id.empty()) {
            install_record["nak"]["id"] = record.nak.id;
            install_record["nak"]["version"] = record.nak.version;
            install_record["nak"]["record_ref"] = record.nak.record_ref;
            install_record["nak"]["loader"] = record.nak.loader;
            install_record["nak"]["selection_reason"] = record.nak.selection_reason;
        }

        // Paths
        install_record["paths"]["install_root"] = record.paths.install_root;

        // Trust
        install_record["trust"]["state"] = "unknown";
        install_record["trust"]["source"] = record.trust.source;
        install_record["trust"]["evaluated_at"] = record.trust.evaluated_at;

        // Provenance
        install_record["provenance"]["package_hash"] = record.provenance.package_hash;
        install_record["provenance"]["installed_at"] = record.provenance.installed_at;
        install_record["provenance"]["installed_by"] = record.provenance.installed_by;
        install_record["provenance"]["source"] = record.provenance.source;

        std::ofstream record_file(record_path);
        record_file << install_record.dump(2);
        record_file.close();

        if (opts.json) {
            nlohmann::json j;
            j["ok"] = true;
            j["app"]["id"] = id;
            j["app"]["version"] = version;
            j["paths"]["install_root"] = install_dir;
            output_json(j);
        } else {
            std::cout << "Installed " << id << "@" << version << std::endl;
        }
    }

    return 0;
}

int install_from_package(const GlobalOptions& opts, const InstallOptions& install_opts,
                         const std::string& package_path, const std::string& nah_root,
                         bool /* is_nak_package */) {
    // Read package file
    std::ifstream file(package_path, std::ios::binary | std::ios::ate);
    if (!file) {
        print_error("Cannot open package file: " + package_path, opts.json);
        return 1;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> compressed(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(compressed.data()), size)) {
        print_error("Failed to read package file", opts.json);
        return 1;
    }
    file.close();

    // Decompress gzip
    auto decompressed_opt = gzip_decompress(compressed);
    if (!decompressed_opt) {
        print_error("Failed to decompress package file", opts.json);
        return 1;
    }

    // Create temporary directory for extraction
    std::string temp_dir = "/tmp/nah_install_" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir);

    // Extract tar
    if (!extract_tar(*decompressed_opt, temp_dir)) {
        std::filesystem::remove_all(temp_dir);
        print_error("Failed to extract package contents", opts.json);
        return 1;
    }

    // Install from extracted directory
    int result = install_from_directory(opts, install_opts, temp_dir, nah_root);

    // Clean up temp directory
    std::filesystem::remove_all(temp_dir);

    return result;
}

int cmd_install(const GlobalOptions& opts, const InstallOptions& install_opts) {
    std::string nah_root = resolve_nah_root(
        opts.root.empty() ? std::nullopt : std::make_optional(opts.root));

    auto source_type = detect_source_type(install_opts.source);

    switch (source_type) {
        case SourceType::Directory:
            return install_from_directory(opts, install_opts, install_opts.source, nah_root);

        case SourceType::Host:
            // Host install - delegate to host command
            print_error("Host install not supported here. Use 'nah host install' instead.", opts.json);
            return 1;

        case SourceType::NapFile:
            return install_from_package(opts, install_opts, install_opts.source, nah_root, false);

        case SourceType::NakFile:
            return install_from_package(opts, install_opts, install_opts.source, nah_root, true);

        case SourceType::Url:
            print_error("URL install not yet implemented.", opts.json);
            return 1;

        default:
            print_error("Unknown source type", opts.json);
            return 1;
    }
}

} // anonymous namespace

void setup_install(CLI::App* app, GlobalOptions& opts) {
    static InstallOptions install_opts;

    app->add_option("source", install_opts.source, "Directory, .nap file, .nak file, or URL")->required();
    app->add_flag("-f,--force", install_opts.force, "Overwrite existing installation");
    app->add_flag("--clean", install_opts.clean, "Remove existing NAH root (host install only)");
    app->add_flag("--app", install_opts.as_app, "Force install as app");
    app->add_flag("--nak", install_opts.as_nak, "Force install as NAK");
    app->add_flag("--dry-run", install_opts.dry_run, "Show what would be installed");

    app->callback([&opts]() {
        std::exit(cmd_install(opts, install_opts));
    });
}

} // namespace nah::cli::commands