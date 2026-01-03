#include <doctest/doctest.h>
#include <nah/warnings.hpp>
#include <nah/types.hpp>

using namespace nah;

TEST_CASE("warning_to_string returns correct warning key") {
    CHECK(std::string(warning_to_string(Warning::invalid_manifest)) == "invalid_manifest");
    CHECK(std::string(warning_to_string(Warning::profile_missing)) == "profile_missing");
    CHECK(std::string(warning_to_string(Warning::nak_not_found)) == "nak_not_found");
    CHECK(std::string(warning_to_string(Warning::missing_env_var)) == "missing_env_var");
}

TEST_CASE("parse_warning_key parses known warning keys") {
    CHECK(parse_warning_key("invalid_manifest") == Warning::invalid_manifest);
    CHECK(parse_warning_key("profile_missing") == Warning::profile_missing);
    CHECK(parse_warning_key("nak_not_found") == Warning::nak_not_found);
    CHECK(parse_warning_key("missing_env_var") == Warning::missing_env_var);
}

TEST_CASE("parse_warning_key returns nullopt for unknown keys") {
    CHECK_FALSE(parse_warning_key("unknown_warning").has_value());
    CHECK_FALSE(parse_warning_key("").has_value());
    CHECK_FALSE(parse_warning_key("not_a_warning").has_value());
}

TEST_CASE("WarningCollector default policy is warn") {
    WarningCollector collector;
    
    collector.emit_with_context(Warning::profile_missing, "test context");
    
    auto warnings = collector.get_warnings();
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].action == "warn");
    CHECK(warnings[0].key == "profile_missing");
}

TEST_CASE("WarningCollector applies error policy") {
    std::unordered_map<std::string, WarningAction> policy;
    policy["profile_missing"] = WarningAction::Error;
    
    WarningCollector collector(policy);
    collector.emit_with_context(Warning::profile_missing, "test context");
    
    auto warnings = collector.get_warnings();
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].action == "error");
}

TEST_CASE("WarningCollector applies ignore policy") {
    std::unordered_map<std::string, WarningAction> policy;
    policy["profile_missing"] = WarningAction::Ignore;
    
    WarningCollector collector(policy);
    collector.emit_with_context(Warning::profile_missing, "test context");
    
    auto warnings = collector.get_warnings();
    CHECK(warnings.empty());
}

TEST_CASE("WarningCollector has_errors detects error-level warnings") {
    std::unordered_map<std::string, WarningAction> policy;
    policy["nak_not_found"] = WarningAction::Error;
    
    WarningCollector collector(policy);
    
    CHECK_FALSE(collector.has_errors());
    
    collector.emit_with_context(Warning::profile_missing, "context1");  // warn
    CHECK_FALSE(collector.has_errors());
    
    collector.emit_with_context(Warning::nak_not_found, "context2");  // error
    CHECK(collector.has_errors());
}

TEST_CASE("WarningCollector has_effective_warnings detects warn-level warnings") {
    std::unordered_map<std::string, WarningAction> policy;
    policy["profile_missing"] = WarningAction::Ignore;
    
    WarningCollector collector(policy);
    
    CHECK_FALSE(collector.has_effective_warnings());
    
    collector.emit_with_context(Warning::profile_missing, "ignored");  // ignored
    CHECK_FALSE(collector.has_effective_warnings());
    
    collector.emit_with_context(Warning::nak_not_found, "warned");  // default = warn
    CHECK(collector.has_effective_warnings());
}

TEST_CASE("WarningCollector accumulates multiple warnings") {
    WarningCollector collector;
    
    collector.emit(Warning::profile_missing);
    collector.emit(Warning::nak_not_found);
    collector.emit(Warning::missing_env_var);
    
    auto warnings = collector.get_warnings();
    CHECK(warnings.size() == 3);
}

TEST_CASE("create_warning_fields produces correct map") {
    std::vector<WarningObject> warnings;
    WarningObject w1;
    w1.key = "profile_missing";
    w1.action = "warn";
    warnings.push_back(w1);
    
    WarningObject w2;
    w2.key = "nak_not_found";
    w2.action = "error";
    warnings.push_back(w2);
    
    auto fields = create_warning_fields(warnings);
    
    CHECK(fields.size() == 2);
    CHECK(fields.count("profile_missing") == 1);
    CHECK(fields.count("nak_not_found") == 1);
}
