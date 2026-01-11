#include "nah/manifest.hpp"
#include "nah/manifest_tlv.hpp"
#include "nah/semver.hpp"

#include <cstdint>
#include <vector>

namespace nah {

namespace {

struct ManifestHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t total_size;
    uint32_t crc32;
};

constexpr uint32_t MANIFEST_MAGIC = 0x4D48414E; // "NAHM" little-endian value
constexpr uint16_t MANIFEST_VERSION = 1;
constexpr uint16_t MANIFEST_TAG_NAK_ID = 12;
constexpr uint16_t MANIFEST_TAG_ID = 10;
constexpr uint16_t MANIFEST_TAG_VERSION = 11;
constexpr uint16_t MANIFEST_TAG_NAK_VERSION_REQ = 13;
constexpr uint16_t MANIFEST_TAG_NAK_LOADER = 14;
constexpr uint16_t MANIFEST_TAG_ENTRYPOINT_PATH = 20;
constexpr uint16_t MANIFEST_TAG_ENTRYPOINT_ARG = 21;
constexpr uint16_t MANIFEST_TAG_ENV_VAR = 30;
constexpr uint16_t MANIFEST_TAG_LIB_DIR = 40;
constexpr uint16_t MANIFEST_TAG_ASSET_DIR = 41;
constexpr uint16_t MANIFEST_TAG_ASSET_EXPORT = 42;
constexpr uint16_t MANIFEST_TAG_PERMISSION_FILESYSTEM = 50;
constexpr uint16_t MANIFEST_TAG_PERMISSION_NETWORK = 51;
constexpr uint16_t MANIFEST_TAG_DESCRIPTION = 60;
constexpr uint16_t MANIFEST_TAG_AUTHOR = 61;
constexpr uint16_t MANIFEST_TAG_LICENSE = 62;
constexpr uint16_t MANIFEST_TAG_HOMEPAGE = 63;

uint32_t read_le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t read_le16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

ManifestHeader parse_header(const uint8_t* data) {
    ManifestHeader h{};
    h.magic = read_le32(data);
    h.version = read_le16(data + 4);
    h.reserved = read_le16(data + 6);
    h.total_size = read_le32(data + 8);
    h.crc32 = read_le32(data + 12);
    return h;
}

uint32_t crc32_le(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            uint32_t mask = (crc & 1u) ? 0xFFFFFFFFu : 0u;
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

} // namespace

ManifestParseResult parse_manifest_blob(const std::vector<uint8_t>& blob) {
    ManifestParseResult out{true, false, {}, {}, {}};
    constexpr size_t HEADER_SIZE = sizeof(ManifestHeader);

    if (blob.size() < HEADER_SIZE) {
        out.ok = false;
        out.critical_missing = true;
        out.error = "header_too_small";
        return out;
    }

    auto header = parse_header(blob.data());
    if (header.magic != MANIFEST_MAGIC) {
        out.ok = false;
        out.critical_missing = true;
        out.error = "bad_magic";
        return out;
    }

    if (header.version != MANIFEST_VERSION) {
        out.warnings.push_back("invalid_manifest:version");
    }

    if (header.total_size != blob.size()) {
        out.warnings.push_back("invalid_manifest:total_size_mismatch");
        // Per spec, treat all fields as absent for this decode attempt.
        return out;
    }

    const uint8_t* payload = blob.data() + HEADER_SIZE;
    size_t payload_size = blob.size() - HEADER_SIZE;
    uint32_t computed_crc = crc32_le(payload, payload_size);
    if (computed_crc != header.crc32) {
        out.ok = false;
        out.critical_missing = true;
        out.error = "crc_mismatch";
        return out;
    }

    auto tlv = decode_manifest_tlv(std::vector<uint8_t>(payload, payload + payload_size),
                                   payload_size);
    out.ok = tlv.ok;
    out.entries = std::move(tlv.entries);
    out.warnings.insert(out.warnings.end(), tlv.warnings.begin(), tlv.warnings.end());
    if (!tlv.ok) {
        out.error = tlv.error;
    }

    // Additional semantic validation per spec: nak_id must be present and non-empty.
    bool has_nak_id = false;
    for (const auto& e : out.entries) {
        if (e.tag == MANIFEST_TAG_NAK_ID && !e.value.empty()) {
            has_nak_id = true;
            break;
        }
    }
    if (!has_nak_id) {
        out.warnings.push_back("invalid_manifest:nak_id_missing");
    }

    return out;
}

ManifestFieldsResult parse_manifest(const std::vector<uint8_t>& blob) {
    ManifestFieldsResult res{};
    auto low = parse_manifest_blob(blob);
    res.ok = low.ok;
    res.critical_missing = low.critical_missing;
    res.error = low.error;
    res.warnings = low.warnings;
    if (!low.ok && low.critical_missing) {
        return res;
    }

    Manifest manifest{};
    for (const auto& e : low.entries) {
        switch (e.tag) {
            case MANIFEST_TAG_ID:
                if (manifest.id.empty()) manifest.id = e.value;
                break;
            case MANIFEST_TAG_VERSION:
                if (manifest.version.empty()) manifest.version = e.value;
                break;
            case MANIFEST_TAG_NAK_ID:
                if (manifest.nak_id.empty()) manifest.nak_id = e.value;
                break;
            case MANIFEST_TAG_NAK_VERSION_REQ: {
                if (!manifest.nak_version_req.has_value()) {
                    auto range = parse_range(e.value);
                    if (range) {
                        manifest.nak_version_req = range;
                    } else {
                        res.warnings.push_back("invalid_manifest:nak_version_req");
                    }
                }
                break;
            }
            case MANIFEST_TAG_NAK_LOADER:
                if (manifest.nak_loader.empty()) manifest.nak_loader = e.value;
                break;
            case MANIFEST_TAG_ENTRYPOINT_PATH:
                if (manifest.entrypoint_path.empty()) manifest.entrypoint_path = e.value;
                break;
            case MANIFEST_TAG_ENTRYPOINT_ARG:
                manifest.entrypoint_args.push_back(e.value);
                break;
            case MANIFEST_TAG_ENV_VAR:
                manifest.env_vars.push_back(e.value);
                break;
            case MANIFEST_TAG_LIB_DIR:
                manifest.lib_dirs.push_back(e.value);
                break;
            case MANIFEST_TAG_ASSET_DIR:
                manifest.asset_dirs.push_back(e.value);
                break;
            case MANIFEST_TAG_ASSET_EXPORT: {
                auto parts = parse_asset_export(e.value);
                if (parts) {
                    manifest.asset_exports.push_back(*parts);
                } else {
                    res.warnings.push_back("invalid_manifest:asset_export");
                }
                break;
            }
            case MANIFEST_TAG_PERMISSION_FILESYSTEM:
                manifest.permissions_filesystem.push_back(e.value);
                break;
            case MANIFEST_TAG_PERMISSION_NETWORK:
                manifest.permissions_network.push_back(e.value);
                break;
            case MANIFEST_TAG_DESCRIPTION:
                if (manifest.description.empty()) manifest.description = e.value;
                break;
            case MANIFEST_TAG_AUTHOR:
                if (manifest.author.empty()) manifest.author = e.value;
                break;
            case MANIFEST_TAG_LICENSE:
                if (manifest.license.empty()) manifest.license = e.value;
                break;
            case MANIFEST_TAG_HOMEPAGE:
                if (manifest.homepage.empty()) manifest.homepage = e.value;
                break;
            default:
                break;
        }
    }

    if (manifest.nak_id.empty()) {
        res.warnings.push_back("invalid_manifest:nak_id_missing");
    }
    if (manifest.id.empty()) {
        res.warnings.push_back("invalid_manifest:id_missing");
    }
    if (manifest.version.empty()) {
        res.warnings.push_back("invalid_manifest:version_missing");
    } else if (!parse_version(manifest.version).has_value()) {
        res.warnings.push_back("invalid_manifest:version_invalid");
        manifest.version.clear();
    }

    if (manifest.entrypoint_path.empty()) {
        res.warnings.push_back("invalid_manifest:entrypoint_missing");
    }

    res.manifest = std::move(manifest);
    return res;
}

} // namespace nah
