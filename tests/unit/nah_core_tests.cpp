/*
 * NAH Core Header-Only Library - Comprehensive Test Suite
 * 
 * Tests cover:
 * - All pure functions
 * - All edge cases from the spec
 * - Path traversal detection
 * - Environment algebra
 * - Placeholder expansion limits
 * - Trust state handling
 * - Warning system
 * - JSON serialization
 * - Determinism (golden file comparisons)
 */

#include "nah/nah_core.h"

#include <doctest/doctest.h>
#include <sstream>

using namespace nah::core;

// ============================================================================
// PATH UTILITIES
// ============================================================================

TEST_CASE("PathUtilities: IsAbsolutePath") {
    // Unix paths
    CHECK(is_absolute_path("/"));
    CHECK(is_absolute_path("/usr"));
    CHECK(is_absolute_path("/usr/bin"));
    CHECK_FALSE(is_absolute_path("usr"));
    CHECK_FALSE(is_absolute_path("./usr"));
    CHECK_FALSE(is_absolute_path("../usr"));
    CHECK_FALSE(is_absolute_path(""));
    
#ifdef _WIN32
    // Windows paths
    CHECK(is_absolute_path("C:"));
    CHECK(is_absolute_path("C:\\"));
    CHECK(is_absolute_path("C:\\Users"));
    CHECK(is_absolute_path("\\\\server\\share"));
#endif
}

TEST_CASE("PathUtilities: NormalizeSeparators") {
    CHECK(normalize_separators("a/b/c") == "a/b/c");
    CHECK(normalize_separators("a\\b\\c") == "a/b/c");
    CHECK(normalize_separators("a\\b/c\\d") == "a/b/c/d");
    CHECK(normalize_separators("") == "");
}

TEST_CASE("PathUtilities: JoinPath") {
    CHECK(join_path("/app", "bin/run") == "/app/bin/run");
    CHECK(join_path("/app/", "bin/run") == "/app/bin/run");
    CHECK(join_path("/app", "/bin/run") == "/app/bin/run");
    CHECK(join_path("/app/", "/bin/run") == "/app/bin/run");
    CHECK(join_path("", "bin/run") == "bin/run");
    CHECK(join_path("/app", "") == "/app");
}

TEST_CASE("PathUtilities: PathEscapesRoot") {
    // Valid paths
    CHECK_FALSE(path_escapes_root("/app", "/app/bin/run"));
    CHECK_FALSE(path_escapes_root("/app", "/app/./bin"));
    CHECK_FALSE(path_escapes_root("/app", "/app/a/../b"));
    
    // Escaping paths
    CHECK(path_escapes_root("/app", "/other/bin"));
    CHECK(path_escapes_root("/app", "/app/../etc/passwd"));
    CHECK(path_escapes_root("/app", "/app/../../etc"));
    
    // Edge cases
    CHECK_FALSE(path_escapes_root("/app/", "/app/bin"));
    CHECK(path_escapes_root("/app", "/application"));  // Not a child
}

TEST_CASE("PathUtilities: GetLibraryPathEnvKey") {
    std::string key = get_library_path_env_key();
#if defined(__APPLE__)
    CHECK(key == "DYLD_LIBRARY_PATH");
#elif defined(_WIN32)
    CHECK(key == "PATH");
#else
    CHECK(key == "LD_LIBRARY_PATH");
#endif
}

// ============================================================================
// ENVIRONMENT OPERATIONS
// ============================================================================

TEST_CASE("EnvironmentOps: EnvOpParsing") {
    CHECK(parse_env_op("set") == EnvOp::Set);
    CHECK(parse_env_op("prepend") == EnvOp::Prepend);
    CHECK(parse_env_op("append") == EnvOp::Append);
    CHECK(parse_env_op("unset") == EnvOp::Unset);
    CHECK_FALSE(parse_env_op("invalid").has_value());
}

TEST_CASE("EnvironmentOps: ApplyEnvOp_Set") {
    std::unordered_map<std::string, std::string> env;
    
    auto result = apply_env_op("PATH", EnvValue(EnvOp::Set, "/new"), env);
    CHECK(result.has_value());
    CHECK(*result == "/new");
    
    env["PATH"] = "/old";
    result = apply_env_op("PATH", EnvValue(EnvOp::Set, "/new"), env);
    CHECK(result.has_value());
    CHECK(*result == "/new");
}

TEST_CASE("EnvironmentOps: ApplyEnvOp_Prepend") {
    std::unordered_map<std::string, std::string> env;
    env["PATH"] = "/existing";
    
    auto result = apply_env_op("PATH", EnvValue(EnvOp::Prepend, "/new", ":"), env);
    CHECK(result.has_value());
    CHECK(*result == "/new:/existing");
    
    // Empty existing
    env.clear();
    result = apply_env_op("PATH", EnvValue(EnvOp::Prepend, "/new", ":"), env);
    CHECK(result.has_value());
    CHECK(*result == "/new");
}

TEST_CASE("EnvironmentOps: ApplyEnvOp_Append") {
    std::unordered_map<std::string, std::string> env;
    env["PATH"] = "/existing";
    
    auto result = apply_env_op("PATH", EnvValue(EnvOp::Append, "/new", ":"), env);
    CHECK(result.has_value());
    CHECK(*result == "/existing:/new");
    
    // Empty existing
    env.clear();
    result = apply_env_op("PATH", EnvValue(EnvOp::Append, "/new", ":"), env);
    CHECK(result.has_value());
    CHECK(*result == "/new");
}

TEST_CASE("EnvironmentOps: ApplyEnvOp_Unset") {
    std::unordered_map<std::string, std::string> env;
    env["PATH"] = "/existing";
    
    auto result = apply_env_op("PATH", EnvValue(EnvOp::Unset, ""), env);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("EnvironmentOps: CustomSeparator") {
    std::unordered_map<std::string, std::string> env;
    env["PATH"] = "a";
    
    auto result = apply_env_op("PATH", EnvValue(EnvOp::Prepend, "b", ";"), env);
    CHECK(*result == "b;a");
}

// ============================================================================
// PLACEHOLDER EXPANSION
// ============================================================================

TEST_CASE("PlaceholderExpansion: Basic") {
    std::unordered_map<std::string, std::string> env;
    env["HOME"] = "/home/user";
    env["APP"] = "myapp";
    
    auto result = expand_placeholders("{HOME}/.{APP}", env);
    CHECK(result.ok);
    CHECK(result.value == "/home/user/.myapp");
}

TEST_CASE("PlaceholderExpansion: MissingVariable") {
    std::unordered_map<std::string, std::string> env;
    
    auto result = expand_placeholders("{MISSING}", env);
    CHECK(result.ok);
    CHECK(result.value == "");
}

TEST_CASE("PlaceholderExpansion: NoPlaceholders") {
    std::unordered_map<std::string, std::string> env;
    
    auto result = expand_placeholders("no placeholders", env);
    CHECK(result.ok);
    CHECK(result.value == "no placeholders");
}

TEST_CASE("PlaceholderExpansion: UnmatchedBrace") {
    std::unordered_map<std::string, std::string> env;
    env["X"] = "x";
    
    auto result = expand_placeholders("{X} and {incomplete", env);
    CHECK(result.ok);
    CHECK(result.value == "x and {incomplete");
}

TEST_CASE("PlaceholderExpansion: PlaceholderLimit") {
    std::unordered_map<std::string, std::string> env;
    env["X"] = "x";
    
    // Build string with too many placeholders
    std::string input;
    for (size_t i = 0; i <= MAX_PLACEHOLDERS; i++) {
        input += "{X}";
    }
    
    auto result = expand_placeholders(input, env);
    CHECK_FALSE(result.ok);
    CHECK(result.error == "placeholder_limit");
}

TEST_CASE("PlaceholderExpansion: ExpansionOverflow") {
    std::unordered_map<std::string, std::string> env;
    // Create a large value
    std::string large(MAX_EXPANDED_SIZE, 'x');
    env["LARGE"] = large;
    
    auto result = expand_placeholders("{LARGE}{LARGE}", env);
    CHECK_FALSE(result.ok);
    CHECK(result.error == "expansion_overflow");
}

TEST_CASE("PlaceholderExpansion: VectorExpansion") {
    std::unordered_map<std::string, std::string> env;
    env["ROOT"] = "/app";
    
    std::vector<std::string> inputs = {"{ROOT}/bin", "{ROOT}/lib", "static"};
    auto result = expand_string_vector(inputs, env);
    
    CHECK(result.size() == 3u);
    CHECK(result[0] == "/app/bin");
    CHECK(result[1] == "/app/lib");
    CHECK(result[2] == "static");
}

// ============================================================================
// VALIDATION
// ============================================================================

TEST_CASE("Validation: ValidDeclaration") {
    AppDeclaration decl;
    decl.id = "com.example.app";
    decl.version = "1.0.0";
    decl.entrypoint_path = "bin/run";
    
    auto result = validate_declaration(decl);
    CHECK(result.ok);
    CHECK(result.errors.empty());
}

TEST_CASE("Validation: MissingId") {
    AppDeclaration decl;
    decl.version = "1.0.0";
    decl.entrypoint_path = "bin/run";
    
    auto result = validate_declaration(decl);
    CHECK_FALSE(result.ok);
    CHECK(result.errors.size() == 1u);
}

TEST_CASE("Validation: AbsoluteEntrypoint") {
    AppDeclaration decl;
    decl.id = "com.example.app";
    decl.version = "1.0.0";
    decl.entrypoint_path = "/bin/run";
    
    auto result = validate_declaration(decl);
    CHECK_FALSE(result.ok);
}

TEST_CASE("Validation: AbsoluteLibDir") {
    AppDeclaration decl;
    decl.id = "com.example.app";
    decl.version = "1.0.0";
    decl.entrypoint_path = "bin/run";
    decl.lib_dirs = {"/usr/lib"};
    
    auto result = validate_declaration(decl);
    CHECK_FALSE(result.ok);
}

TEST_CASE("Validation: ValidInstallRecord") {
    InstallRecord record;
    record.install.instance_id = "uuid-123";
    record.paths.install_root = "/apps/myapp";
    
    auto result = validate_install_record(record);
    CHECK(result.ok);
}

TEST_CASE("Validation: RelativeInstallRoot") {
    InstallRecord record;
    record.install.instance_id = "uuid-123";
    record.paths.install_root = "apps/myapp";
    
    auto result = validate_install_record(record);
    CHECK_FALSE(result.ok);
}

TEST_CASE("Validation: ValidRuntime") {
    RuntimeDescriptor runtime;
    runtime.nak.id = "lua";
    runtime.nak.version = "5.4.6";
    runtime.paths.root = "/nah/nak/lua/5.4.6";
    runtime.paths.lib_dirs = {"/nah/nak/lua/5.4.6/lib"};
    
    auto result = validate_runtime(runtime);
    CHECK(result.ok);
}

TEST_CASE("Validation: RelativeRuntimeLibDir") {
    RuntimeDescriptor runtime;
    runtime.nak.id = "lua";
    runtime.nak.version = "5.4.6";
    runtime.paths.root = "/nah/nak/lua/5.4.6";
    runtime.paths.lib_dirs = {"lib"};
    
    auto result = validate_runtime(runtime);
    CHECK_FALSE(result.ok);
}

// ============================================================================
// HOST ENVIRONMENT
// ============================================================================

TEST_CASE("HostEnvironment: OverridePolicy_AllowAll") {
    HostEnvironment host_env;
    // Default: allow_env_overrides = true, allowed_env_keys empty = all allowed
    CHECK(host_env.overrides.allow_env_overrides == true);
    CHECK(host_env.overrides.allowed_env_keys.empty());
}

TEST_CASE("HostEnvironment: OverridePolicy_Disabled") {
    HostEnvironment host_env;
    host_env.overrides.allow_env_overrides = false;
    CHECK_FALSE(host_env.overrides.allow_env_overrides);
}

TEST_CASE("HostEnvironment: OverridePolicy_Allowlist") {
    HostEnvironment host_env;
    host_env.overrides.allowed_env_keys = {"DEBUG", "LOG_LEVEL"};
    CHECK(host_env.overrides.allowed_env_keys.size() == 2);
}

TEST_CASE("HostEnvironment: LibraryPaths") {
    HostEnvironment host_env;
    host_env.paths.library_prepend = {"/opt/libs"};
    host_env.paths.library_append = {"/usr/local/lib"};
    CHECK(host_env.paths.library_prepend.size() == 1);
    CHECK(host_env.paths.library_append.size() == 1);
}

// ============================================================================
// TRUST STATE
// ============================================================================

TEST_CASE("TrustState: Parsing") {
    CHECK(parse_trust_state("verified") == TrustState::Verified);
    CHECK(parse_trust_state("unverified") == TrustState::Unverified);
    CHECK(parse_trust_state("failed") == TrustState::Failed);
    CHECK(parse_trust_state("unknown") == TrustState::Unknown);
    CHECK_FALSE(parse_trust_state("invalid").has_value());
}

TEST_CASE("TrustState: Serialization") {
    CHECK(std::string(trust_state_to_string(TrustState::Verified)) == "verified");
    CHECK(std::string(trust_state_to_string(TrustState::Unverified)) == "unverified");
    CHECK(std::string(trust_state_to_string(TrustState::Failed)) == "failed");
    CHECK(std::string(trust_state_to_string(TrustState::Unknown)) == "unknown");
}

TEST_CASE("TrustState: TimestampNormalization") {
    CHECK(normalize_rfc3339("2025-01-18T00:00:00Z") == "2025-01-18T00:00:00Z");
    CHECK(normalize_rfc3339("2025-01-18T00:00:00+00:00") == "2025-01-18T00:00:00Z");
    CHECK(normalize_rfc3339("2025-01-18T00:00:00-00:00") == "2025-01-18T00:00:00Z");
}

TEST_CASE("TrustState: TimestampComparison") {
    CHECK(timestamp_before("2025-01-17T00:00:00Z", "2025-01-18T00:00:00Z"));
    CHECK_FALSE(timestamp_before("2025-01-18T00:00:00Z", "2025-01-17T00:00:00Z"));
    CHECK_FALSE(timestamp_before("2025-01-18T00:00:00Z", "2025-01-18T00:00:00Z"));
    
    // Normalization
    CHECK_FALSE(timestamp_before("2025-01-18T00:00:00Z", "2025-01-18T00:00:00+00:00"));
}

// ============================================================================
// WARNING SYSTEM
// ============================================================================

TEST_CASE("WarningSystem: WarningParsing") {
    CHECK(parse_warning_key("invalid_manifest") == Warning::invalid_manifest);
    CHECK_FALSE(parse_warning_key("not_a_warning").has_value());
}

TEST_CASE("WarningSystem: WarningToString") {
    CHECK(std::string(warning_to_string(Warning::invalid_manifest)) == "invalid_manifest");
    CHECK(std::string(warning_to_string(Warning::nak_not_found)) == "nak_not_found");
}

// ============================================================================
// COMPOSITION - STANDALONE APP
// ============================================================================

TEST_CASE("Composition: StandaloneApp") {
    AppDeclaration app;
    app.id = "com.example.hello";
    app.version = "1.0.0";
    app.entrypoint_path = "bin/hello";
    
    HostEnvironment profile;
    
    InstallRecord install;
    install.install.instance_id = "inst-001";
    install.paths.install_root = "/apps/hello";
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    
    RuntimeInventory inventory;
    
    auto result = nah_compose(app, profile, install, inventory);
    
    CHECK(result.ok);
    CHECK_FALSE(result.critical_error.has_value());
    
    CHECK(result.contract.app.id == "com.example.hello");
    CHECK(result.contract.app.version == "1.0.0");
    CHECK(result.contract.app.root == "/apps/hello");
    CHECK(result.contract.app.entrypoint == "/apps/hello/bin/hello");
    
    CHECK(result.contract.execution.binary == "/apps/hello/bin/hello");
    CHECK(result.contract.execution.cwd == "/apps/hello");
    
    // NAH standard vars
    CHECK(result.contract.environment.at("NAH_APP_ID") == "com.example.hello");
    CHECK(result.contract.environment.at("NAH_APP_ROOT") == "/apps/hello");
}

// ============================================================================
// COMPOSITION - APP WITH RUNTIME
// ============================================================================

TEST_CASE("Composition: AppWithRuntime") {
    AppDeclaration app;
    app.id = "com.example.game";
    app.version = "2.0.0";
    app.nak_id = "lua";
    app.nak_version_req = ">=5.4.0";
    app.entrypoint_path = "main.lua";
    app.entrypoint_args = {"--debug"};
    
    HostEnvironment profile;
    
    InstallRecord install;
    install.install.instance_id = "inst-002";
    install.paths.install_root = "/apps/game";
    install.nak.id = "lua";
    install.nak.version = "5.4.6";
    install.nak.record_ref = "lua@5.4.6.json";
    install.nak.loader = "default";
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    
    RuntimeDescriptor lua;
    lua.nak.id = "lua";
    lua.nak.version = "5.4.6";
    lua.paths.root = "/nah/nak/lua/5.4.6";
    lua.paths.lib_dirs = {"/nah/nak/lua/5.4.6/lib"};
    
    LoaderConfig loader;
    loader.exec_path = "/nah/nak/lua/5.4.6/bin/lua";
    loader.args_template = {"{NAH_APP_ENTRY}"};
    lua.loaders["default"] = loader;
    
    RuntimeInventory inventory;
    inventory.runtimes["lua@5.4.6.json"] = lua;
    
    auto result = nah_compose(app, profile, install, inventory);
    
    CHECK(result.ok);
    
    CHECK(result.contract.nak.id == "lua");
    CHECK(result.contract.nak.version == "5.4.6");
    CHECK(result.contract.execution.binary == "/nah/nak/lua/5.4.6/bin/lua");
    
    // Arguments: loader template expanded + entrypoint args
    REQUIRE(result.contract.execution.arguments.size() >= 2u);
    CHECK(result.contract.execution.arguments[0] == "/apps/game/main.lua");
    CHECK(result.contract.execution.arguments[1] == "--debug");
    
    // Library paths include NAK lib_dirs
    CHECK(result.contract.execution.library_paths.size() == 1u);
    CHECK(result.contract.execution.library_paths[0] == "/nah/nak/lua/5.4.6/lib");
}

// ============================================================================
// COMPOSITION - PATH TRAVERSAL
// ============================================================================

TEST_CASE("Composition: PathTraversal") {
    AppDeclaration app;
    app.id = "com.example.bad";
    app.version = "1.0.0";
    app.entrypoint_path = "../../../etc/passwd";
    
    HostEnvironment profile;
    
    InstallRecord install;
    install.install.instance_id = "inst-003";
    install.paths.install_root = "/apps/bad";
    
    RuntimeInventory inventory;
    
    auto result = nah_compose(app, profile, install, inventory);
    
    CHECK_FALSE(result.ok);
    REQUIRE(result.critical_error.has_value());
    CHECK(*result.critical_error == CriticalError::PATH_TRAVERSAL);
}

// ============================================================================
// COMPOSITION - ENVIRONMENT PRECEDENCE
// ============================================================================

TEST_CASE("Composition: EnvironmentPrecedence") {
    AppDeclaration app;
    app.id = "com.example.env";
    app.version = "1.0.0";
    app.entrypoint_path = "bin/run";
    app.env_vars = {"SHARED=from_manifest", "MANIFEST_ONLY=yes"};
    
    HostEnvironment profile;
    profile.vars["SHARED"] = EnvValue(EnvOp::Set, "from_profile");
    profile.vars["PROFILE_ONLY"] = EnvValue(EnvOp::Set, "yes");
    
    InstallRecord install;
    install.install.instance_id = "inst-004";
    install.paths.install_root = "/apps/env";
    install.overrides.environment["SHARED"] = EnvValue(EnvOp::Set, "from_override");
    install.overrides.environment["OVERRIDE_ONLY"] = EnvValue(EnvOp::Set, "yes");
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    
    RuntimeInventory inventory;
    
    auto result = nah_compose(app, profile, install, inventory);
    
    CHECK(result.ok);
    
    // Override wins over profile wins over manifest
    CHECK(result.contract.environment.at("SHARED") == "from_override");
    CHECK(result.contract.environment.at("PROFILE_ONLY") == "yes");
    CHECK(result.contract.environment.at("MANIFEST_ONLY") == "yes");
    CHECK(result.contract.environment.at("OVERRIDE_ONLY") == "yes");
    
    // NAH standard vars always set
    CHECK(result.contract.environment.at("NAH_APP_ID") == "com.example.env");
}

// ============================================================================
// COMPOSITION - LOADER SELECTION
// ============================================================================

TEST_CASE("Composition: LoaderAutoSelectDefault") {
    AppDeclaration app;
    app.id = "com.example.app";
    app.version = "1.0.0";
    app.nak_id = "runtime";
    app.entrypoint_path = "main.txt";
    
    InstallRecord install;
    install.install.instance_id = "inst-005";
    install.paths.install_root = "/apps/app";
    install.nak.record_ref = "runtime@1.0.json";
    // No loader specified
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    
    RuntimeDescriptor runtime;
    runtime.nak.id = "runtime";
    runtime.nak.version = "1.0.0";
    runtime.paths.root = "/nah/nak/runtime/1.0.0";
    
    LoaderConfig default_loader;
    default_loader.exec_path = "/nah/nak/runtime/1.0.0/bin/default";
    default_loader.args_template = {"{NAH_APP_ENTRY}"};
    runtime.loaders["default"] = default_loader;
    
    LoaderConfig other_loader;
    other_loader.exec_path = "/nah/nak/runtime/1.0.0/bin/other";
    runtime.loaders["other"] = other_loader;
    
    RuntimeInventory inventory;
    inventory.runtimes["runtime@1.0.json"] = runtime;
    
    HostEnvironment profile;
    
    auto result = nah_compose(app, profile, install, inventory);
    
    CHECK(result.ok);
    CHECK(result.contract.execution.binary == "/nah/nak/runtime/1.0.0/bin/default");
}

TEST_CASE("Composition: LoaderAutoSelectSingle") {
    AppDeclaration app;
    app.id = "com.example.app";
    app.version = "1.0.0";
    app.nak_id = "runtime";
    app.entrypoint_path = "main.txt";
    
    InstallRecord install;
    install.install.instance_id = "inst-006";
    install.paths.install_root = "/apps/app";
    install.nak.record_ref = "runtime@1.0.json";
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    
    RuntimeDescriptor runtime;
    runtime.nak.id = "runtime";
    runtime.nak.version = "1.0.0";
    runtime.paths.root = "/nah/nak/runtime/1.0.0";
    
    LoaderConfig only_loader;
    only_loader.exec_path = "/nah/nak/runtime/1.0.0/bin/only";
    runtime.loaders["only"] = only_loader;
    
    RuntimeInventory inventory;
    inventory.runtimes["runtime@1.0.json"] = runtime;
    
    HostEnvironment profile;
    
    auto result = nah_compose(app, profile, install, inventory);
    
    CHECK(result.ok);
    CHECK(result.contract.execution.binary == "/nah/nak/runtime/1.0.0/bin/only");
}

TEST_CASE("Composition: LoaderMultipleNoDefault") {
    AppDeclaration app;
    app.id = "com.example.app";
    app.version = "1.0.0";
    app.nak_id = "runtime";
    app.entrypoint_path = "main.txt";
    
    InstallRecord install;
    install.install.instance_id = "inst-007";
    install.paths.install_root = "/apps/app";
    install.nak.record_ref = "runtime@1.0.json";
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    
    RuntimeDescriptor runtime;
    runtime.nak.id = "runtime";
    runtime.nak.version = "1.0.0";
    runtime.paths.root = "/nah/nak/runtime/1.0.0";
    
    LoaderConfig loader1;
    loader1.exec_path = "/nah/nak/runtime/1.0.0/bin/one";
    runtime.loaders["one"] = loader1;
    
    LoaderConfig loader2;
    loader2.exec_path = "/nah/nak/runtime/1.0.0/bin/two";
    runtime.loaders["two"] = loader2;
    
    RuntimeInventory inventory;
    inventory.runtimes["runtime@1.0.json"] = runtime;
    
    HostEnvironment profile;
    
    auto result = nah_compose(app, profile, install, inventory);
    
    CHECK(result.ok);
    // Falls back to entrypoint when no loader can be auto-selected
    CHECK(result.contract.execution.binary == "/apps/app/main.txt");
    
    // Should have warning
    bool found_warning = false;
    for (const auto& w : result.warnings) {
        if (w.key == "nak_loader_required") {
            found_warning = true;
            break;
        }
    }
    CHECK(found_warning);
}

// ============================================================================
// COMPOSITION - TRUST WARNINGS
// ============================================================================

TEST_CASE("Composition: TrustWarnings") {
    AppDeclaration app;
    app.id = "com.example.app";
    app.version = "1.0.0";
    app.entrypoint_path = "bin/run";
    
    HostEnvironment profile;
    
    InstallRecord install;
    install.install.instance_id = "inst-008";
    install.paths.install_root = "/apps/app";
    
    RuntimeInventory inventory;
    
    // Unknown trust
    install.trust.state = TrustState::Unknown;
    auto result = nah_compose(app, profile, install, inventory);
    CHECK(result.ok);
    bool found = false;
    for (const auto& w : result.warnings) {
        if (w.key == "trust_state_unknown") found = true;
    }
    CHECK(found);
    
    // Unverified trust
    install.trust.state = TrustState::Unverified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    result = nah_compose(app, profile, install, inventory);
    found = false;
    for (const auto& w : result.warnings) {
        if (w.key == "trust_state_unverified") found = true;
    }
    CHECK(found);
    
    // Failed trust
    install.trust.state = TrustState::Failed;
    result = nah_compose(app, profile, install, inventory);
    found = false;
    for (const auto& w : result.warnings) {
        if (w.key == "trust_state_failed") found = true;
    }
    CHECK(found);
}

TEST_CASE("Composition: TrustStaleness") {
    AppDeclaration app;
    app.id = "com.example.app";
    app.version = "1.0.0";
    app.entrypoint_path = "bin/run";
    
    HostEnvironment profile;
    
    InstallRecord install;
    install.install.instance_id = "inst-009";
    install.paths.install_root = "/apps/app";
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-01T00:00:00Z";
    install.trust.expires_at = "2025-01-15T00:00:00Z";
    
    RuntimeInventory inventory;
    
    CompositionOptions options;
    options.now = "2025-01-18T00:00:00Z";  // After expiry
    
    auto result = nah_compose(app, profile, install, inventory, options);
    
    CHECK(result.ok);
    bool found = false;
    for (const auto& w : result.warnings) {
        if (w.key == "trust_state_stale") found = true;
    }
    CHECK(found);
}

// ============================================================================
// COMPOSITION - TRACING
// ============================================================================

TEST_CASE("Composition: Tracing") {
    AppDeclaration app;
    app.id = "com.example.app";
    app.version = "1.0.0";
    app.entrypoint_path = "bin/run";
    app.env_vars = {"APP_VAR=from_app"};
    
    HostEnvironment profile;
    profile.vars["PROFILE_VAR"] = EnvValue(EnvOp::Set, "from_profile");
    
    InstallRecord install;
    install.install.instance_id = "inst-010";
    install.paths.install_root = "/apps/app";
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    
    RuntimeInventory inventory;
    
    CompositionOptions options;
    options.enable_trace = true;
    
    auto result = nah_compose(app, profile, install, inventory, options);
    
    CHECK(result.ok);
    REQUIRE(result.trace.has_value());
    
    // Check trace has environment entries
    CHECK_FALSE(result.trace->environment.empty());
    
    // Check decisions log
    CHECK_FALSE(result.trace->decisions.empty());
    CHECK(result.trace->decisions[0] == "Starting composition");
}

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

TEST_CASE("JsonSerialization: EscapeString") {
    CHECK(json::escape("hello") == "hello");
    CHECK(json::escape("he\"llo") == "he\\\"llo");
    CHECK(json::escape("he\\llo") == "he\\\\llo");
    CHECK(json::escape("line1\nline2") == "line1\\nline2");
    CHECK(json::escape("tab\there") == "tab\\there");
}

TEST_CASE("JsonSerialization: SerializeContract") {
    LaunchContract contract;
    contract.app.id = "com.example.app";
    contract.app.version = "1.0.0";
    contract.app.root = "/apps/app";
    contract.app.entrypoint = "/apps/app/bin/run";
    contract.execution.binary = "/apps/app/bin/run";
    contract.execution.cwd = "/apps/app";
    contract.execution.library_path_env_key = "LD_LIBRARY_PATH";
    contract.trust.state = TrustState::Verified;
    
    std::string json_str = serialize_contract(contract);
    
    // Basic checks
    CHECK(json_str.find("\"schema\": \"nah.launch.contract.v1\"") != std::string::npos);
    CHECK(json_str.find("\"id\": \"com.example.app\"") != std::string::npos);
    CHECK(json_str.find("\"state\": \"verified\"") != std::string::npos);
}

TEST_CASE("JsonSerialization: SerializeResult") {
    CompositionResult result;
    result.ok = true;
    result.contract.app.id = "test";
    result.contract.app.version = "1.0";
    result.contract.app.root = "/app";
    result.contract.app.entrypoint = "/app/bin";
    result.contract.execution.binary = "/app/bin";
    result.contract.execution.cwd = "/app";
    result.contract.trust.state = TrustState::Unknown;
    
    result.warnings.push_back({"test_warning", "warn", {{"key", "value"}}});
    
    std::string json_str = serialize_result(result);
    
    CHECK(json_str.find("\"ok\": true") != std::string::npos);
    CHECK(json_str.find("\"critical_error\": null") != std::string::npos);
    CHECK(json_str.find("\"test_warning\"") != std::string::npos);
}

TEST_CASE("JsonSerialization: SerializeFailedResult") {
    CompositionResult result;
    result.ok = false;
    result.critical_error = CriticalError::PATH_TRAVERSAL;
    result.critical_error_context = "path escapes root";
    
    std::string json_str = serialize_result(result);
    
    CHECK(json_str.find("\"ok\": false") != std::string::npos);
    CHECK(json_str.find("\"PATH_TRAVERSAL\"") != std::string::npos);
    CHECK(json_str.find("\"contract\": null") != std::string::npos);
}

// ============================================================================
// DETERMINISM
// ============================================================================

TEST_CASE("Determinism: SameInputsSameOutput") {
    AppDeclaration app;
    app.id = "com.example.determinism";
    app.version = "1.0.0";
    app.entrypoint_path = "bin/run";
    app.env_vars = {"A=1", "B=2"};
    
    HostEnvironment profile;
    profile.vars["C"] = EnvValue(EnvOp::Set, "3");
    
    InstallRecord install;
    install.install.instance_id = "inst-det";
    install.paths.install_root = "/apps/det";
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    
    RuntimeInventory inventory;
    
    // Compose twice
    auto result1 = nah_compose(app, profile, install, inventory);
    auto result2 = nah_compose(app, profile, install, inventory);
    
    CHECK(result1.ok);
    CHECK(result2.ok);
    
    // Serialize and compare
    std::string json1 = serialize_contract(result1.contract);
    std::string json2 = serialize_contract(result2.contract);
    
    CHECK(json1 == json2);
}

// ============================================================================
// EDGE CASES
// ============================================================================

TEST_CASE("EdgeCases: EmptyInventory") {
    AppDeclaration app;
    app.id = "com.example.app";
    app.version = "1.0.0";
    app.nak_id = "missing";
    app.entrypoint_path = "bin/run";
    
    HostEnvironment profile;
    
    InstallRecord install;
    install.install.instance_id = "inst-edge";
    install.paths.install_root = "/apps/app";
    install.nak.record_ref = "missing@1.0.json";
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    
    RuntimeInventory inventory;  // Empty
    
    auto result = nah_compose(app, profile, install, inventory);
    
    CHECK(result.ok);  // Continues without NAK
    CHECK(result.contract.nak.id == "");
    
    // Should have warning
    bool found = false;
    for (const auto& w : result.warnings) {
        if (w.key == "nak_not_found") found = true;
    }
    CHECK(found);
}

TEST_CASE("EdgeCases: LibsOnlyNak") {
    AppDeclaration app;
    app.id = "com.example.app";
    app.version = "1.0.0";
    app.nak_id = "libs";
    app.entrypoint_path = "bin/run";
    
    HostEnvironment profile;
    
    InstallRecord install;
    install.install.instance_id = "inst-libs";
    install.paths.install_root = "/apps/app";
    install.nak.record_ref = "libs@1.0.json";
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    
    RuntimeDescriptor libs;
    libs.nak.id = "libs";
    libs.nak.version = "1.0.0";
    libs.paths.root = "/nah/nak/libs/1.0.0";
    libs.paths.lib_dirs = {"/nah/nak/libs/1.0.0/lib"};
    // No loaders
    
    RuntimeInventory inventory;
    inventory.runtimes["libs@1.0.json"] = libs;
    
    auto result = nah_compose(app, profile, install, inventory);
    
    CHECK(result.ok);
    // Should use app entrypoint since no loaders
    CHECK(result.contract.execution.binary == "/apps/app/bin/run");
    // But should have library paths from NAK
    REQUIRE(result.contract.execution.library_paths.size() == 1u);
    CHECK(result.contract.execution.library_paths[0] == "/nah/nak/libs/1.0.0/lib");
}

TEST_CASE("EdgeCases: LoaderNotFound") {
    AppDeclaration app;
    app.id = "com.example.app";
    app.version = "1.0.0";
    app.nak_id = "runtime";
    app.entrypoint_path = "main.txt";
    
    InstallRecord install;
    install.install.instance_id = "inst-loader";
    install.paths.install_root = "/apps/app";
    install.nak.record_ref = "runtime@1.0.json";
    install.nak.loader = "nonexistent";  // Bad loader
    install.trust.state = TrustState::Verified;
    install.trust.source = "test";
    install.trust.evaluated_at = "2025-01-18T00:00:00Z";
    
    RuntimeDescriptor runtime;
    runtime.nak.id = "runtime";
    runtime.nak.version = "1.0.0";
    runtime.paths.root = "/nah/nak/runtime/1.0.0";
    
    LoaderConfig loader;
    loader.exec_path = "/nah/nak/runtime/1.0.0/bin/existing";
    runtime.loaders["existing"] = loader;
    
    RuntimeInventory inventory;
    inventory.runtimes["runtime@1.0.json"] = runtime;
    
    HostEnvironment profile;
    
    auto result = nah_compose(app, profile, install, inventory);
    
    CHECK_FALSE(result.ok);
    REQUIRE(result.critical_error.has_value());
    CHECK(*result.critical_error == CriticalError::NAK_LOADER_INVALID);
}

// ============================================================================
// VERSION INFO
// ============================================================================

TEST_CASE("Version: Constants") {
    CHECK(std::string(NAH_CORE_VERSION) == "1.0.0");
    CHECK(NAH_CORE_VERSION_MAJOR == 1);
    CHECK(NAH_CORE_VERSION_MINOR == 0);
    CHECK(NAH_CORE_VERSION_PATCH == 0);
    CHECK(std::string(NAH_CONTRACT_SCHEMA) == "nah.launch.contract.v1");
}
