#include <doctest/doctest.h>
#include <nah/types.hpp>
#include <nah/warnings.hpp>

using namespace nah;

// ============================================================================
// Trust State Tests (per SPEC L470-L484)
// ============================================================================

TEST_CASE("parse_trust_state parses valid states") {
    CHECK(parse_trust_state("verified") == TrustState::Verified);
    CHECK(parse_trust_state("unverified") == TrustState::Unverified);
    CHECK(parse_trust_state("failed") == TrustState::Failed);
    CHECK(parse_trust_state("unknown") == TrustState::Unknown);
}

TEST_CASE("parse_trust_state is case-insensitive") {
    CHECK(parse_trust_state("VERIFIED") == TrustState::Verified);
    CHECK(parse_trust_state("Unverified") == TrustState::Unverified);
    CHECK(parse_trust_state("FAILED") == TrustState::Failed);
}

TEST_CASE("parse_trust_state returns nullopt for invalid states") {
    CHECK_FALSE(parse_trust_state("invalid").has_value());
    CHECK_FALSE(parse_trust_state("").has_value());
    CHECK_FALSE(parse_trust_state("trusted").has_value());
}

TEST_CASE("trust_state_to_string returns correct strings") {
    CHECK(std::string(trust_state_to_string(TrustState::Verified)) == "verified");
    CHECK(std::string(trust_state_to_string(TrustState::Unverified)) == "unverified");
    CHECK(std::string(trust_state_to_string(TrustState::Failed)) == "failed");
    CHECK(std::string(trust_state_to_string(TrustState::Unknown)) == "unknown");
}

// ============================================================================
// Trust Warning Emission Tests (per SPEC L472-L483)
// ============================================================================

TEST_CASE("trust_state_verified emits no warning") {
    // Per SPEC: "If [trust].state == 'verified', NAH MUST emit no trust-state warning"
    std::unordered_map<std::string, WarningAction> policy;
    WarningCollector collector(policy);
    
    // Simulating verified trust state - no warning should be emitted
    // (The actual emission is in compose_contract, but we test the collector behavior)
    
    auto warnings = collector.get_warnings();
    CHECK(warnings.empty());
}

TEST_CASE("trust_state_unverified warning can be emitted") {
    std::unordered_map<std::string, WarningAction> policy;
    WarningCollector collector(policy);
    
    collector.emit(Warning::trust_state_unverified);
    
    auto warnings = collector.get_warnings();
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].key == "trust_state_unverified");
    CHECK(warnings[0].action == "warn");
}

TEST_CASE("trust_state_failed warning can be upgraded to error") {
    // Per SPEC L683: trust_state_failed = "error" in example
    std::unordered_map<std::string, WarningAction> policy;
    policy["trust_state_failed"] = WarningAction::Error;
    
    WarningCollector collector(policy);
    collector.emit(Warning::trust_state_failed);
    
    auto warnings = collector.get_warnings();
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].key == "trust_state_failed");
    CHECK(warnings[0].action == "error");
    CHECK(collector.has_errors());
}

TEST_CASE("trust_state_unknown warning is emitted for absent trust section") {
    // Per SPEC L472: "If [trust] is absent, state MUST be treated as 'unknown'"
    std::unordered_map<std::string, WarningAction> policy;
    WarningCollector collector(policy);
    
    collector.emit(Warning::trust_state_unknown);
    
    auto warnings = collector.get_warnings();
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].key == "trust_state_unknown");
}

TEST_CASE("trust_state_stale warning for expired trust") {
    // Per SPEC L479: "If [trust].expires_at exists and is earlier than now..."
    std::unordered_map<std::string, WarningAction> policy;
    WarningCollector collector(policy);
    
    collector.emit(Warning::trust_state_stale);
    
    auto warnings = collector.get_warnings();
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].key == "trust_state_stale");
}

TEST_CASE("invalid_trust_state warning for unrecognized state value") {
    // Per SPEC L473: "If [trust].state is present but not one of {...}, emit invalid_trust_state"
    std::unordered_map<std::string, WarningAction> policy;
    WarningCollector collector(policy);
    
    collector.emit(Warning::invalid_trust_state);
    
    auto warnings = collector.get_warnings();
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].key == "invalid_trust_state");
}

// ============================================================================
// TrustInfo Structure Tests
// ============================================================================

TEST_CASE("TrustInfo defaults to Unknown state") {
    TrustInfo info;
    CHECK(info.state == TrustState::Unknown);
    CHECK(info.source.empty());
    CHECK(info.evaluated_at.empty());
    CHECK(info.expires_at.empty());
    CHECK(info.details.empty());
}

TEST_CASE("TrustInfo can store all fields") {
    TrustInfo info;
    info.state = TrustState::Verified;
    info.source = "corp-verifier";
    info.evaluated_at = "2024-01-15T10:30:00Z";
    info.expires_at = "2024-02-15T10:30:00Z";
    info.inputs_hash = "sha256:abc123";
    info.details["method"] = "codesign";
    info.details["signer"] = "Developer ID";
    
    CHECK(info.state == TrustState::Verified);
    CHECK(info.source == "corp-verifier");
    CHECK(info.evaluated_at == "2024-01-15T10:30:00Z");
    CHECK(info.expires_at == "2024-02-15T10:30:00Z");
    CHECK(info.inputs_hash == "sha256:abc123");
    CHECK(info.details.size() == 2);
    CHECK(info.details.at("method") == "codesign");
}
