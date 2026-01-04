#include <doctest/doctest.h>
#include <nah/contract.hpp>
#include <nah/manifest.hpp>
#include <nah/manifest_tlv.hpp>
#include <nah/install_record.hpp>
#include <nah/host_profile.hpp>
#include <nah/platform.hpp>
#include <nah/types.hpp>
#include <nah/warnings.hpp>
#include <nah/semver.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

using namespace nah;

// ============================================================================
// Test Fixtures - Helper functions to create test inputs
// ============================================================================

namespace {

// Shorthand for portable path conversion in tests
std::string pp(const fs::path& path) {
    return to_portable_path(path.string());
}

// Create a temporary directory with test files
class TempTestDir {
public:
    TempTestDir() {
        base_path = fs::temp_directory_path() / ("nah_test_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        fs::create_directories(base_path);
        
        // Create app structure
        app_root = base_path / "apps" / "com.example.app" / "1.0.0";
        fs::create_directories(app_root / "bin");
        fs::create_directories(app_root / "lib");
        
        // Create entrypoint binary
        entrypoint = app_root / "bin" / "myapp";
        std::ofstream(entrypoint) << "#!/bin/sh\necho test\n";
        fs::permissions(entrypoint, fs::perms::owner_exec | fs::perms::owner_read);
        
        // Create NAK structure  
        nak_root = base_path / "naks" / "com.example.nak" / "3.0.0";
        fs::create_directories(nak_root / "lib");
        fs::create_directories(nak_root / "resources");
        
        // Create NAK loader binary
        nak_loader = nak_root / "bin" / "loader";
        fs::create_directories(nak_root / "bin");
        std::ofstream(nak_loader) << "#!/bin/sh\nexec $@\n";
        fs::permissions(nak_loader, fs::perms::owner_exec | fs::perms::owner_read);
    }
    
    ~TempTestDir() {
        fs::remove_all(base_path);
    }
    
    fs::path base_path;
    fs::path app_root;
    fs::path entrypoint;
    fs::path nak_root;
    fs::path nak_loader;
};

Manifest create_test_manifest() {
    Manifest m;
    m.id = "com.example.app";
    m.version = "1.0.0";
    m.entrypoint_path = "bin/myapp";
    m.nak_version_req = parse_range(">=3.0.0 <4.0.0");
    return m;
}

AppInstallRecord create_test_install_record(const std::string& app_root) {
    AppInstallRecord r;
    r.schema = "nah.app.install.v1";
    r.app.id = "com.example.app";
    r.app.version = "1.0.0";
    r.paths.install_root = app_root;
    r.nak.id = "com.example.nak";
    r.nak.version = "3.0.0";
    r.nak.record_ref = "com.example.nak@3.0.0.toml";
    r.trust.state = TrustState::Verified;
    r.trust.source = "test-host";
    r.trust.evaluated_at = "2025-01-01T00:00:00Z";
    return r;
}

HostProfile create_test_profile() {
    return get_builtin_empty_profile();
}

} // namespace

// ============================================================================
// compose_contract Basic Tests (per SPEC L877-L930)
// ============================================================================

TEST_CASE("compose_contract: produces valid contract for minimal inputs") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    HostProfile profile = create_test_profile();
    
    // Clear NAK pin to avoid needing actual NAK record files
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    inputs.now = "2025-06-01T00:00:00Z";
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK_FALSE(result.critical_error.has_value());
    CHECK(result.envelope.contract.app.id == "com.example.app");
    CHECK(result.envelope.contract.app.version == "1.0.0");
}

TEST_CASE("compose_contract: returns ENTRYPOINT_NOT_FOUND when entrypoint missing") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.entrypoint_path = "bin/nonexistent";  // Does not exist
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK_FALSE(result.ok);
    REQUIRE(result.critical_error.has_value());
    CHECK(*result.critical_error == CriticalError::ENTRYPOINT_NOT_FOUND);
}

TEST_CASE("compose_contract: returns ENTRYPOINT_NOT_FOUND for empty entrypoint") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.entrypoint_path = "";  // Empty
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK_FALSE(result.ok);
    REQUIRE(result.critical_error.has_value());
    CHECK(*result.critical_error == CriticalError::ENTRYPOINT_NOT_FOUND);
}

TEST_CASE("compose_contract: returns PATH_TRAVERSAL for entrypoint with ..") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.entrypoint_path = "../../../etc/passwd";  // Path traversal attempt
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK_FALSE(result.ok);
    REQUIRE(result.critical_error.has_value());
    CHECK(*result.critical_error == CriticalError::PATH_TRAVERSAL);
}

TEST_CASE("compose_contract: returns ENTRYPOINT_NOT_FOUND for absolute entrypoint") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.entrypoint_path = "/bin/sh";  // Absolute path not allowed
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK_FALSE(result.ok);
    REQUIRE(result.critical_error.has_value());
    CHECK(*result.critical_error == CriticalError::ENTRYPOINT_NOT_FOUND);
}

// ============================================================================
// App Field Derivation Tests (per SPEC L932-L948)
// ============================================================================

TEST_CASE("compose_contract: app.id comes from manifest") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.id = "com.custom.appid";
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.app.id = "com.custom.appid";
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.app.id == "com.custom.appid");
}

TEST_CASE("compose_contract: app.root comes from install_record.paths.install_root") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.app.root == tmp.app_root.string());
}

TEST_CASE("compose_contract: app.entrypoint is resolved under app.root") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.entrypoint_path = "bin/myapp";
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.app.entrypoint == pp(tmp.app_root / "bin" / "myapp"));
}

// ============================================================================
// Environment Layering Tests (per SPEC L978-L1016)
// ============================================================================

TEST_CASE("compose_contract: profile environment is layer 1 (lowest precedence defaults)") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    profile.environment["PROFILE_VAR"] = "from_profile";
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.environment.count("PROFILE_VAR") == 1);
    CHECK(result.envelope.contract.environment.at("PROFILE_VAR") == "from_profile");
}

TEST_CASE("compose_contract: manifest env_vars are layer 3") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.env_vars.push_back("MANIFEST_VAR=from_manifest");
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.environment.count("MANIFEST_VAR") == 1);
    CHECK(result.envelope.contract.environment.at("MANIFEST_VAR") == "from_manifest");
}

TEST_CASE("compose_contract: install_record overrides are layer 4 (overwrite)") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.env_vars.push_back("SHARED_VAR=from_manifest");
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    install_record.overrides.environment["SHARED_VAR"] = "from_install_override";
    
    HostProfile profile = create_test_profile();
    profile.environment["SHARED_VAR"] = "from_profile";
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    // Install record override should win over manifest and profile
    CHECK(result.envelope.contract.environment.at("SHARED_VAR") == "from_install_override");
}

TEST_CASE("compose_contract: NAH standard variables are layer 5") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.environment.count("NAH_APP_ID") == 1);
    CHECK(result.envelope.contract.environment.at("NAH_APP_ID") == "com.example.app");
    CHECK(result.envelope.contract.environment.at("NAH_APP_VERSION") == "1.0.0");
    CHECK(result.envelope.contract.environment.at("NAH_APP_ROOT") == tmp.app_root.string());
}

// ============================================================================
// Trust State Tests (per SPEC L470-L484)
// ============================================================================

TEST_CASE("compose_contract: trust.state copied from install_record") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    install_record.trust.state = TrustState::Verified;
    install_record.trust.source = "test-verifier";
    install_record.trust.evaluated_at = "2025-01-01T00:00:00Z";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    inputs.now = "2025-06-01T00:00:00Z";
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.trust.state == TrustState::Verified);
    CHECK(result.envelope.contract.trust.source == "test-verifier");
}

TEST_CASE("compose_contract: trust_state_unknown emitted when trust section absent") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    // Clear trust info to simulate absent section
    install_record.trust.source = "";
    install_record.trust.evaluated_at = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.trust.state == TrustState::Unknown);
    
    // Check for trust_state_unknown warning
    bool found_warning = false;
    for (const auto& w : result.envelope.warnings) {
        if (w.key == "trust_state_unknown") {
            found_warning = true;
            break;
        }
    }
    CHECK(found_warning);
}

TEST_CASE("compose_contract: trust_state_stale emitted when expires_at is in the past") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    install_record.trust.state = TrustState::Verified;
    install_record.trust.source = "test";
    install_record.trust.evaluated_at = "2025-01-01T00:00:00Z";
    install_record.trust.expires_at = "2025-02-01T00:00:00Z";  // In the past
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    inputs.now = "2025-06-01T00:00:00Z";  // After expires_at
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    
    // Check for trust_state_stale warning
    bool found_warning = false;
    for (const auto& w : result.envelope.warnings) {
        if (w.key == "trust_state_stale") {
            found_warning = true;
            break;
        }
    }
    CHECK(found_warning);
}

// ============================================================================
// Execution Field Tests (per SPEC L1068-L1086)
// ============================================================================

TEST_CASE("compose_contract: execution.binary is entrypoint when no NAK loader") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.execution.binary == result.envelope.contract.app.entrypoint);
}

TEST_CASE("compose_contract: execution.cwd defaults to app.root") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.execution.cwd == result.envelope.contract.app.root);
}

TEST_CASE("compose_contract: execution.library_path_env_key is platform-specific") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
#if defined(__APPLE__)
    CHECK(result.envelope.contract.execution.library_path_env_key == "DYLD_LIBRARY_PATH");
#elif defined(_WIN32)
    CHECK(result.envelope.contract.execution.library_path_env_key == "PATH");
#else
    CHECK(result.envelope.contract.execution.library_path_env_key == "LD_LIBRARY_PATH");
#endif
}

// ============================================================================
// Library Path Tests (per SPEC L1090-L1108)
// ============================================================================

TEST_CASE("compose_contract: manifest LIB_DIR entries resolved under app.root") {
    TempTestDir tmp;
    
    // Create lib directory
    fs::create_directories(tmp.app_root / "lib" / "native");
    
    Manifest manifest = create_test_manifest();
    manifest.lib_dirs.push_back("lib/native");
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    bool found_lib = false;
    for (const auto& path : result.envelope.contract.execution.library_paths) {
        if (path == pp(tmp.app_root / "lib" / "native")) {
            found_lib = true;
            break;
        }
    }
    CHECK(found_lib);
}

TEST_CASE("compose_contract: profile library_prepend added before app libs") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.lib_dirs.push_back("lib");
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    profile.paths.library_prepend.push_back("/opt/host/lib");
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    REQUIRE(result.envelope.contract.execution.library_paths.size() >= 2);
    // Profile prepend should come before app lib
    CHECK(result.envelope.contract.execution.library_paths[0] == "/opt/host/lib");
}

// ============================================================================
// Asset Export Tests (per SPEC L1110-L1120)
// ============================================================================

TEST_CASE("compose_contract: asset exports resolved under app.root") {
    TempTestDir tmp;
    
    // Create asset file
    fs::create_directories(tmp.app_root / "assets");
    std::ofstream(tmp.app_root / "assets" / "icon.png") << "PNG DATA";
    
    Manifest manifest = create_test_manifest();
    AssetExportParts exp;
    exp.id = "icon";
    exp.path = "assets/icon.png";
    exp.type = "image/png";
    manifest.asset_exports.push_back(exp);
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    REQUIRE(result.envelope.contract.exports.count("icon") == 1);
    CHECK(result.envelope.contract.exports.at("icon").path == pp(tmp.app_root / "assets" / "icon.png"));
    CHECK(result.envelope.contract.exports.at("icon").type == "image/png");
}

TEST_CASE("compose_contract: asset export with path traversal returns PATH_TRAVERSAL") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AssetExportParts exp;
    exp.id = "evil";
    exp.path = "../../../etc/passwd";  // Traversal attempt
    exp.type = "";
    manifest.asset_exports.push_back(exp);
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK_FALSE(result.ok);
    REQUIRE(result.critical_error.has_value());
    CHECK(*result.critical_error == CriticalError::PATH_TRAVERSAL);
}

TEST_CASE("compose_contract: duplicate asset export ids use last wins") {
    TempTestDir tmp;
    
    // Create asset files
    fs::create_directories(tmp.app_root / "assets");
    std::ofstream(tmp.app_root / "assets" / "first.txt") << "first";
    std::ofstream(tmp.app_root / "assets" / "second.txt") << "second";
    
    Manifest manifest = create_test_manifest();
    
    AssetExportParts exp1;
    exp1.id = "data";
    exp1.path = "assets/first.txt";
    manifest.asset_exports.push_back(exp1);
    
    AssetExportParts exp2;
    exp2.id = "data";  // Same ID
    exp2.path = "assets/second.txt";
    manifest.asset_exports.push_back(exp2);
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    REQUIRE(result.envelope.contract.exports.count("data") == 1);
    // Last wins
    CHECK(result.envelope.contract.exports.at("data").path == pp(tmp.app_root / "assets" / "second.txt"));
}

// ============================================================================
// Capability Derivation Tests (per SPEC L1044-L1066)
// ============================================================================

TEST_CASE("compose_contract: capability_usage.present is true when permissions declared") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.permissions_filesystem.push_back("read:$NAH_APP_ROOT");
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    profile.capabilities["filesystem.read"] = "sandbox.readonly";
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.capability_usage.present == true);
}

TEST_CASE("compose_contract: capability_usage.present is false when no permissions") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    // No permissions declared
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    CHECK(result.envelope.contract.capability_usage.present == false);
}

// ============================================================================
// NAK Pin Warning Tests (per SPEC L893-L918)
// ============================================================================

TEST_CASE("compose_contract: nak_pin_invalid warning when pin fields empty") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    
    // Empty NAK pin fields
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    
    // Check for nak_pin_invalid warning
    bool found_warning = false;
    for (const auto& w : result.envelope.warnings) {
        if (w.key == "nak_pin_invalid") {
            found_warning = true;
            break;
        }
    }
    CHECK(found_warning);
}

// ============================================================================
// Argument Ordering Tests (per SPEC L1068-L1078)
// ============================================================================

TEST_CASE("compose_contract: arguments order: prepend, template, manifest, append") {
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.entrypoint_args.push_back("--manifest-arg");
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    install_record.overrides.arguments.prepend.push_back("--prepend-arg");
    install_record.overrides.arguments.append.push_back("--append-arg");
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    
    const auto& args = result.envelope.contract.execution.arguments;
    REQUIRE(args.size() >= 3);
    
    // Find positions
    std::ptrdiff_t prepend_pos = -1, manifest_pos = -1, append_pos = -1;
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == "--prepend-arg") prepend_pos = static_cast<std::ptrdiff_t>(i);
        if (args[i] == "--manifest-arg") manifest_pos = static_cast<std::ptrdiff_t>(i);
        if (args[i] == "--append-arg") append_pos = static_cast<std::ptrdiff_t>(i);
    }
    
    CHECK(prepend_pos >= 0);
    CHECK(manifest_pos >= 0);
    CHECK(append_pos >= 0);
    CHECK(prepend_pos < manifest_pos);
    CHECK(manifest_pos < append_pos);
}

// ============================================================================
// Full Precedence Chain Tests (per SPEC L842-859)
// ============================================================================

TEST_CASE("compose_contract: fill-only layers - first to fill wins") {
    // Per SPEC L845-847: Profile, NAK, Manifest are all fill-only
    // Profile is layer 1 (applied first), Manifest is layer 3 (applied later)
    // For fill-only: first to set a key wins, later layers don't overwrite
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.env_vars.push_back("MANIFEST_ONLY=from_manifest");
    manifest.env_vars.push_back("SHARED_VAR=from_manifest");  // Also in profile
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    profile.environment["PROFILE_ONLY"] = "from_profile";
    profile.environment["SHARED_VAR"] = "from_profile";  // Also in manifest
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    // Profile-only var is set from profile
    CHECK(result.envelope.contract.environment.at("PROFILE_ONLY") == "from_profile");
    // Manifest-only var is set from manifest
    CHECK(result.envelope.contract.environment.at("MANIFEST_ONLY") == "from_manifest");
    // For SHARED_VAR: profile (layer 1) fills first, manifest (layer 3) can't overwrite
    CHECK(result.envelope.contract.environment.at("SHARED_VAR") == "from_profile");
}

TEST_CASE("compose_contract: install record overrides overwrite lower layers") {
    // Per SPEC L848: Install Record overrides (overwrite)
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.env_vars.push_back("TEST_VAR=from_manifest");
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    install_record.overrides.environment["TEST_VAR"] = "from_override";
    
    HostProfile profile = create_test_profile();
    profile.environment["TEST_VAR"] = "from_profile";
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    // Install record override wins over both profile and manifest
    CHECK(result.envelope.contract.environment.at("TEST_VAR") == "from_override");
}

TEST_CASE("compose_contract: NAH standard variables overwrite lower layers") {
    // Per SPEC L849: Standard NAH_* variables (overwrite)
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    // Try to set NAH_APP_ID in manifest - should be overwritten by standard
    manifest.env_vars.push_back("NAH_APP_ID=wrong_id");
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK(result.ok);
    // NAH standard variable wins - must match actual app.id
    CHECK(result.envelope.contract.environment.at("NAH_APP_ID") == "com.example.app");
}

// ============================================================================
// ENTRYPOINT_NOT_FOUND Tests (per SPEC L951-954, L1388)
// ============================================================================

TEST_CASE("compose_contract: ENTRYPOINT_NOT_FOUND for empty entrypoint_path") {
    // Per SPEC L951-954: Missing/empty entrypoint -> CriticalError::ENTRYPOINT_NOT_FOUND
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.entrypoint_path = "";
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK_FALSE(result.ok);
    REQUIRE(result.critical_error.has_value());
    CHECK(*result.critical_error == CriticalError::ENTRYPOINT_NOT_FOUND);
}

TEST_CASE("compose_contract: ENTRYPOINT_NOT_FOUND for nonexistent file") {
    // Per SPEC L1388: ENTRYPOINT_NOT_FOUND CriticalError
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.entrypoint_path = "bin/does_not_exist";
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK_FALSE(result.ok);
    REQUIRE(result.critical_error.has_value());
    CHECK(*result.critical_error == CriticalError::ENTRYPOINT_NOT_FOUND);
}

TEST_CASE("compose_contract: ENTRYPOINT_NOT_FOUND for absolute path in manifest") {
    // Per SPEC: Absolute paths in manifest fields are not allowed
    TempTestDir tmp;
    
    Manifest manifest = create_test_manifest();
    manifest.entrypoint_path = "/absolute/path/to/binary";
    
    AppInstallRecord install_record = create_test_install_record(tmp.app_root.string());
    install_record.nak.id = "";
    install_record.nak.version = "";
    install_record.nak.record_ref = "";
    
    HostProfile profile = create_test_profile();
    
    CompositionInputs inputs;
    inputs.nah_root = tmp.base_path.string();
    inputs.manifest = manifest;
    inputs.install_record = install_record;
    inputs.profile = profile;
    
    auto result = compose_contract(inputs);
    
    CHECK_FALSE(result.ok);
    REQUIRE(result.critical_error.has_value());
    CHECK(*result.critical_error == CriticalError::ENTRYPOINT_NOT_FOUND);
}
