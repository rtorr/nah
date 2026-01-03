#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nah {

struct TLVEntry {
    uint16_t tag;
    std::string value;
};

struct ManifestDecodeResult {
    bool ok;
    std::string error;
    std::vector<TLVEntry> entries;
    std::vector<std::string> warnings;
};

// Decode TLV payload with ordering and size limits per SPEC; CRC is expected to be verified separately.
// If expected_total_size is provided and does not match the payload size, entries are discarded and
// invalid_manifest is reported via warnings.
ManifestDecodeResult decode_manifest_tlv(const std::vector<uint8_t>& data,
                                         std::optional<size_t> expected_total_size = std::nullopt);

// Validate ASSET_EXPORT entry of form id:relative_path[:type], returns parsed components or std::nullopt on invalid.
struct AssetExportParts {
    std::string id;
    std::string path;
    std::string type; // may be empty
};
std::optional<AssetExportParts> parse_asset_export(const std::string& value);

} // namespace nah
