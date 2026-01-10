#pragma once

#include "nah/manifest_tlv.hpp"
#include "nah/semver.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace nah {

struct ManifestParseResult {
    bool ok;
    bool critical_missing;
    std::string error;
    std::vector<TLVEntry> entries;
    std::vector<std::string> warnings;
};

// Parse a manifest blob (header + TLV payload). Verifies magic, total_size, and CRC32 over the
// payload. CRC mismatch is treated as manifest missing (critical). Structural invalidities emit
// warnings and drop only invalid fields.
ManifestParseResult parse_manifest_blob(const std::vector<uint8_t>& blob);

struct Manifest {
    std::string id;
    std::string version; // raw string
    std::string nak_id;
    std::optional<VersionRange> nak_version_req;
    std::string entrypoint_path;
    std::vector<std::string> entrypoint_args;
    std::vector<std::string> env_vars;
    std::vector<std::string> lib_dirs;
    std::vector<std::string> asset_dirs;
    std::vector<AssetExportParts> asset_exports;
    std::vector<std::string> permissions_filesystem;
    std::vector<std::string> permissions_network;
    
    // Metadata fields
    std::string description;
    std::string author;
    std::string license;
    std::string homepage;
};

struct ManifestFieldsResult {
    bool ok;
    bool critical_missing;
    std::string error;
    Manifest manifest;
    std::vector<std::string> warnings;
};

// High-level manifest parser that decodes the blob and extracts canonical fields.
ManifestFieldsResult parse_manifest(const std::vector<uint8_t>& blob);

} // namespace nah
