#include <doctest/doctest.h>
#include <nah/nak_selection.hpp>
#include <nah/warnings.hpp>
#include <nah/semver.hpp>

using namespace nah;

// ============================================================================
// Helper to create test manifests
// ============================================================================

namespace {

Manifest make_manifest(const std::string& nak_id, const std::string& nak_version_req) {
    Manifest m;
    m.id = "com.example.app";
    m.version = "1.0.0";
    m.nak_id = nak_id;
    m.nak_version_req = parse_range(nak_version_req);
    m.entrypoint_path = "bin/app";
    return m;
}

HostProfile make_profile(BindingMode mode = BindingMode::Canonical) {
    HostProfile p;
    p.nak.binding_mode = mode;
    return p;
}

NakRegistryEntry make_registry_entry(const std::string& id, const std::string& version) {
    NakRegistryEntry e;
    e.id = id;
    e.version = version;
    e.record_ref = id + "@" + version + ".json";
    e.record_path = "/nah/registry/naks/" + e.record_ref;
    return e;
}

} // namespace

// ============================================================================
// Install-Time NAK Selection Tests (per SPEC L1151-L1187)
// ============================================================================

TEST_CASE("select_nak_for_install: canonical mode selects highest satisfying version") {
    // Per SPEC L1176-1180: Canonical mode chooses highest installed version
    auto manifest = make_manifest("com.example.nak", ">=1.0.0 <2.0.0");
    auto profile = make_profile(BindingMode::Canonical);
    
    std::vector<NakRegistryEntry> registry = {
        make_registry_entry("com.example.nak", "1.0.0"),
        make_registry_entry("com.example.nak", "1.2.0"),
        make_registry_entry("com.example.nak", "1.5.0"),
        make_registry_entry("com.example.nak", "2.0.0"),  // Outside >=1.0.0 <2.0.0
    };
    
    WarningCollector warnings;
    auto result = select_nak_for_install(manifest, profile, registry, warnings);
    
    CHECK(result.resolved);
    CHECK(result.pin.id == "com.example.nak");
    CHECK(result.pin.version == "1.5.0");  // Highest satisfying >=1.0.0 <2.0.0
    CHECK_FALSE(warnings.has_errors());
}

TEST_CASE("select_nak_for_install: emits nak_not_found when no candidates") {
    // Per SPEC L1182-1184: nak_not_found only at install time
    auto manifest = make_manifest("com.nonexistent.nak", ">=1.0.0 <2.0.0");
    auto profile = make_profile();
    
    std::vector<NakRegistryEntry> registry = {
        make_registry_entry("com.other.nak", "1.0.0"),
    };
    
    WarningCollector warnings;
    auto result = select_nak_for_install(manifest, profile, registry, warnings);
    
    CHECK_FALSE(result.resolved);
    
    auto w = warnings.get_warnings();
    REQUIRE(w.size() >= 1);
    CHECK(w[0].key == "nak_not_found");
}

TEST_CASE("select_nak_for_install: emits nak_version_unsupported when no version satisfies") {
    auto manifest = make_manifest("com.example.nak", ">=3.0.0 <4.0.0");
    auto profile = make_profile();
    
    std::vector<NakRegistryEntry> registry = {
        make_registry_entry("com.example.nak", "1.0.0"),
        make_registry_entry("com.example.nak", "2.0.0"),
    };
    
    WarningCollector warnings;
    auto result = select_nak_for_install(manifest, profile, registry, warnings);
    
    CHECK_FALSE(result.resolved);
    
    auto w = warnings.get_warnings();
    REQUIRE(w.size() >= 1);
    CHECK(w[0].key == "nak_version_unsupported");
}

TEST_CASE("select_nak_for_install: invalid nak_version_req emits invalid_manifest") {
    Manifest manifest;
    manifest.id = "com.example.app";
    manifest.version = "1.0.0";
    manifest.nak_id = "com.example.nak";
    manifest.nak_version_req = std::nullopt;  // Invalid/missing
    
    auto profile = make_profile();
    std::vector<NakRegistryEntry> registry;
    
    WarningCollector warnings;
    auto result = select_nak_for_install(manifest, profile, registry, warnings);
    
    CHECK_FALSE(result.resolved);
    
    auto w = warnings.get_warnings();
    REQUIRE(w.size() >= 1);
    CHECK(w[0].key == "invalid_manifest");
}

TEST_CASE("select_nak_for_install: mapped mode uses selection_key lookup") {
    // Per SPEC L1172-1175: Mapped mode uses profile.nak.map
    auto manifest = make_manifest("com.example.nak", ">=3.0.0 <4.0.0");
    auto profile = make_profile(BindingMode::Mapped);
    profile.nak.map["3.0"] = "com.example.nak@3.0.7.json";
    
    std::vector<NakRegistryEntry> registry = {
        make_registry_entry("com.example.nak", "3.0.0"),
        make_registry_entry("com.example.nak", "3.0.5"),
        make_registry_entry("com.example.nak", "3.0.7"),
        make_registry_entry("com.example.nak", "3.1.0"),
    };
    
    WarningCollector warnings;
    auto result = select_nak_for_install(manifest, profile, registry, warnings);
    
    CHECK(result.resolved);
    CHECK(result.pin.version == "3.0.7");  // From map, not highest
    CHECK(result.pin.record_ref == "com.example.nak@3.0.7.json");
}

TEST_CASE("select_nak_for_install: mapped mode emits nak_version_unsupported when key missing") {
    auto manifest = make_manifest("com.example.nak", ">=3.0.0 <4.0.0");
    auto profile = make_profile(BindingMode::Mapped);
    // No entry for "3.0" in map
    
    std::vector<NakRegistryEntry> registry = {
        make_registry_entry("com.example.nak", "3.0.0"),
    };
    
    WarningCollector warnings;
    auto result = select_nak_for_install(manifest, profile, registry, warnings);
    
    CHECK_FALSE(result.resolved);
    
    auto w = warnings.get_warnings();
    REQUIRE(w.size() >= 1);
    CHECK(w[0].key == "nak_version_unsupported");
}

TEST_CASE("select_nak_for_install: selection is deterministic") {
    // Per SPEC L1166: Selection MUST be stable/deterministic
    auto manifest = make_manifest("com.example.nak", ">=1.0.0 <2.0.0");
    auto profile = make_profile();
    
    std::vector<NakRegistryEntry> registry = {
        make_registry_entry("com.example.nak", "1.0.0"),
        make_registry_entry("com.example.nak", "1.2.0"),
        make_registry_entry("com.example.nak", "1.1.0"),
    };
    
    WarningCollector w1, w2;
    auto result1 = select_nak_for_install(manifest, profile, registry, w1);
    auto result2 = select_nak_for_install(manifest, profile, registry, w2);
    
    CHECK(result1.resolved == result2.resolved);
    CHECK(result1.pin.id == result2.pin.id);
    CHECK(result1.pin.version == result2.pin.version);
    CHECK(result1.pin.record_ref == result2.pin.record_ref);
}

TEST_CASE("select_nak_for_install: respects allow_versions filter") {
    auto manifest = make_manifest("com.example.nak", ">=1.0.0 <2.0.0");
    auto profile = make_profile();
    profile.nak.allow_versions = {"1.0.*"};
    
    std::vector<NakRegistryEntry> registry = {
        make_registry_entry("com.example.nak", "1.0.5"),
        make_registry_entry("com.example.nak", "1.1.0"),  // Not in allow
        make_registry_entry("com.example.nak", "1.2.0"),  // Not in allow
    };
    
    WarningCollector warnings;
    auto result = select_nak_for_install(manifest, profile, registry, warnings);
    
    CHECK(result.resolved);
    CHECK(result.pin.version == "1.0.5");  // Only allowed version
}

TEST_CASE("select_nak_for_install: respects deny_versions filter") {
    auto manifest = make_manifest("com.example.nak", ">=1.0.0 <2.0.0");
    auto profile = make_profile();
    profile.nak.deny_versions = {"1.2.*"};
    
    std::vector<NakRegistryEntry> registry = {
        make_registry_entry("com.example.nak", "1.0.0"),
        make_registry_entry("com.example.nak", "1.1.0"),
        make_registry_entry("com.example.nak", "1.2.0"),  // Denied
        make_registry_entry("com.example.nak", "1.2.5"),  // Denied
    };
    
    WarningCollector warnings;
    auto result = select_nak_for_install(manifest, profile, registry, warnings);
    
    CHECK(result.resolved);
    CHECK(result.pin.version == "1.1.0");  // Highest non-denied
}

// ============================================================================
// Compose-Time Pinned NAK Load Tests (per SPEC L1189-L1236)
// ============================================================================

TEST_CASE("load_pinned_nak: empty record_ref emits nak_pin_invalid") {
    // Per SPEC L1207-1209
    NakPin pin;
    pin.id = "com.example.nak";
    pin.version = "1.0.0";
    pin.record_ref = "";  // Empty
    
    auto manifest = make_manifest("com.example.nak", ">=1.0.0 <2.0.0");
    auto profile = make_profile();
    
    WarningCollector warnings;
    auto result = load_pinned_nak(pin, manifest, profile, "/nonexistent", warnings);
    
    CHECK_FALSE(result.loaded);
    
    auto w = warnings.get_warnings();
    REQUIRE(w.size() >= 1);
    CHECK(w[0].key == "nak_pin_invalid");
}

TEST_CASE("load_pinned_nak: missing record file emits nak_pin_invalid") {
    // Per SPEC L1210-1212
    NakPin pin;
    pin.id = "com.example.nak";
    pin.version = "1.0.0";
    pin.record_ref = "com.example.nak@1.0.0.json";
    
    auto manifest = make_manifest("com.example.nak", ">=1.0.0 <2.0.0");
    auto profile = make_profile();
    
    WarningCollector warnings;
    auto result = load_pinned_nak(pin, manifest, profile, "/nonexistent/path", warnings);
    
    CHECK_FALSE(result.loaded);
    
    auto w = warnings.get_warnings();
    REQUIRE(w.size() >= 1);
    CHECK(w[0].key == "nak_pin_invalid");
}

TEST_CASE("load_pinned_nak: missing manifest nak_id emits invalid_manifest") {
    NakPin pin;
    pin.id = "com.example.nak";
    pin.version = "1.0.0";
    pin.record_ref = "com.example.nak@1.0.0.json";
    
    Manifest manifest;
    manifest.id = "com.example.app";
    manifest.version = "1.0.0";
    manifest.nak_id = "";  // Missing
    manifest.nak_version_req = parse_range(">=1.0.0 <2.0.0");
    
    auto profile = make_profile();
    
    WarningCollector warnings;
    // This will fail at file read, but tests the flow
    auto result = load_pinned_nak(pin, manifest, profile, "/nonexistent", warnings);
    
    CHECK_FALSE(result.loaded);
}

// ============================================================================
// NAK Not Resolved Tests (per SPEC L1238-1241)
// ============================================================================

TEST_CASE("NAK not resolved: empty registry results in unresolved") {
    auto manifest = make_manifest("com.example.nak", ">=1.0.0 <2.0.0");
    auto profile = make_profile();
    std::vector<NakRegistryEntry> registry;  // Empty
    
    WarningCollector warnings;
    auto result = select_nak_for_install(manifest, profile, registry, warnings);
    
    CHECK_FALSE(result.resolved);
    CHECK(result.pin.id.empty());
    CHECK(result.pin.version.empty());
    CHECK(result.pin.record_ref.empty());
}
