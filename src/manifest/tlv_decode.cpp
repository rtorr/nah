#include "nah/manifest_tlv.hpp"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace nah {

namespace {
constexpr size_t MAX_TOTAL_SIZE = 64 * 1024; // 64 KiB
constexpr size_t MAX_ENTRIES = 512;
constexpr size_t MAX_STRING = 4096;
constexpr size_t MAX_REPEATS = 128;

enum ManifestTag : uint16_t {
    END = 0,
    SCHEMA_VERSION = 1,
    ID = 10,
    VERSION = 11,
    NAK_ID = 12,
    NAK_VERSION_REQ = 13,
    NAK_LOADER = 14,
    ENTRYPOINT_PATH = 20,
    ENTRYPOINT_ARG = 21,
    ENV_VAR = 30,
    LIB_DIR = 40,
    ASSET_DIR = 41,
    ASSET_EXPORT = 42,
    PERMISSION_FILESYSTEM = 50,
    PERMISSION_NETWORK = 51,
};

bool is_repeatable(uint16_t tag) {
    return tag == ENTRYPOINT_ARG || tag == ENV_VAR || tag == LIB_DIR || tag == ASSET_DIR ||
           tag == ASSET_EXPORT || tag == PERMISSION_FILESYSTEM || tag == PERMISSION_NETWORK;
}

std::vector<std::string> split_once(const std::string& s, char delim) {
    auto pos = s.find(delim);
    if (pos == std::string::npos) return {s};
    return {s.substr(0, pos), s.substr(pos + 1)};
}

bool is_relative_path(const std::string& value) {
    return !value.empty() && value[0] != '/';
}

// Check if string contains NUL bytes (per SPEC L1501, L1563: strings must be UTF-8 without NUL)
bool contains_nul(const std::string& value) {
    return value.find('\0') != std::string::npos;
}

bool is_valid_env_var(const std::string& value) {
    auto pos = value.find('=');
    if (pos == std::string::npos || pos == 0) return false;
    // key must not contain additional '='
    if (value.find('=', pos + 1) != std::string::npos) return false;
    return true;
}

bool validate_tag_value(uint16_t tag, const std::string& value) {
    // All string values must not contain NUL bytes (per SPEC L1501, L1563)
    if (contains_nul(value)) {
        return false;
    }
    
    switch (tag) {
        case ENTRYPOINT_PATH:
        case LIB_DIR:
        case ASSET_DIR:
            return is_relative_path(value);
        case ENV_VAR:
            return is_valid_env_var(value);
        case ASSET_EXPORT:
            return parse_asset_export(value).has_value();
        default:
            return true;
    }
}

} // namespace

ManifestDecodeResult decode_manifest_tlv(const std::vector<uint8_t>& data,
                                         std::optional<size_t> expected_total_size) {
    ManifestDecodeResult result{true, {}, {}, {}};

    if (expected_total_size.has_value() && *expected_total_size != data.size()) {
        result.warnings.push_back("invalid_manifest:total_size_mismatch");
        return result;
    }

    if (data.size() > MAX_TOTAL_SIZE) {
        result.warnings.push_back("invalid_manifest:total_size_exceeded");
        return result;
    }

    size_t offset = 0;
    uint16_t last_tag = 0;
    size_t encountered_entries = 0;
    std::unordered_map<uint16_t, size_t> repeat_counts;

    while (offset + 4 <= data.size()) {
        if (encountered_entries >= MAX_ENTRIES) {
            result.warnings.push_back("invalid_manifest:entry_limit_exceeded");
            break;
        }
        uint16_t tag = static_cast<uint16_t>(data[offset]) |
                       static_cast<uint16_t>(data[offset + 1] << 8);
        uint16_t len = static_cast<uint16_t>(data[offset + 2]) |
                       static_cast<uint16_t>(data[offset + 3] << 8);
        offset += 4;
        encountered_entries++;
        if (offset + len > data.size()) {
            result.ok = false;
            result.error = "length_out_of_bounds";
            result.entries.clear();
            return result;
        }
        std::string value(reinterpret_cast<const char*>(&data[offset]), len);
        offset += len;

        // END tag handling
        if (tag == END) {
            if (len != 0) {
                result.warnings.push_back("invalid_manifest:end_length_nonzero");
                continue; // ignore invalid END
            }
            if (offset != data.size()) {
                // END not final
                result.warnings.push_back("invalid_manifest:end_not_final");
                continue;
            }
            break;
        }

        // tag ordering
        if (!result.entries.empty() && tag < last_tag) {
            result.warnings.push_back("invalid_manifest:tag_order");
            continue;
        }
        last_tag = tag;

        // repeat limit
        if (is_repeatable(tag)) {
            auto& count = repeat_counts[tag];
            count++;
            if (count > MAX_REPEATS) {
                result.warnings.push_back("invalid_manifest:repeat_limit");
                continue; // ignore extra repeats
            }
        } else {
            if (repeat_counts[tag] > 0) {
                result.warnings.push_back("invalid_manifest:duplicate_nonrepeatable");
                continue; // repeated non-repeatable tag, ignore this occurrence
            }
            repeat_counts[tag] = 1;
        }

        if (value.size() > MAX_STRING) {
            result.warnings.push_back("invalid_manifest:string_too_long");
            continue; // ignore oversize strings
        }

        if (!validate_tag_value(tag, value)) {
            result.warnings.push_back("invalid_manifest:invalid_value");
            continue;
        }

        result.entries.push_back(TLVEntry{tag, std::move(value)});
    }

    return result;
}

std::optional<AssetExportParts> parse_asset_export(const std::string& value) {
    auto first_split = split_once(value, ':');
    if (first_split.size() < 2) return std::nullopt;
    const auto& id = first_split[0];
    if (id.empty()) return std::nullopt;
    auto rest = first_split[1];
    auto second_split = split_once(rest, ':');
    const auto& rel = second_split[0];
    if (rel.empty() || rel[0] == '/') return std::nullopt;
    std::string type;
    if (second_split.size() > 1) type = second_split[1];
    return AssetExportParts{id, rel, type};
}

} // namespace nah
