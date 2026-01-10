#include <doctest/doctest.h>
#include <nah/host_profile.hpp>
#include <nah/types.hpp>
#include <nah/warnings.hpp>

using namespace nah;

const char* VALID_PROFILE = R"({
    "$schema": "nah.host.profile.v2",
    "nak": {
        "binding_mode": "canonical"
    },
    "environment": {
        "TEST_VAR": "test_value"
    }
})";

const char* DEVELOPMENT_PROFILE = R"({
    "$schema": "nah.host.profile.v2",
    "nak": {
        "binding_mode": "mapped"
    },
    "environment": {
        "NAH_MODE": "development"
    }
})";

// ============================================================================
// Active Host Profile Resolution Tests (per SPEC L597-L612)
// ============================================================================

TEST_CASE("profile resolution: valid profile parses successfully") {
    // Per SPEC L599: Load profile from JSON
    auto result = parse_host_profile_full(VALID_PROFILE, "/test/profile.json");
    
    CHECK(result.ok);
    CHECK(result.profile.schema == "nah.host.profile.v2");
    CHECK(result.profile.nak.binding_mode == BindingMode::Canonical);
}

TEST_CASE("profile resolution: profile with mapped mode parses correctly") {
    auto result = parse_host_profile_full(DEVELOPMENT_PROFILE, "/test/dev.json");
    
    CHECK(result.ok);
    CHECK(result.profile.nak.binding_mode == BindingMode::Mapped);
    CHECK(result.profile.environment["NAH_MODE"] == "development");
}

TEST_CASE("profile resolution: missing schema fails") {
    // Per SPEC L605-L606
    const char* no_schema = R"({
        "nak": {
            "binding_mode": "canonical"
        }
    })";
    
    auto result = parse_host_profile_full(no_schema);
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("schema") != std::string::npos);
}

TEST_CASE("profile resolution: parse error returns error") {
    // Per SPEC L607-L608
    const char* invalid_json = "this is not valid JSON { [ }";
    
    auto result = parse_host_profile_full(invalid_json);
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("parse") != std::string::npos);
}

TEST_CASE("profile resolution: schema mismatch fails") {
    // Per SPEC L609-L610
    const char* wrong_schema = R"({
        "$schema": "nah.host.profile.v1",
        "nak": {
            "binding_mode": "canonical"
        }
    })";
    
    auto result = parse_host_profile_full(wrong_schema);
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("schema") != std::string::npos);
}

// ============================================================================
// Built-in Empty Profile Tests (per SPEC L614-L630)
// ============================================================================

TEST_CASE("built-in empty profile has correct schema") {
    // Per SPEC L617
    HostProfile empty = get_builtin_empty_profile();
    CHECK(empty.schema == "nah.host.profile.v2");
}

TEST_CASE("built-in empty profile has canonical binding_mode") {
    // Per SPEC L620
    HostProfile empty = get_builtin_empty_profile();
    CHECK(empty.nak.binding_mode == BindingMode::Canonical);
}

TEST_CASE("built-in empty profile has default warning actions") {
    // Per SPEC L622-L628: default warnings
    HostProfile empty = get_builtin_empty_profile();
    
    // Specific defaults from SPEC
    CHECK(empty.warnings.count("nak_not_found") == 1);
    CHECK(empty.warnings.at("nak_not_found") == WarningAction::Warn);
    CHECK(empty.warnings.count("nak_version_unsupported") == 1);
    CHECK(empty.warnings.at("nak_version_unsupported") == WarningAction::Warn);
    CHECK(empty.warnings.count("profile_missing") == 1);
    CHECK(empty.warnings.at("profile_missing") == WarningAction::Warn);
}

// ============================================================================
// Binding Mode Tests (per SPEC L637-L652)
// ============================================================================

TEST_CASE("parse_binding_mode parses valid modes") {
    CHECK(parse_binding_mode("canonical") == BindingMode::Canonical);
    CHECK(parse_binding_mode("mapped") == BindingMode::Mapped);
}

TEST_CASE("parse_binding_mode is case-insensitive") {
    CHECK(parse_binding_mode("CANONICAL") == BindingMode::Canonical);
    CHECK(parse_binding_mode("Mapped") == BindingMode::Mapped);
}

TEST_CASE("parse_binding_mode returns nullopt for invalid modes") {
    CHECK_FALSE(parse_binding_mode("invalid").has_value());
    CHECK_FALSE(parse_binding_mode("").has_value());
    CHECK_FALSE(parse_binding_mode("direct").has_value());
}

TEST_CASE("binding_mode_to_string returns correct strings") {
    CHECK(std::string(binding_mode_to_string(BindingMode::Canonical)) == "canonical");
    CHECK(std::string(binding_mode_to_string(BindingMode::Mapped)) == "mapped");
}

// ============================================================================
// Host Profile Parsing with All Fields (per SPEC L632-L693)
// ============================================================================

TEST_CASE("host profile parses all sections") {
    const char* full_profile = R"({
        "$schema": "nah.host.profile.v2",
        "nak": {
            "binding_mode": "mapped",
            "allow_versions": ["3.*"],
            "deny_versions": ["3.0.0"],
            "map": {
                "3.0": "com.example.nak@3.0.7.json",
                "3.1": "com.example.nak@3.1.2.json"
            }
        },
        "environment": {
            "NAH_HOST_VERSION": "1.0",
            "NAH_MODE": "production"
        },
        "warnings": {
            "nak_not_found": "error",
            "profile_missing": "ignore"
        },
        "capabilities": {
            "filesystem.read": "sandbox.readonly"
        },
        "overrides": {
            "mode": "allowlist",
            "allow_keys": ["ENVIRONMENT", "WARNINGS_*"]
        }
    })";
    
    auto result = parse_host_profile_full(full_profile);
    
    CHECK(result.ok);
    CHECK(result.profile.schema == "nah.host.profile.v2");
    CHECK(result.profile.nak.binding_mode == BindingMode::Mapped);
    CHECK(result.profile.nak.allow_versions.size() == 1);
    CHECK(result.profile.nak.allow_versions[0] == "3.*");
    CHECK(result.profile.nak.deny_versions.size() == 1);
    CHECK(result.profile.nak.deny_versions[0] == "3.0.0");
    CHECK(result.profile.nak.map.size() == 2);
    CHECK(result.profile.nak.map["3.0"] == "com.example.nak@3.0.7.json");
    CHECK(result.profile.environment.size() == 2);
    CHECK(result.profile.environment["NAH_HOST_VERSION"] == "1.0");
    CHECK(result.profile.warnings.size() == 2);
    CHECK(result.profile.warnings["nak_not_found"] == WarningAction::Error);
    CHECK(result.profile.capabilities.size() == 1);
    CHECK(result.profile.capabilities["filesystem.read"] == "sandbox.readonly");
    CHECK(result.profile.overrides.mode == OverrideMode::Allowlist);
    CHECK(result.profile.overrides.allow_keys.size() == 2);
}

// ============================================================================
// Default Warning Action Tests (per SPEC L630)
// ============================================================================

TEST_CASE("missing warning action defaults to warn") {
    // Per SPEC L630: "If a warning key is absent from profile.warnings, effective action MUST be 'warn'"
    std::unordered_map<std::string, WarningAction> policy;
    // policy is empty - no specific actions set
    
    WarningCollector collector(policy);
    
    // Emit a warning not in policy
    collector.emit(Warning::capability_missing);
    
    auto w = collector.get_warnings();
    REQUIRE(w.size() == 1);
    CHECK(w[0].action == "warn");  // Default
}
