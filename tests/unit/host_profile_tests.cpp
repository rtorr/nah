#include <doctest/doctest.h>
#include <nah/host_profile.hpp>

using namespace nah;

TEST_CASE("host profile valid binding_mode") {
    const char* json = R"({
        "nak": {
            "binding_mode": "canonical"
        }
    })";
    HostProfileRecord rec;
    auto v = parse_host_profile(json, rec);
    CHECK(v.ok);
    CHECK(rec.binding_mode == "canonical");
}

TEST_CASE("host profile missing binding_mode defaults to canonical") {
    // Per SPEC L620, binding_mode defaults to "canonical" when omitted
    const char* json = R"({
        "nak": {}
    })";
    HostProfileRecord rec;
    auto v = parse_host_profile(json, rec);
    CHECK(v.ok);
    CHECK(rec.binding_mode == "canonical");
}

TEST_CASE("host profile empty binding_mode defaults to canonical") {
    // Per SPEC L620, empty binding_mode should default to "canonical"
    const char* json = R"({
        "nak": {
            "binding_mode": ""
        }
    })";
    HostProfileRecord rec;
    auto v = parse_host_profile(json, rec);
    CHECK(v.ok);
    CHECK(rec.binding_mode == "canonical");
}

// ============================================================================
// Full Profile Parsing Tests (SPEC L638-660)
// ============================================================================

TEST_CASE("host profile parses allow_versions patterns") {
    const char* json = R"({
        "nak": {
            "binding_mode": "canonical",
            "allow_versions": ["1.*", "2.0.*"]
        }
    })";
    auto result = parse_host_profile_full(json, "test.json");
    CHECK(result.ok);
    REQUIRE(result.profile.nak.allow_versions.size() == 2);
    CHECK(result.profile.nak.allow_versions[0] == "1.*");
    CHECK(result.profile.nak.allow_versions[1] == "2.0.*");
}

TEST_CASE("host profile parses deny_versions patterns") {
    const char* json = R"({
        "nak": {
            "binding_mode": "canonical",
            "deny_versions": ["0.*", "1.0.0"]
        }
    })";
    auto result = parse_host_profile_full(json, "test.json");
    CHECK(result.ok);
    REQUIRE(result.profile.nak.deny_versions.size() == 2);
    CHECK(result.profile.nak.deny_versions[0] == "0.*");
    CHECK(result.profile.nak.deny_versions[1] == "1.0.0");
}

TEST_CASE("host profile parses environment section") {
    const char* json = R"({
        "nak": {
            "binding_mode": "canonical"
        },
        "environment": {
            "MY_VAR": "my_value",
            "OTHER_VAR": "other_value"
        }
    })";
    auto result = parse_host_profile_full(json, "test.json");
    CHECK(result.ok);
    CHECK(result.profile.environment.at("MY_VAR").value == "my_value");
    CHECK(result.profile.environment.at("OTHER_VAR").value == "other_value");
}

TEST_CASE("host profile parses paths section") {
    const char* json = R"({
        "nak": {
            "binding_mode": "canonical"
        },
        "paths": {
            "library_prepend": ["/opt/lib", "/usr/local/lib"],
            "library_append": ["/lib/fallback"]
        }
    })";
    auto result = parse_host_profile_full(json, "test.json");
    CHECK(result.ok);
    REQUIRE(result.profile.paths.library_prepend.size() == 2);
    CHECK(result.profile.paths.library_prepend[0] == "/opt/lib");
    CHECK(result.profile.paths.library_append.size() == 1);
    CHECK(result.profile.paths.library_append[0] == "/lib/fallback");
}

TEST_CASE("host profile binding_mode mapped parses correctly") {
    const char* json = R"({
        "nak": {
            "binding_mode": "mapped",
            "map": {
                "com.example.nak:^1.0.0": "com.example.nak@1.0.5"
            }
        }
    })";
    auto result = parse_host_profile_full(json, "test.json");
    CHECK(result.ok);
    CHECK(result.profile.nak.binding_mode == BindingMode::Mapped);
    CHECK(result.profile.nak.map.count("com.example.nak:^1.0.0") > 0);
}
