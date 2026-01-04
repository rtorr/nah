#include <doctest/doctest.h>
#include <nah/manifest.hpp>

#include <vector>

using nah::TLVEntry;
using nah::parse_manifest;

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

std::vector<uint8_t> build_manifest(const std::vector<TLVEntry>& entries) {
    std::vector<uint8_t> payload;
    for (const auto& e : entries) {
        auto enc = encode_tlv(e.tag, e.value);
        payload.insert(payload.end(), enc.begin(), enc.end());
    }

    uint32_t crc = crc32_le(payload.data(), payload.size());
    uint32_t total_size = static_cast<uint32_t>(payload.size() + 16);

    std::vector<uint8_t> blob;
    uint32_t magic = 0x4D48414E;
    blob.push_back(magic & 0xFF);
    blob.push_back((magic >> 8) & 0xFF);
    blob.push_back((magic >> 16) & 0xFF);
    blob.push_back((magic >> 24) & 0xFF);
    blob.push_back(1);
    blob.push_back(0);
    blob.push_back(0);
    blob.push_back(0);
    blob.push_back(total_size & 0xFF);
    blob.push_back((total_size >> 8) & 0xFF);
    blob.push_back((total_size >> 16) & 0xFF);
    blob.push_back((total_size >> 24) & 0xFF);
    blob.push_back(crc & 0xFF);
    blob.push_back((crc >> 8) & 0xFF);
    blob.push_back((crc >> 16) & 0xFF);
    blob.push_back((crc >> 24) & 0xFF);
    blob.insert(blob.end(), payload.begin(), payload.end());
    return blob;
}

} // namespace

TEST_CASE("Manifest parses identity fields and requirement") {
    std::vector<TLVEntry> entries = {
        {10, "app.id"},
        {11, "1.2.3"},
        {12, "nak.id"},
        {13, "^3.1.0"},
        {20, "bin/app"},
    };
    auto blob = build_manifest(entries);
    auto res = parse_manifest(blob);
    CHECK(res.ok);
    CHECK_FALSE(res.critical_missing);
    CHECK(res.manifest.id == "app.id");
    CHECK(res.manifest.version == "1.2.3");
    CHECK(res.manifest.nak_id == "nak.id");
    CHECK(res.manifest.nak_version_req.has_value());
    CHECK(res.warnings.empty());
}

TEST_CASE("Manifest invalid requirement emits warning and drops constraint") {
    std::vector<TLVEntry> entries = {
        {10, "app.id"},
        {11, "1.2.3"},
        {12, "nak.id"},
        {13, "not-a-range"},
    };
    auto blob = build_manifest(entries);
    auto res = parse_manifest(blob);
    CHECK(res.ok);
    CHECK_FALSE(res.manifest.nak_version_req.has_value());
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("Manifest invalid version emits warning and clears version") {
    std::vector<TLVEntry> entries = {
        {10, "app.id"},
        {11, "1.2"}, // invalid
        {12, "nak.id"},
    };
    auto blob = build_manifest(entries);
    auto res = parse_manifest(blob);
    CHECK(res.ok);
    CHECK(res.manifest.version.empty());
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("Manifest missing entrypoint emits warning") {
    std::vector<TLVEntry> entries = {
        {10, "app.id"},
        {11, "1.2.3"},
        {12, "nak.id"},
    };
    auto blob = build_manifest(entries);
    auto res = parse_manifest(blob);
    CHECK(res.ok);
    CHECK(res.manifest.entrypoint_path.empty());
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("Manifest invalid entrypoint path is ignored with warning") {
    std::vector<TLVEntry> entries = {
        {10, "app.id"},
        {11, "1.2.3"},
        {12, "nak.id"},
        {20, "/abs/path"}, // invalid absolute
    };
    auto blob = build_manifest(entries);
    auto res = parse_manifest(blob);
    CHECK(res.ok);
    CHECK(res.manifest.entrypoint_path.empty());
    CHECK_FALSE(res.warnings.empty());
}
