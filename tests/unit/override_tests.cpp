#include <doctest/doctest.h>
#include <nah/types.hpp>
#include <nah/host_profile.hpp>
#include <nah/warnings.hpp>
#include <nah/contract.hpp>

using namespace nah;

// ============================================================================
// Override Mode Tests (per SPEC L691-L717)
// ============================================================================

TEST_CASE("parse_override_mode parses valid modes") {
    CHECK(parse_override_mode("allow") == OverrideMode::Allow);
    CHECK(parse_override_mode("deny") == OverrideMode::Deny);
    CHECK(parse_override_mode("allowlist") == OverrideMode::Allowlist);
}

TEST_CASE("parse_override_mode is case-insensitive") {
    CHECK(parse_override_mode("ALLOW") == OverrideMode::Allow);
    CHECK(parse_override_mode("Deny") == OverrideMode::Deny);
    CHECK(parse_override_mode("ALLOWLIST") == OverrideMode::Allowlist);
}

TEST_CASE("parse_override_mode returns nullopt for invalid modes") {
    CHECK_FALSE(parse_override_mode("invalid").has_value());
    CHECK_FALSE(parse_override_mode("").has_value());
    CHECK_FALSE(parse_override_mode("permit").has_value());
}

TEST_CASE("override_mode_to_string returns correct strings") {
    CHECK(std::string(override_mode_to_string(OverrideMode::Allow)) == "allow");
    CHECK(std::string(override_mode_to_string(OverrideMode::Deny)) == "deny");
    CHECK(std::string(override_mode_to_string(OverrideMode::Allowlist)) == "allowlist");
}

// ============================================================================
// Override Policy Tests (per SPEC L701-L717)
// ============================================================================

TEST_CASE("override_denied warning has required fields") {
    // Per SPEC L1088: override_denied requires target, source_kind, source_ref
    auto fields = warnings::override_denied("NAH_OVERRIDE_ENVIRONMENT", "process_env", "NAH_OVERRIDE_ENVIRONMENT");
    
    CHECK(fields.count("target") == 1);
    CHECK(fields.count("source_kind") == 1);
    CHECK(fields.count("source_ref") == 1);
    CHECK(fields["target"] == "NAH_OVERRIDE_ENVIRONMENT");
    CHECK(fields["source_kind"] == "process_env");
}

TEST_CASE("override_invalid warning has required fields") {
    // Per SPEC L1089: override_invalid requires target, reason, source_kind, source_ref
    auto fields = warnings::override_invalid("NAH_OVERRIDE_ENVIRONMENT", "parse_failure", "process_env", "NAH_OVERRIDE_ENVIRONMENT");
    
    CHECK(fields.count("target") == 1);
    CHECK(fields.count("reason") == 1);
    CHECK(fields.count("source_kind") == 1);
    CHECK(fields.count("source_ref") == 1);
    CHECK(fields["reason"] == "parse_failure");
}

TEST_CASE("WarningCollector emits override_denied correctly") {
    WarningCollector collector;
    
    collector.emit(Warning::override_denied, 
                   warnings::override_denied("NAH_OVERRIDE_FOO", "process_env", "NAH_OVERRIDE_FOO"));
    
    auto w = collector.get_warnings();
    REQUIRE(w.size() == 1);
    CHECK(w[0].key == "override_denied");
    CHECK(w[0].fields.at("target") == "NAH_OVERRIDE_FOO");
}

TEST_CASE("WarningCollector emits override_invalid correctly") {
    WarningCollector collector;
    
    collector.emit(Warning::override_invalid,
                   warnings::override_invalid("NAH_OVERRIDE_ENVIRONMENT", "invalid_shape", "overrides_file", "/path/to/file.json"));
    
    auto w = collector.get_warnings();
    REQUIRE(w.size() == 1);
    CHECK(w[0].key == "override_invalid");
    CHECK(w[0].fields.at("reason") == "invalid_shape");
    CHECK(w[0].fields.at("source_kind") == "overrides_file");
}

// ============================================================================
// Overrides File Parsing Tests (per SPEC L903-L916)
// ============================================================================

TEST_CASE("parse_overrides_file accepts valid JSON with environment") {
    std::string json = R"({
        "environment": {
            "MY_VAR": "value1",
            "OTHER_VAR": "value2"
        }
    })";
    
    auto result = parse_overrides_file(json, "test.json");
    
    CHECK(result.ok);
    CHECK(result.overrides.environment.size() == 2);
    CHECK(result.overrides.environment["MY_VAR"] == "value1");
    CHECK(result.overrides.environment["OTHER_VAR"] == "value2");
}

TEST_CASE("parse_overrides_file accepts valid JSON with warnings") {
    std::string json = R"({
        "warnings": {
            "nak_not_found": "ignore",
            "profile_missing": "error"
        }
    })";
    
    auto result = parse_overrides_file(json, "test.json");
    
    CHECK(result.ok);
    CHECK(result.overrides.warnings.size() == 2);
    CHECK(result.overrides.warnings["nak_not_found"] == "ignore");
    CHECK(result.overrides.warnings["profile_missing"] == "error");
}

TEST_CASE("parse_overrides_file accepts valid JSON with both sections") {
    std::string json = R"({
        "environment": {"VAR": "val"},
        "warnings": {"nak_not_found": "ignore"}
    })";
    
    auto result = parse_overrides_file(json, "test.json");
    
    CHECK(result.ok);
    CHECK(result.overrides.environment.size() == 1);
    CHECK(result.overrides.warnings.size() == 1);
}

TEST_CASE("parse_overrides_file rejects JSON with invalid top-level keys") {
    // Per SPEC: "Any other top-level key/table... is invalid"
    std::string json = R"({
        "environment": {"VAR": "val"},
        "invalid_key": "value"
    })";
    
    auto result = parse_overrides_file(json, "test.json");
    
    CHECK_FALSE(result.ok);
    CHECK(result.error == "invalid_shape");
}

TEST_CASE("parse_overrides_file rejects JSON with non-object environment") {
    std::string json = R"({
        "environment": "not an object"
    })";
    
    auto result = parse_overrides_file(json, "test.json");
    
    CHECK_FALSE(result.ok);
    CHECK(result.error == "invalid_shape");
}

TEST_CASE("parse_overrides_file rejects JSON with non-string values") {
    std::string json = R"({
        "environment": {"VAR": 123}
    })";
    
    auto result = parse_overrides_file(json, "test.json");
    
    CHECK_FALSE(result.ok);
    CHECK(result.error == "invalid_shape");
}

TEST_CASE("parse_overrides_file rejects malformed JSON") {
    std::string json = "{ not valid json }";
    
    auto result = parse_overrides_file(json, "test.json");
    
    CHECK_FALSE(result.ok);
    CHECK(result.error == "parse_failure");
}

TEST_CASE("parse_overrides_file accepts valid JSON with all sections") {
    std::string json = R"({
        "environment": {
            "MY_VAR": "value1"
        },
        "warnings": {
            "nak_not_found": "ignore"
        }
    })";
    
    auto result = parse_overrides_file(json, "test.json");
    
    CHECK(result.ok);
    CHECK(result.overrides.environment["MY_VAR"] == "value1");
    CHECK(result.overrides.warnings["nak_not_found"] == "ignore");
}

TEST_CASE("parse_overrides_file rejects JSON with invalid top-level keys") {
    std::string json = R"({
        "environment": {
            "VAR": "val"
        },
        "invalid_section": {
            "foo": "bar"
        }
    })";
    
    auto result = parse_overrides_file(json, "test.json");
    
    CHECK_FALSE(result.ok);
    CHECK(result.error == "invalid_shape");
}

// ============================================================================
// Warning Override Tests (per SPEC L989-L1011)
// ============================================================================

TEST_CASE("WarningCollector apply_override changes action") {
    std::unordered_map<std::string, WarningAction> policy;
    policy["nak_not_found"] = WarningAction::Warn;
    
    WarningCollector collector(policy);
    
    // Apply override to change action
    collector.apply_override("nak_not_found", WarningAction::Error);
    
    collector.emit(Warning::nak_not_found);
    
    auto w = collector.get_warnings();
    REQUIRE(w.size() == 1);
    CHECK(w[0].key == "nak_not_found");
    CHECK(w[0].action == "error");
}

TEST_CASE("WarningCollector apply_override to ignore suppresses warning") {
    WarningCollector collector;
    
    collector.apply_override("profile_missing", WarningAction::Ignore);
    collector.emit(Warning::profile_missing);
    
    auto w = collector.get_warnings();
    CHECK(w.empty());  // Ignored warnings not in output
}
