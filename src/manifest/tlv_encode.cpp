#include "nah/manifest_builder.hpp"

#include <algorithm>
#include <cstring>

namespace nah {

namespace {

// TLV Tags (per SPEC L1527-L1558)
enum ManifestTag : uint16_t {
    END = 0,
    SCHEMA_VERSION = 1,
    ID = 10,
    VERSION = 11,
    NAK_ID = 12,
    NAK_VERSION_REQ = 13,
    ENTRYPOINT_PATH = 20,
    ENTRYPOINT_ARG = 21,
    ENV_VAR = 30,
    LIB_DIR = 40,
    ASSET_DIR = 41,
    ASSET_EXPORT = 42,
    PERMISSION_FILESYSTEM = 50,
    PERMISSION_NETWORK = 51,
    DESCRIPTION = 60,
    AUTHOR = 61,
    LICENSE = 62,
    HOMEPAGE = 63,
};

// Manifest header (per SPEC L1518-L1525)
constexpr uint32_t MANIFEST_MAGIC = 0x4D48414E;  // "NAHM" little-endian
constexpr uint16_t MANIFEST_VERSION = 1;
constexpr size_t HEADER_SIZE = 16;

void write_le16(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

void write_le32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void write_tlv(std::vector<uint8_t>& buf, uint16_t tag, const std::string& value) {
    write_le16(buf, tag);
    write_le16(buf, static_cast<uint16_t>(value.size()));
    buf.insert(buf.end(), value.begin(), value.end());
}

// CRC32 (IEEE poly, reflected form) - per SPEC L1523
uint32_t crc32_le(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

struct TLVEntry {
    uint16_t tag;
    std::string value;
};

} // namespace

ManifestBuilder& ManifestBuilder::id(const std::string& value) {
    id_ = value;
    return *this;
}

ManifestBuilder& ManifestBuilder::version(const std::string& value) {
    version_ = value;
    return *this;
}

ManifestBuilder& ManifestBuilder::nak_id(const std::string& value) {
    nak_id_ = value;
    return *this;
}

ManifestBuilder& ManifestBuilder::nak_version_req(const std::string& value) {
    nak_version_req_ = value;
    return *this;
}

ManifestBuilder& ManifestBuilder::entrypoint(const std::string& value) {
    entrypoint_ = value;
    return *this;
}

ManifestBuilder& ManifestBuilder::entrypoint_arg(const std::string& value) {
    entrypoint_args_.push_back(value);
    return *this;
}

ManifestBuilder& ManifestBuilder::env(const std::string& key, const std::string& value) {
    env_vars_.push_back(key + "=" + value);
    return *this;
}

ManifestBuilder& ManifestBuilder::lib_dir(const std::string& value) {
    lib_dirs_.push_back(value);
    return *this;
}

ManifestBuilder& ManifestBuilder::asset_dir(const std::string& value) {
    asset_dirs_.push_back(value);
    return *this;
}

ManifestBuilder& ManifestBuilder::asset_export(const std::string& id, 
                                                 const std::string& path,
                                                 const std::string& type) {
    std::string entry = id + ":" + path;
    if (!type.empty()) {
        entry += ":" + type;
    }
    asset_exports_.push_back(entry);
    return *this;
}

ManifestBuilder& ManifestBuilder::filesystem_permission(const std::string& value) {
    filesystem_permissions_.push_back(value);
    return *this;
}

ManifestBuilder& ManifestBuilder::network_permission(const std::string& value) {
    network_permissions_.push_back(value);
    return *this;
}

ManifestBuilder& ManifestBuilder::description(const std::string& value) {
    description_ = value;
    return *this;
}

ManifestBuilder& ManifestBuilder::author(const std::string& value) {
    author_ = value;
    return *this;
}

ManifestBuilder& ManifestBuilder::license(const std::string& value) {
    license_ = value;
    return *this;
}

ManifestBuilder& ManifestBuilder::homepage(const std::string& value) {
    homepage_ = value;
    return *this;
}

std::vector<uint8_t> ManifestBuilder::build() const {
    // Collect all entries
    std::vector<TLVEntry> entries;
    
    // SCHEMA_VERSION
    entries.push_back({SCHEMA_VERSION, std::string(reinterpret_cast<const char*>("\x01\x00"), 2)});
    
    // Identity
    if (!id_.empty()) {
        entries.push_back({ID, id_});
    }
    if (!version_.empty()) {
        entries.push_back({VERSION, version_});
    }
    if (!nak_id_.empty()) {
        entries.push_back({NAK_ID, nak_id_});
    }
    if (!nak_version_req_.empty()) {
        entries.push_back({NAK_VERSION_REQ, nak_version_req_});
    }
    
    // Execution
    if (!entrypoint_.empty()) {
        entries.push_back({ENTRYPOINT_PATH, entrypoint_});
    }
    for (const auto& arg : entrypoint_args_) {
        entries.push_back({ENTRYPOINT_ARG, arg});
    }
    
    // Environment
    for (const auto& env : env_vars_) {
        entries.push_back({ENV_VAR, env});
    }
    
    // Layout
    for (const auto& lib : lib_dirs_) {
        entries.push_back({LIB_DIR, lib});
    }
    for (const auto& asset : asset_dirs_) {
        entries.push_back({ASSET_DIR, asset});
    }
    for (const auto& exp : asset_exports_) {
        entries.push_back({ASSET_EXPORT, exp});
    }
    
    // Permissions
    for (const auto& perm : filesystem_permissions_) {
        entries.push_back({PERMISSION_FILESYSTEM, perm});
    }
    for (const auto& perm : network_permissions_) {
        entries.push_back({PERMISSION_NETWORK, perm});
    }
    
    // Metadata
    if (!description_.empty()) {
        entries.push_back({DESCRIPTION, description_});
    }
    if (!author_.empty()) {
        entries.push_back({AUTHOR, author_});
    }
    if (!license_.empty()) {
        entries.push_back({LICENSE, license_});
    }
    if (!homepage_.empty()) {
        entries.push_back({HOMEPAGE, homepage_});
    }
    
    // Sort entries by tag (per SPEC: entries MUST appear in ascending tag order)
    std::stable_sort(entries.begin(), entries.end(),
                     [](const TLVEntry& a, const TLVEntry& b) { return a.tag < b.tag; });
    
    // Build TLV payload
    std::vector<uint8_t> payload;
    for (const auto& entry : entries) {
        write_tlv(payload, entry.tag, entry.value);
    }
    
    // Add END tag
    write_tlv(payload, END, "");
    
    // Calculate CRC32 of payload
    uint32_t crc = crc32_le(payload.data(), payload.size());
    
    // Build complete manifest
    std::vector<uint8_t> manifest;
    manifest.reserve(HEADER_SIZE + payload.size());
    
    // Header
    write_le32(manifest, MANIFEST_MAGIC);
    write_le16(manifest, MANIFEST_VERSION);
    write_le16(manifest, 0);  // reserved
    write_le32(manifest, static_cast<uint32_t>(HEADER_SIZE + payload.size()));  // total_size
    write_le32(manifest, crc);  // crc32
    
    // Payload
    manifest.insert(manifest.end(), payload.begin(), payload.end());
    
    return manifest;
}

} // namespace nah
