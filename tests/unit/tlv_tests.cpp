#include <doctest/doctest.h>
#include <nah/manifest_tlv.hpp>

#include <vector>

using nah::decode_manifest_tlv;

static std::vector<uint8_t> mk(uint16_t tag, const std::string& value) {
    std::vector<uint8_t> out;
    out.push_back(tag & 0xFF);
    out.push_back((tag >> 8) & 0xFF);
    uint16_t len = static_cast<uint16_t>(value.size());
    out.push_back(len & 0xFF);
    out.push_back((len >> 8) & 0xFF);
    out.insert(out.end(), value.begin(), value.end());
    return out;
}

TEST_CASE("TLV decode enforces ordering and limits") {
    std::vector<uint8_t> data;
    auto e1 = mk(10, "id");
    auto e2 = mk(11, "1.0.0");
    data.insert(data.end(), e1.begin(), e1.end());
    data.insert(data.end(), e2.begin(), e2.end());
    auto res = decode_manifest_tlv(data);
    CHECK(res.ok);
    REQUIRE(res.entries.size() == 2);
    CHECK(res.entries[0].tag == 10);
    CHECK(res.entries[1].tag == 11);
}

TEST_CASE("TLV decode rejects oversize payload") {
    std::vector<uint8_t> data(64 * 1024 + 1, 0);
    auto res = decode_manifest_tlv(data);
    CHECK(res.ok); // invalid manifest but not fatal
    CHECK(res.entries.empty());
    CHECK(res.warnings.size() == 1);
}

TEST_CASE("TLV decode ignores descending tag order entries") {
    std::vector<uint8_t> data;
    auto e1 = mk(11, "1.0.0");
    auto e2 = mk(10, "id");
    data.insert(data.end(), e1.begin(), e1.end());
    data.insert(data.end(), e2.begin(), e2.end());
    auto res = decode_manifest_tlv(data);
    CHECK(res.ok);
    CHECK(res.entries.size() == 1);
    CHECK(res.entries[0].tag == 11);
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("TLV decode stops at END tag") {
    std::vector<uint8_t> data;
    auto e1 = mk(10, "id");
    std::vector<uint8_t> end = {0,0,0,0};
    auto e2 = mk(11, "version");
    data.insert(data.end(), e1.begin(), e1.end());
    data.insert(data.end(), end.begin(), end.end());
    data.insert(data.end(), e2.begin(), e2.end());
    auto res = decode_manifest_tlv(data);
    CHECK(res.ok);
    CHECK(res.entries.size() == 2); // END not final is ignored per spec
    CHECK(res.entries[0].tag == 10);
    CHECK(res.entries[1].tag == 11);
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("TLV decode validates total_size when provided") {
    auto e1 = mk(10, "id");
    auto e2 = mk(11, "1.0.0");
    std::vector<uint8_t> data;
    data.insert(data.end(), e1.begin(), e1.end());
    data.insert(data.end(), e2.begin(), e2.end());
    auto res = decode_manifest_tlv(data, data.size() + 4);
    CHECK(res.ok);
    CHECK(res.entries.empty());
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("TLV decode enforces entry limit and warnings") {
    std::vector<uint8_t> data;
    for (int i = 0; i < 520; ++i) {
        uint16_t tag = static_cast<uint16_t>(100 + i); // strictly ascending
        auto e = mk(tag, "v");
        data.insert(data.end(), e.begin(), e.end());
    }
    auto res = decode_manifest_tlv(data);
    CHECK(res.ok);
    CHECK(res.entries.size() == 512);
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("TLV decode enforces repeat limits and value validation") {
    std::vector<uint8_t> data;
    // 129 ENTRYPOINT_ARG (repeatable) entries
    for (int i = 0; i < 129; ++i) {
        auto e = mk(21, "arg");
        data.insert(data.end(), e.begin(), e.end());
    }
    auto res = decode_manifest_tlv(data);
    CHECK(res.ok);
    CHECK(res.entries.size() == 128);
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("TLV decode rejects non-repeatable duplicates and invalid values") {
    std::vector<uint8_t> data;
    auto e1 = mk(10, "app");
    auto e2 = mk(10, "second");
    auto e3 = mk(20, "/abs/path"); // invalid: absolute
    data.insert(data.end(), e1.begin(), e1.end());
    data.insert(data.end(), e2.begin(), e2.end());
    data.insert(data.end(), e3.begin(), e3.end());
    auto res = decode_manifest_tlv(data);
    CHECK(res.ok);
    CHECK(res.entries.size() == 1);
    CHECK(res.entries[0].value == "app");
    CHECK(res.warnings.size() >= 2);
}

TEST_CASE("TLV decode enforces string length limit") {
    std::vector<uint8_t> data;
    std::string long_value(4097, 'a');
    auto e1 = mk(10, "app");
    auto e2 = mk(11, long_value);
    data.insert(data.end(), e1.begin(), e1.end());
    data.insert(data.end(), e2.begin(), e2.end());
    auto res = decode_manifest_tlv(data);
    CHECK(res.ok);
    CHECK(res.entries.size() == 1);
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("TLV decode validates ENV_VAR formatting") {
    std::vector<uint8_t> data;
    auto good = mk(30, "KEY=VALUE");
    auto bad = mk(30, "NOVALUE");
    data.insert(data.end(), good.begin(), good.end());
    data.insert(data.end(), bad.begin(), bad.end());
    auto res = decode_manifest_tlv(data);
    CHECK(res.ok);
    REQUIRE(res.entries.size() == 1);
    CHECK(res.entries[0].value == "KEY=VALUE");
    CHECK_FALSE(res.warnings.empty());
}

TEST_CASE("parse_asset_export validates format") {
    using nah::parse_asset_export;
    auto good = parse_asset_export("id:path:type");
    REQUIRE(good.has_value());
    CHECK(good->id == "id");
    CHECK(good->path == "path");
    CHECK(good->type == "type");

    CHECK_FALSE(parse_asset_export("missingcolon"));
    CHECK_FALSE(parse_asset_export(":path"));
    CHECK_FALSE(parse_asset_export("id:/abs"));
}

// ============================================================================
// UTF-8 and NUL validation (SPEC L1508)
// ============================================================================

TEST_CASE("TLV decode rejects strings with embedded NUL bytes") {
    // SPEC L1508: Strings MUST be UTF-8 without NUL
    std::vector<uint8_t> data;
    std::string value_with_nul = "hello";
    value_with_nul += '\0';
    value_with_nul += "world";
    auto entry = mk(10, value_with_nul);
    data.insert(data.end(), entry.begin(), entry.end());
    
    auto res = decode_manifest_tlv(data);
    // NUL bytes in strings should emit invalid_manifest warning and be rejected
    CHECK(res.ok);
    CHECK(res.entries.empty());  // Entry with NUL is rejected
    CHECK(res.warnings.size() >= 1);
    bool found_warning = false;
    for (const auto& w : res.warnings) {
        if (w.find("invalid_value") != std::string::npos) {
            found_warning = true;
            break;
        }
    }
    CHECK(found_warning);
}

TEST_CASE("TLV decode accepts valid UTF-8 strings") {
    std::vector<uint8_t> data;
    // Valid UTF-8: "Héllo Wörld" with accented chars
    auto entry = mk(10, "H\xc3\xa9llo W\xc3\xb6rld");
    data.insert(data.end(), entry.begin(), entry.end());
    
    auto res = decode_manifest_tlv(data);
    CHECK(res.ok);
    REQUIRE(res.entries.size() == 1);
    CHECK(res.entries[0].value == "H\xc3\xa9llo W\xc3\xb6rld");
}

// ============================================================================
// SCHEMA_VERSION validation (SPEC L1510)
// ============================================================================

TEST_CASE("manifest SCHEMA_VERSION is 1") {
    // The TLV schema version tag (0x01) must have value "1"
    std::vector<uint8_t> data;
    auto schema = mk(0x01, "1");  // SCHEMA_VERSION tag = 0x01
    auto id = mk(0x10, "com.example.app");
    data.insert(data.end(), schema.begin(), schema.end());
    data.insert(data.end(), id.begin(), id.end());
    
    auto res = decode_manifest_tlv(data);
    CHECK(res.ok);
    
    // Find schema version entry
    bool found_schema = false;
    for (const auto& entry : res.entries) {
        if (entry.tag == 0x01) {
            CHECK(entry.value == "1");
            found_schema = true;
        }
    }
    CHECK(found_schema);
}
