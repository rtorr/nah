#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace nah {

// ============================================================================
// Manifest Input (per SPEC: Manifest Input Format)
// ============================================================================

struct ManifestInput {
    std::string id;
    std::string version;
    std::string nak_id;
    std::string nak_version_req;
    std::string entrypoint;
    
    std::vector<std::string> entrypoint_args;
    std::string description;
    std::string author;
    std::string license;
    std::string homepage;
    
    std::vector<std::string> lib_dirs;
    std::vector<std::string> asset_dirs;
    
    struct AssetExport {
        std::string id;
        std::string path;
        std::string type;
    };
    std::vector<AssetExport> exports;
    
    std::unordered_map<std::string, std::string> environment;
    
    std::vector<std::string> permissions_filesystem;
    std::vector<std::string> permissions_network;
};

struct ManifestInputParseResult {
    bool ok = false;
    std::string error;
    ManifestInput input;
    std::vector<std::string> warnings;
};

// Parse manifest input from TOML content
// Schema must be "nah.manifest.input.v1"
ManifestInputParseResult parse_manifest_input(const std::string& toml_content);

// Build TLV manifest bytes from parsed input
std::vector<uint8_t> build_manifest_from_input(const ManifestInput& input);

// ============================================================================
// Manifest Generation Result
// ============================================================================

struct ManifestGenerateResult {
    bool ok = false;
    std::string error;
    std::vector<uint8_t> manifest_bytes;
    std::vector<std::string> warnings;
};

// Generate manifest from TOML input string
ManifestGenerateResult generate_manifest(const std::string& toml_content);

} // namespace nah
