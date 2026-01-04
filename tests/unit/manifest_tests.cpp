#include <doctest/doctest.h>
#include <nah/manifest.hpp>
#include <nah/manifest_builder.hpp>

#include <cstdint>
#include <algorithm>
#include <vector>

using nah::TLVEntry;
using nah::parse_manifest_blob;

namespace {

std::vector<uint8_t> encode_tlv(uint16_t tag, const std::string& value) {
    std::vector<uint8_t> out;
    out.push_back(tag & 0xFF);
    out.push_back((tag >> 8) & 0xFF);
    uint16_t len = static_cast<uint16_t>(value.size());
    out.push_back(len & 0xFF);
    out.push_back((len >> 8) & 0xFF);
    out.insert(out.end(), value.begin(), value.end());
    return out;
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

std::vector<uint8_t> build_manifest(const std::vector<TLVEntry>& entries,
                                    bool ascending = true,
                                    bool corrupt_crc = false,
                                    bool alter_total = false) {
    std::vector<uint8_t> payload;
    if (ascending) {
        for (const auto& e : entries) {
            auto enc = encode_tlv(e.tag, e.value);
            payload.insert(payload.end(), enc.begin(), enc.end());
        }
    } else {
        // preserve provided order (could be descending)
        for (const auto& e : entries) {
            auto enc = encode_tlv(e.tag, e.value);
            payload.insert(payload.end(), enc.begin(), enc.end());
        }
    }

    uint32_t crc = crc32_le(payload.data(), payload.size());
    if (corrupt_crc) crc ^= 0xFFFFFFFFu;

    uint32_t total_size = static_cast<uint32_t>(payload.size() + 16);
    if (alter_total) total_size -= 1;

    std::vector<uint8_t> blob;
    // magic "NAHM" little-endian 0x4D48414E
    uint32_t magic = 0x4D48414E;
    blob.push_back(magic & 0xFF);
    blob.push_back((magic >> 8) & 0xFF);
    blob.push_back((magic >> 16) & 0xFF);
    blob.push_back((magic >> 24) & 0xFF);
    // version = 1
    blob.push_back(1);
    blob.push_back(0);
    // reserved
    blob.push_back(0);
    blob.push_back(0);
    // total_size
    blob.push_back(total_size & 0xFF);
    blob.push_back((total_size >> 8) & 0xFF);
    blob.push_back((total_size >> 16) & 0xFF);
    blob.push_back((total_size >> 24) & 0xFF);
    // crc32
    blob.push_back(crc & 0xFF);
    blob.push_back((crc >> 8) & 0xFF);
    blob.push_back((crc >> 16) & 0xFF);
    blob.push_back((crc >> 24) & 0xFF);

    blob.insert(blob.end(), payload.begin(), payload.end());
    return blob;
}

} // namespace

TEST_CASE("Manifest parse succeeds with valid CRC and structure") {
    std::vector<TLVEntry> entries = {{10, "app"}, {11, "1.0.0"}, {12, "nak"}};
    auto blob = build_manifest(entries);
    auto res = parse_manifest_blob(blob);
    CHECK(res.ok);
    CHECK_FALSE(res.critical_missing);
    CHECK(res.entries.size() == 3);
    CHECK(res.warnings.empty());
}

TEST_CASE("Manifest parse tolerates structural invalidity with warnings") {
    std::vector<TLVEntry> entries = {{11, "1.0.0"}, {10, "app"}}; // descending order
    auto blob = build_manifest(entries, /*ascending=*/false);
    auto res = parse_manifest_blob(blob);
    CHECK(res.ok);
    CHECK_FALSE(res.critical_missing);
    CHECK(res.entries.size() == 1); // second ignored
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("Manifest parse treats CRC failure as critical missing") {
    std::vector<TLVEntry> entries = {{10, "app"}, {11, "1.0.0"}};
    auto blob = build_manifest(entries, /*ascending=*/true, /*corrupt_crc=*/true);
    auto res = parse_manifest_blob(blob);
    CHECK_FALSE(res.ok);
    CHECK(res.critical_missing);
    CHECK(res.entries.empty());
    CHECK(res.error == "crc_mismatch");
}

TEST_CASE("Manifest parse handles total_size mismatch as invalid manifest") {
    std::vector<TLVEntry> entries = {{10, "app"}};
    auto blob = build_manifest(entries, /*ascending=*/true, /*corrupt_crc=*/false,
                               /*alter_total=*/true);
    auto res = parse_manifest_blob(blob);
    CHECK(res.ok); // structural invalid only
    CHECK_FALSE(res.critical_missing);
    CHECK(res.entries.empty());
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("Manifest requires nak_id present and non-empty") {
    // Missing nak_id
    std::vector<TLVEntry> entries = {{10, "app"}, {11, "1.0.0"}};
    auto blob = build_manifest(entries);
    auto res = parse_manifest_blob(blob);
    CHECK(res.ok);
    CHECK_FALSE(res.critical_missing);
    REQUIRE_FALSE(res.warnings.empty());
    CHECK(res.warnings.back().find("nak_id_missing") != std::string::npos);

    // Empty nak_id
    std::vector<TLVEntry> entries2 = {{10, "app"}, {11, "1.0.0"}, {12, ""}};
    auto blob2 = build_manifest(entries2);
    auto res2 = parse_manifest_blob(blob2);
    CHECK(res2.ok);
    REQUIRE_FALSE(res2.warnings.empty());
    CHECK(res2.warnings.back().find("nak_id_missing") != std::string::npos);

    // Present nak_id
    std::vector<TLVEntry> entries3 = {{10, "app"}, {11, "1.0.0"}, {12, "nak"}};
    auto blob3 = build_manifest(entries3);
    auto res3 = parse_manifest_blob(blob3);
    CHECK(res3.ok);
    CHECK(std::none_of(res3.warnings.begin(), res3.warnings.end(), [](const std::string& w) {
        return w.find("nak_id_missing") != std::string::npos;
    }));
}

// ============================================================================
// ManifestBuilder Tests
// ============================================================================

TEST_CASE("ManifestBuilder produces valid binary manifest") {
    nah::ManifestBuilder builder;
    builder.id("com.example.app")
           .version("1.0.0")
           .nak_id("com.example.nak")
           .nak_version_req("^1.0.0")
           .entrypoint("bin/app");
    
    auto binary = builder.build();
    
    // Should have valid magic
    REQUIRE(binary.size() >= 16);
    uint32_t magic = static_cast<uint32_t>(binary[0]) | 
                     (static_cast<uint32_t>(binary[1]) << 8) | 
                     (static_cast<uint32_t>(binary[2]) << 16) | 
                     (static_cast<uint32_t>(binary[3]) << 24);
    CHECK(magic == 0x4D48414E);  // "NAHM"
    
    // Should parse back correctly
    auto result = nah::parse_manifest(binary);
    CHECK(result.ok);
    CHECK_FALSE(result.critical_missing);
    CHECK(result.manifest.id == "com.example.app");
    CHECK(result.manifest.version == "1.0.0");
    CHECK(result.manifest.nak_id == "com.example.nak");
    CHECK(result.manifest.entrypoint_path == "bin/app");
}

TEST_CASE("ManifestBuilder includes all fields") {
    nah::ManifestBuilder builder;
    builder.id("com.test.full")
           .version("2.0.0")
           .nak_id("com.test.nak")
           .nak_version_req("~1.5.0")
           .entrypoint("bin/main")
           .entrypoint_arg("--config")
           .entrypoint_arg("app.conf")
           .lib_dir("lib")
           .lib_dir("lib64")
           .asset_dir("assets")
           .asset_dir("share")
           .filesystem_permission("read:assets/*")
           .network_permission("connect:localhost:8080")
           .env("MY_VAR", "my_value")
           .description("Test application")
           .author("Test Author")
           .license("MIT")
           .homepage("https://example.com");
    
    auto binary = builder.build();
    auto result = nah::parse_manifest(binary);
    
    CHECK(result.ok);
    CHECK(result.manifest.id == "com.test.full");
    CHECK(result.manifest.version == "2.0.0");
    CHECK(result.manifest.nak_id == "com.test.nak");
    CHECK(result.manifest.entrypoint_path == "bin/main");
    
    // Check arrays
    REQUIRE(result.manifest.entrypoint_args.size() == 2);
    CHECK(result.manifest.entrypoint_args[0] == "--config");
    CHECK(result.manifest.entrypoint_args[1] == "app.conf");
    
    REQUIRE(result.manifest.lib_dirs.size() == 2);
    CHECK(result.manifest.lib_dirs[0] == "lib");
    CHECK(result.manifest.lib_dirs[1] == "lib64");
    
    REQUIRE(result.manifest.asset_dirs.size() == 2);
    CHECK(result.manifest.asset_dirs[0] == "assets");
    
    REQUIRE(result.manifest.permissions_filesystem.size() == 1);
    CHECK(result.manifest.permissions_filesystem[0] == "read:assets/*");
    
    REQUIRE(result.manifest.permissions_network.size() == 1);
    CHECK(result.manifest.permissions_network[0] == "connect:localhost:8080");
}

TEST_CASE("ManifestBuilder CRC is correct") {
    nah::ManifestBuilder builder;
    builder.id("com.crc.test").version("1.0.0");
    
    auto binary = builder.build();
    auto result = nah::parse_manifest(binary);
    
    // If CRC was wrong, parse would fail with critical_missing
    CHECK(result.ok);
    CHECK_FALSE(result.critical_missing);
}

TEST_CASE("ManifestBuilder entries are in ascending tag order") {
    // Build manifest with fields that would naturally be out of order
    nah::ManifestBuilder builder;
    builder.homepage("https://example.com")  // tag 63
           .id("com.order.test")             // tag 10
           .entrypoint("bin/app")            // tag 20
           .version("1.0.0");                // tag 11
    
    auto binary = builder.build();
    auto result = nah::parse_manifest(binary);
    
    // Parsing validates ascending order - if wrong, would get warnings
    CHECK(result.ok);
    CHECK(result.manifest.id == "com.order.test");
    CHECK(result.manifest.version == "1.0.0");
}
