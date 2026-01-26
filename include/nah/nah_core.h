/*
 * NAH Core - Header-Only Library
 * SPDX-License-Identifier: Apache-2.0
 *
 * ============================================================================
 * WHAT IS NAH?
 * ============================================================================
 *
 * NAH answers a simple question: "How should I launch this application?"
 *
 * When you install an app that needs Python 3.11, or Node 20, or Lua 5.4,
 * something has to figure out: which binary to run, what environment variables
 * to set, which library paths to include, and what permissions are required.
 *
 * NAH takes four inputs and produces one output:
 *
 *   +-------------------+
 *   |  AppDeclaration   |---+   What the app says it needs
 *   +-------------------+   |
 *   +-------------------+   |
 *   |  HostEnvironment  |---+--> nah_compose() --> LaunchContract
 *   +-------------------+   |                      (everything needed to run)
 *   +-------------------+   |
 *   |   InstallRecord   |---+   Where the app is installed
 *   +-------------------+   |
 *   +-------------------+   |
 *   | RuntimeInventory  |---+   Available runtimes (Python, Node, etc.)
 *   +-------------------+
 *
 * The result is a LaunchContract: a complete, self-contained specification
 * that tells you exactly how to run the application.
 *
 * ============================================================================
 * QUICK START
 * ============================================================================
 *
 * 1. Include the header (no linking required):
 *
 *      #include "nah/nah_core.h"
 *      using namespace nah::core;
 *
 * 2. Prepare your inputs:
 *
 *      AppDeclaration app;
 *      app.id = "com.example.myapp";
 *      app.version = "1.0.0";
 *      app.entrypoint_path = "main.lua";
 *      app.nak_id = "lua";
 *      app.nak_version_req = ">=5.4.0";
 *
 *      InstallRecord install;
 *      install.install.instance_id = "abc123";
 *      install.paths.install_root = "/apps/myapp";
 *      install.nak.record_ref = "lua@5.4.6.json";
 *
 *      HostEnvironment host_env;  // Empty = no host overrides
 *      RuntimeInventory inventory;
 *      inventory.runtimes["lua@5.4.6.json"] = your_lua_runtime;
 *
 * 3. Compose and use the contract:
 *
 *      CompositionResult result = nah_compose(app, host_env, install, inventory);
 *      if (result.ok) {
 *          // result.contract.execution.binary  -> "/runtimes/lua/bin/lua"
 *          // result.contract.execution.arguments -> ["/apps/myapp/main.lua"]
 *          // result.contract.environment -> {"LUA_PATH": "...", ...}
 *          exec(result.contract);  // Your execution logic
 *      }
 *
 * For file-based usage with JSON parsing, see nah.h which adds nah_json.h,
 * nah_fs.h, and nah_exec.h on top of this pure core.
 *
 * ============================================================================
 * KEY TYPES
 * ============================================================================
 *
 * Inputs:
 *   AppDeclaration   - What the app needs (id, version, entrypoint, runtime)
 *   HostEnvironment  - Host-provided environment variables
 *   InstallRecord    - Where the app lives and which runtime version to use
 *   RuntimeInventory - Available runtimes on this host
 *
 * Output:
 *   LaunchContract   - Complete exec specification (binary, args, env, cwd)
 *
 * Each type has inline documentation with usage examples.
 */

#ifndef NAH_CORE_H
#define NAH_CORE_H

#ifdef __cplusplus

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace nah {
namespace core {

// ============================================================================
// VERSION AND CONSTANTS
// ============================================================================

/// Library version following semver
constexpr const char* NAH_CORE_VERSION = "1.0.0";
constexpr int NAH_CORE_VERSION_MAJOR = 1;
constexpr int NAH_CORE_VERSION_MINOR = 0;
constexpr int NAH_CORE_VERSION_PATCH = 0;

/// Schema identifier for serialized contracts
constexpr const char* NAH_CONTRACT_SCHEMA = "nah.launch.contract.v1";

/// Maximum expanded string size (64 KiB) - prevents DoS via expansion
constexpr size_t MAX_EXPANDED_SIZE = 64 * 1024;

/// Maximum placeholder count per string - prevents DoS via placeholder bombs
constexpr size_t MAX_PLACEHOLDERS = 128;

/// Maximum environment variables - prevents resource exhaustion
constexpr size_t MAX_ENV_VARS = 1024;

/// Maximum library paths - prevents resource exhaustion
constexpr size_t MAX_LIBRARY_PATHS = 256;

/// Maximum arguments - prevents resource exhaustion  
constexpr size_t MAX_ARGUMENTS = 1024;

// ============================================================================
// ENVIRONMENT OPERATIONS
// ============================================================================

/**
 * Environment variable operation type.
 * 
 * The environment algebra supports four operations:
 * - Set: Replace or set a value (default)
 * - Prepend: Add to the front with separator
 * - Append: Add to the end with separator
 * - Unset: Remove the variable entirely
 */
enum class EnvOp {
    Set,      ///< Replace/fill value
    Prepend,  ///< Prepend with separator
    Append,   ///< Append with separator
    Unset     ///< Remove variable
};

/// Convert EnvOp to string representation
inline const char* env_op_to_string(EnvOp op) {
    switch (op) {
        case EnvOp::Set: return "set";
        case EnvOp::Prepend: return "prepend";
        case EnvOp::Append: return "append";
        case EnvOp::Unset: return "unset";
    }
    return "set";
}

/// Parse string to EnvOp
inline std::optional<EnvOp> parse_env_op(const std::string& s) {
    if (s == "set") return EnvOp::Set;
    if (s == "prepend") return EnvOp::Prepend;
    if (s == "append") return EnvOp::Append;
    if (s == "unset") return EnvOp::Unset;
    return std::nullopt;
}

/**
 * Environment variable value with operation.
 * 
 * Supports the environment algebra: set, prepend, append, unset.
 * Default separator for prepend/append is ":" (Unix-style).
 */
struct EnvValue {
    EnvOp op = EnvOp::Set;
    std::string value;
    std::string separator = ":";
    
    EnvValue() = default;
    EnvValue(const char* v) : op(EnvOp::Set), value(v) {}
    EnvValue(const std::string& v) : op(EnvOp::Set), value(v) {}
    EnvValue(EnvOp o, const std::string& v, const std::string& sep = ":")
        : op(o), value(v), separator(sep) {}
    
    bool is_simple() const { return op == EnvOp::Set; }
    
    // Comparison operators
    bool operator==(const std::string& other) const { return value == other; }
    bool operator==(const char* other) const { return value == other; }
    bool operator==(const EnvValue& other) const {
        return op == other.op && value == other.value && separator == other.separator;
    }
    bool operator!=(const EnvValue& other) const { return !(*this == other); }
};

/// Type alias for environment map
using EnvMap = std::unordered_map<std::string, EnvValue>;

// ============================================================================
// TRUST STATE
// ============================================================================

/**
 * Trust verification state.
 * 
 * - Verified: Cryptographic verification succeeded
 * - Unverified: No verification was performed
 * - Failed: Verification was attempted but failed
 * - Unknown: Trust state could not be determined
 */
enum class TrustState {
    Verified,
    Unverified,
    Failed,
    Unknown
};

inline const char* trust_state_to_string(TrustState s) {
    switch (s) {
        case TrustState::Verified: return "verified";
        case TrustState::Unverified: return "unverified";
        case TrustState::Failed: return "failed";
        case TrustState::Unknown: return "unknown";
    }
    return "unknown";
}

inline std::optional<TrustState> parse_trust_state(const std::string& s) {
    if (s == "verified") return TrustState::Verified;
    if (s == "unverified") return TrustState::Unverified;
    if (s == "failed") return TrustState::Failed;
    if (s == "unknown") return TrustState::Unknown;
    return std::nullopt;
}

/**
 * Trust information for an installed artifact.
 * 
 * Contains verification state, timestamps, and optional details.
 * Timestamps use RFC3339 format (e.g., "2025-01-18T12:00:00Z").
 */
struct TrustInfo {
    TrustState state = TrustState::Unknown;
    std::string source;          ///< Who/what performed verification
    std::string evaluated_at;    ///< When verification occurred (RFC3339)
    std::string expires_at;      ///< When verification expires (RFC3339, optional)
    std::string inputs_hash;     ///< Hash of inputs to verification (optional)
    std::unordered_map<std::string, std::string> details;  ///< Additional metadata
};

// ============================================================================
// WARNING SYSTEM
// ============================================================================

/**
 * Warning types that can be emitted during composition.
 * 
 * Warnings are non-fatal issues that may indicate problems.
 * Each warning can be configured with an action: warn, ignore, or error.
 */
enum class Warning {
    invalid_manifest,         ///< Manifest has structural issues
    invalid_configuration,    ///< Configuration is malformed
    profile_invalid,          ///< Profile has structural issues
    profile_missing,          ///< Referenced profile not found
    profile_parse_error,      ///< Profile could not be parsed
    nak_pin_invalid,          ///< NAK pin is malformed
    nak_not_found,            ///< Referenced NAK not in inventory
    nak_version_unsupported,  ///< NAK version not allowed by policy
    nak_loader_required,      ///< NAK has loaders but none specified
    nak_loader_missing,       ///< Requested loader not in NAK
    binary_not_found,         ///< Binary doesn't exist (diagnostic only)
    capability_missing,       ///< Required capability not granted
    capability_malformed,     ///< Capability string is malformed
    capability_unknown,       ///< Capability type not recognized
    missing_env_var,          ///< Referenced env var not found
    invalid_trust_state,      ///< Trust state is malformed
    override_denied,          ///< Override blocked by policy
    override_invalid,         ///< Override value is invalid
    invalid_library_path,     ///< Library path is invalid
    trust_state_unknown,      ///< Trust state is unknown
    trust_state_unverified,   ///< Trust state is unverified
    trust_state_failed,       ///< Trust verification failed
    trust_state_stale,        ///< Trust verification has expired
};

inline const char* warning_to_string(Warning w) {
    switch (w) {
        case Warning::invalid_manifest: return "invalid_manifest";
        case Warning::invalid_configuration: return "invalid_configuration";
        case Warning::profile_invalid: return "profile_invalid";
        case Warning::profile_missing: return "profile_missing";
        case Warning::profile_parse_error: return "profile_parse_error";
        case Warning::nak_pin_invalid: return "nak_pin_invalid";
        case Warning::nak_not_found: return "nak_not_found";
        case Warning::nak_version_unsupported: return "nak_version_unsupported";
        case Warning::nak_loader_required: return "nak_loader_required";
        case Warning::nak_loader_missing: return "nak_loader_missing";
        case Warning::binary_not_found: return "binary_not_found";
        case Warning::capability_missing: return "capability_missing";
        case Warning::capability_malformed: return "capability_malformed";
        case Warning::capability_unknown: return "capability_unknown";
        case Warning::missing_env_var: return "missing_env_var";
        case Warning::invalid_trust_state: return "invalid_trust_state";
        case Warning::override_denied: return "override_denied";
        case Warning::override_invalid: return "override_invalid";
        case Warning::invalid_library_path: return "invalid_library_path";
        case Warning::trust_state_unknown: return "trust_state_unknown";
        case Warning::trust_state_unverified: return "trust_state_unverified";
        case Warning::trust_state_failed: return "trust_state_failed";
        case Warning::trust_state_stale: return "trust_state_stale";
    }
    return "unknown";
}

inline std::optional<Warning> parse_warning_key(const std::string& key) {
    if (key == "invalid_manifest") return Warning::invalid_manifest;
    if (key == "invalid_configuration") return Warning::invalid_configuration;
    if (key == "profile_invalid") return Warning::profile_invalid;
    if (key == "profile_missing") return Warning::profile_missing;
    if (key == "profile_parse_error") return Warning::profile_parse_error;
    if (key == "nak_pin_invalid") return Warning::nak_pin_invalid;
    if (key == "nak_not_found") return Warning::nak_not_found;
    if (key == "nak_version_unsupported") return Warning::nak_version_unsupported;
    if (key == "nak_loader_required") return Warning::nak_loader_required;
    if (key == "nak_loader_missing") return Warning::nak_loader_missing;
    if (key == "binary_not_found") return Warning::binary_not_found;
    if (key == "capability_missing") return Warning::capability_missing;
    if (key == "capability_malformed") return Warning::capability_malformed;
    if (key == "capability_unknown") return Warning::capability_unknown;
    if (key == "missing_env_var") return Warning::missing_env_var;
    if (key == "invalid_trust_state") return Warning::invalid_trust_state;
    if (key == "override_denied") return Warning::override_denied;
    if (key == "override_invalid") return Warning::override_invalid;
    if (key == "invalid_library_path") return Warning::invalid_library_path;
    if (key == "trust_state_unknown") return Warning::trust_state_unknown;
    if (key == "trust_state_unverified") return Warning::trust_state_unverified;
    if (key == "trust_state_failed") return Warning::trust_state_failed;
    if (key == "trust_state_stale") return Warning::trust_state_stale;
    return std::nullopt;
}

/**
 * A warning object with key, action, and optional fields.
 */
struct WarningObject {
    std::string key;     ///< Warning identifier (lowercase_snake_case)
    std::string action;  ///< Action taken: "warn" or "error"
    std::unordered_map<std::string, std::string> fields;  ///< Additional context
    
    bool operator==(const WarningObject& other) const {
        return key == other.key && action == other.action && fields == other.fields;
    }
};

// ============================================================================
// CRITICAL ERRORS
// ============================================================================

/**
 * Critical errors that halt composition.
 * 
 * Unlike warnings, critical errors cannot be ignored and always
 * result in composition failure.
 */
enum class CriticalError {
    MANIFEST_MISSING,        ///< App manifest not found or invalid
    ENTRYPOINT_NOT_FOUND,    ///< Entrypoint binary doesn't exist
    PATH_TRAVERSAL,          ///< Path escapes allowed root
    INSTALL_RECORD_INVALID,  ///< Install record is malformed
    NAK_LOADER_INVALID,      ///< Requested loader not available
};

inline const char* critical_error_to_string(CriticalError e) {
    switch (e) {
        case CriticalError::MANIFEST_MISSING: return "MANIFEST_MISSING";
        case CriticalError::ENTRYPOINT_NOT_FOUND: return "ENTRYPOINT_NOT_FOUND";
        case CriticalError::PATH_TRAVERSAL: return "PATH_TRAVERSAL";
        case CriticalError::INSTALL_RECORD_INVALID: return "INSTALL_RECORD_INVALID";
        case CriticalError::NAK_LOADER_INVALID: return "NAK_LOADER_INVALID";
    }
    return "UNKNOWN";
}

inline std::optional<CriticalError> parse_critical_error(const std::string& s) {
    if (s == "MANIFEST_MISSING") return CriticalError::MANIFEST_MISSING;
    if (s == "ENTRYPOINT_NOT_FOUND") return CriticalError::ENTRYPOINT_NOT_FOUND;
    if (s == "PATH_TRAVERSAL") return CriticalError::PATH_TRAVERSAL;
    if (s == "INSTALL_RECORD_INVALID") return CriticalError::INSTALL_RECORD_INVALID;
    if (s == "NAK_LOADER_INVALID") return CriticalError::NAK_LOADER_INVALID;
    return std::nullopt;
}

// ============================================================================
// TRACE SYSTEM
// ============================================================================

/**
 * Source kind constants for tracing.
 * 
 * Valid values: host, nak_record, manifest, install_record,
 * process_env, overrides_file, standard, nah_standard
 */
namespace trace_source {
    constexpr const char* HOST = "host";
    constexpr const char* NAK_RECORD = "nak_record";
    constexpr const char* NAK = "nak";  // Alias for NAK_RECORD
    constexpr const char* MANIFEST = "manifest";
    constexpr const char* INSTALL_RECORD = "install_record";
    constexpr const char* INSTALL_OVERRIDE = "install_override";
    constexpr const char* PROCESS_ENV = "process_env";
    constexpr const char* OVERRIDES_FILE = "overrides_file";
    constexpr const char* STANDARD = "standard";
    constexpr const char* NAH_STANDARD = "nah_standard";
    constexpr const char* COMPUTED = "computed";
}

/**
 * A single contribution to a traced value.
 * 
 * Records where a value came from, its precedence, and whether it was used.
 */
struct TraceContribution {
    std::string value;           ///< The contributed value
    std::string source_kind;     ///< Where it came from (profile, nak_record, manifest, etc.)
    std::string source_path;     ///< Specific file/location
    int precedence_rank = 0;     ///< Priority (1=highest)
    EnvOp operation = EnvOp::Set;///< Operation applied
    bool accepted = false;       ///< Was this used in final value?
};

/**
 * Full trace entry for a single value.
 * 
 * Contains the final resolved value and history of all contributions.
 */
struct TraceEntry {
    std::string value;           ///< The resolved value
    std::string source_kind;     ///< Winning source kind
    std::string source_path;     ///< Winning source path
    int precedence_rank = 0;     ///< Winning precedence
    std::vector<TraceContribution> history;  ///< All contributions
};

/**
 * Complete trace of composition decisions.
 */
struct CompositionTrace {
    std::unordered_map<std::string, TraceEntry> environment;
    std::unordered_map<std::string, TraceEntry> library_paths;
    std::unordered_map<std::string, TraceEntry> arguments;
    std::vector<std::string> decisions;  ///< Human-readable decision log
};

// ============================================================================
// COMPONENT DECLARATION
// ============================================================================

/// A component is a launchable feature within an application.
///
/// Components allow a single app package to provide multiple entry points
/// (e.g., editor, viewer, debugger) with independent loader selection.
///
/// Example:
///
///     ComponentDecl editor;
///     editor.id = "editor";
///     editor.name = "Document Editor";
///     editor.entrypoint = "bin/editor";
///     editor.uri_pattern = "com.docproc.suite://editor/*";
///     editor.loader = "default";  // Optional: use specific NAK loader
///     editor.standalone = true;
///     editor.hidden = false;
///
struct ComponentDecl {
    std::string id;            ///< Component identifier (unique within app)
    std::string name;          ///< Human-readable name
    std::string description;   ///< Optional description
    std::string icon;          ///< Relative path to icon (optional)
    std::string entrypoint;    ///< Relative path to executable/script
    std::string uri_pattern;   ///< URI pattern this component handles
    std::string loader;        ///< Optional: specific NAK loader name
    bool standalone = true;    ///< Can be launched independently
    bool hidden = false;       ///< Hide from host UI
    
    // Per-component overrides (extend app-level settings)
    EnvMap environment;                       ///< Component-specific environment
    std::vector<std::string> permissions_filesystem;  ///< Component-specific perms
    std::vector<std::string> permissions_network;     ///< Component-specific perms
    
    std::unordered_map<std::string, std::string> metadata;  ///< Arbitrary metadata
};

// ============================================================================
// APP DECLARATION
// ============================================================================

// An asset the app exposes for other apps or the host to use.
struct AssetExportDecl {
    std::string id;    ///< Export identifier (e.g., "icon", "schema")
    std::string path;  ///< Relative path under app root (e.g., "assets/icon.png")
    std::string type;  ///< MIME type or category (optional)
};

// What the app declares it needs to run.
//
// This is typically parsed from a manifest file (nah.json, package.json, etc.)
// but can be constructed directly. All paths are relative to where the app
// will be installed.
//
// Example - a Lua app:
//
//     AppDeclaration app;
//     app.id = "com.example.game";
//     app.version = "2.1.0";
//     app.entrypoint_path = "main.lua";
//     app.nak_id = "lua";
//     app.nak_version_req = ">=5.4.0";
//     app.lib_dirs = {"lib", "vendor"};
//     app.env_vars = {"GAME_DATA=./data"};
//
// Example - a standalone binary (no runtime):
//
//     AppDeclaration app;
//     app.id = "org.tool.converter";
//     app.version = "1.0.0";
//     app.entrypoint_path = "bin/converter";
//     // nak_id left empty - no runtime needed
//
struct AppDeclaration {
    // Required: App identity
    std::string id;       ///< Unique identifier (e.g., "com.example.app")
    std::string version;  ///< Semantic version (e.g., "1.2.3")
    
    // Required: What to run
    std::string entrypoint_path;  ///< Relative path to main binary or script
    
    // Optional: Runtime requirements (leave nak_id empty for standalone binaries)
    std::string nak_id;          ///< Runtime identifier (e.g., "lua", "node", "python")
    std::string nak_version_req; ///< Version constraint (e.g., ">=5.4.0", "^20.0.0")
    std::string nak_loader;      ///< Specific loader if runtime has multiple
    
    // Optional: Arguments passed after the entrypoint
    std::vector<std::string> entrypoint_args;
    
    // Optional: Environment variables (lowest precedence, fill-only)
    // Format: "KEY=value" - only set if not already defined by host/runtime
    std::vector<std::string> env_vars;
    
    // Optional: Library search paths (relative to app root)
    std::vector<std::string> lib_dirs;
    
    // Optional: Asset directories and exports
    std::vector<std::string> asset_dirs;
    std::vector<AssetExportDecl> asset_exports;
    
    // Optional: Permission requests
    std::vector<std::string> permissions_filesystem;  ///< e.g., "read:./data"
    std::vector<std::string> permissions_network;     ///< e.g., "connect:https://*"
    
    // Optional: Metadata (informational only, does not affect composition)
    std::string description;
    std::string author;
    std::string license;
    std::string homepage;
    
    // Optional: Components provided by this application
    std::vector<ComponentDecl> components;
};

// ============================================================================
// HOST ENVIRONMENT
// ============================================================================

// Host configuration loaded from host.json.
//
// This replaces the old "profiles" concept with a single host configuration.
// It contains environment variables, library paths, and override policy.
//
// Example:
//
//     HostEnvironment host_env;
//     host_env.vars["NAH_ENV"] = EnvValue(EnvOp::Set, "production");
//     host_env.vars["LOG_LEVEL"] = EnvValue(EnvOp::Set, "warn");
//     host_env.paths.library_prepend = {"/opt/libs"};
//
// Host environment takes precedence over app-declared environment variables
// but can be overridden by install record overrides (subject to override policy).
//
struct HostEnvironment {
    EnvMap vars;  ///< Environment variables to inject
    
    struct {
        std::vector<std::string> library_prepend;  ///< Library paths to prepend
        std::vector<std::string> library_append;   ///< Library paths to append
    } paths;
    
    struct {
        bool allow_env_overrides = true;  ///< Allow NAH_OVERRIDE_ENVIRONMENT
        std::vector<std::string> allowed_env_keys;  ///< If non-empty, only these keys can be overridden
    } overrides;
    
    std::string source_path;  ///< For tracing (e.g., "/nah/host/host.json")
};

// ============================================================================
// LOADER CONFIGURATION
// ============================================================================

// How a runtime executes app entrypoints.
//
// For example, Lua's loader might be:
//     exec_path: "/runtimes/lua/bin/lua"
//     args_template: ["{NAH_APP_ENTRY}"]
//
// The args_template supports {VAR} placeholders that are expanded from the
// environment before execution.
struct LoaderConfig {
    std::string exec_path;                   ///< Absolute path to interpreter/runtime
    std::vector<std::string> args_template;  ///< Arguments with {VAR} placeholders
};

// ============================================================================
// RUNTIME DESCRIPTOR
// ============================================================================

// Describes an installed runtime (NAK - Native Application Kit).
//
// A NAK is a runtime like Lua, Node, or Python that apps can depend on.
// The RuntimeDescriptor tells NAH where the runtime is installed and how
// to use it.
//
// Example - Lua 5.4.6:
//
//     RuntimeDescriptor lua;
//     lua.nak.id = "lua";
//     lua.nak.version = "5.4.6";
//     lua.paths.root = "/runtimes/lua/5.4.6";
//     lua.paths.lib_dirs = {"/runtimes/lua/5.4.6/lib"};
//     lua.environment["LUA_PATH"] = EnvValue(EnvOp::Prepend, "./?.lua", ";");
//     lua.loaders["default"] = {"/runtimes/lua/5.4.6/bin/lua", {"{NAH_APP_ENTRY}"}};
//
// NAKs without loaders (libs-only) just provide libraries and environment.
//
struct RuntimeDescriptor {
    struct {
        std::string id;       ///< Runtime identifier (e.g., "lua", "node")
        std::string version;  ///< Version string (e.g., "5.4.6")
    } nak;
    
    struct {
        std::string root;           ///< Absolute path to runtime installation
        std::string resource_root;  ///< Resource path (defaults to root if empty)
        std::vector<std::string> lib_dirs;  ///< Library directories (absolute paths)
    } paths;
    
    // Environment variables provided by this runtime
    EnvMap environment;
    
    // Loaders - how this runtime executes apps. Empty for libs-only NAKs.
    // Key is loader name (use "default" for the primary loader).
    std::unordered_map<std::string, LoaderConfig> loaders;
    
    bool has_loaders() const { return !loaders.empty(); }
    
    struct {
        bool present = false;
        std::string cwd;  ///< Working directory template (supports {VAR} placeholders)
    } execution;
    
    struct {
        std::string package_hash;   ///< SHA256 of installed package
        std::string installed_at;   ///< When installed (RFC3339)
        std::string installed_by;   ///< What tool installed it
        std::string source;         ///< Where it came from (URL, path)
    } provenance;
    
    std::string source_path;  ///< For tracing
};

// ============================================================================
// INSTALL RECORD
// ============================================================================

// Records where an app is installed and which runtime version to use.
//
// Created at install time, this captures:
// - Where the app lives on disk (paths.install_root)
// - Which specific runtime version to use (nak.record_ref)
// - Trust/verification state
// - Per-install overrides
//
// Example - minimal install record:
//
//     InstallRecord install;
//     install.install.instance_id = "550e8400-e29b-41d4-a716-446655440000";
//     install.paths.install_root = "/apps/myapp";
//     install.nak.record_ref = "lua@5.4.6.json";  // Key into RuntimeInventory
//
// Example - with environment override:
//
//     InstallRecord install;
//     install.install.instance_id = "...";
//     install.paths.install_root = "/apps/myapp";
//     install.nak.record_ref = "lua@5.4.6.json";
//     install.overrides.environment["DEBUG"] = "1";
//
struct InstallRecord {
    // Unique identifier for this installation
    struct {
        std::string instance_id;  ///< UUID or similar unique ID
    } install;
    
    // Snapshot of app info at install time (audit only, does not affect composition)
    struct {
        std::string id;
        std::string version;
        std::string nak_id;
        std::string nak_version_req;
    } app;
    
    // Which runtime to use - resolved and pinned at install time
    struct {
        std::string id;               ///< Runtime identifier
        std::string version;          ///< Pinned version
        std::string record_ref;       ///< Key into RuntimeInventory (e.g., "lua@5.4.6.json")
        std::string loader;           ///< Pinned loader name (if runtime has multiple)
        std::string selection_reason; ///< Why this version was chosen
    } nak;
    
    struct {
        std::string install_root;  ///< Absolute path to installed app
    } paths;
    
    struct {
        std::string package_hash;   ///< SHA256 of installed package
        std::string installed_at;   ///< When installed (RFC3339)
        std::string installed_by;   ///< What tool installed it
        std::string source;         ///< Where it came from
    } provenance;
    
    TrustInfo trust;

    // Verification info (optional)
    struct {
        std::string last_verified_at;      ///< When last verified (RFC3339)
        std::string last_verifier_version; ///< Version of tool that verified
    } verification;

    // Per-install overrides (subject to host profile policy)
    struct {
        EnvMap environment;  ///< Additional/override environment variables
        struct {
            std::vector<std::string> prepend;  ///< Arguments to prepend
            std::vector<std::string> append;   ///< Arguments to append
        } arguments;
        struct {
            std::vector<std::string> library_prepend;  ///< Library paths to prepend
        } paths;
    } overrides;
    
    std::string source_path;  ///< For tracing
};

// ============================================================================
// RUNTIME INVENTORY
// ============================================================================

// Collection of available runtimes on the host.
//
// Maps record_ref (e.g., "lua@5.4.6.json") to RuntimeDescriptor.
// The InstallRecord's nak.record_ref is used as the key to look up the runtime.
//
// Example:
//
//     RuntimeInventory inventory;
//     inventory.runtimes["lua@5.4.6.json"] = lua_descriptor;
//     inventory.runtimes["node@20.0.0.json"] = node_descriptor;
//
struct RuntimeInventory {
    std::unordered_map<std::string, RuntimeDescriptor> runtimes;
};

// ============================================================================
// ASSET EXPORT
// ============================================================================

// An exported asset in the contract (paths resolved to absolute).
struct AssetExport {
    std::string id;    ///< Export identifier
    std::string path;  ///< Absolute path on disk
    std::string type;  ///< MIME type (optional)
};

// ============================================================================
// COMPONENT URI
// ============================================================================

/// Parsed component URI.
///
/// Format: <app-id>://<component-path>[?<query>][#<fragment>]
///
/// Example:
///     com.devtools://editor/open?file=doc.txt#line-42
///
struct ComponentURI {
    bool valid = false;             ///< Parse succeeded
    std::string raw_uri;            ///< Original URI
    std::string app_id;             ///< Application ID (scheme)
    std::string component_path;     ///< Path after ://
    std::string query;              ///< Query string (without ?)
    std::string fragment;           ///< Fragment (without #)
};

/// Parse a component URI.
///
/// Format: <app-id>://<component-path>[?<query>][#<fragment>]
///
/// Examples:
///     com.suite://editor                    → app_id="com.suite", component_path="editor"
///     com.suite://editor/open               → component_path="editor/open"
///     com.suite://editor?file=doc.txt       → query="file=doc.txt"
///     com.suite://editor#section-3          → fragment="section-3"
///
inline ComponentURI parse_component_uri(const std::string& uri) {
    ComponentURI result;
    result.raw_uri = uri;
    
    // Find scheme separator
    size_t scheme_end = uri.find("://");
    if (scheme_end == std::string::npos) {
        return result;  // Invalid
    }
    
    // Extract app_id (scheme)
    result.app_id = uri.substr(0, scheme_end);
    if (result.app_id.empty()) {
        return result;
    }
    
    std::string rest = uri.substr(scheme_end + 3);
    
    // Extract fragment (if present)
    size_t fragment_pos = rest.find('#');
    if (fragment_pos != std::string::npos) {
        result.fragment = rest.substr(fragment_pos + 1);
        rest = rest.substr(0, fragment_pos);
    }
    
    // Extract query (if present)
    size_t query_pos = rest.find('?');
    if (query_pos != std::string::npos) {
        result.query = rest.substr(query_pos + 1);
        rest = rest.substr(0, query_pos);
    }
    
    // Remaining is component_path
    result.component_path = rest;
    result.valid = true;
    
    return result;
}

// ============================================================================
// CAPABILITY USAGE
// ============================================================================

// Summary of capabilities requested by the app.
struct CapabilityUsage {
    bool present = false;
    std::vector<std::string> required_capabilities;
    std::vector<std::string> optional_capabilities;
    std::vector<std::string> critical_capabilities;
};

// ============================================================================
// LAUNCH CONTRACT
// ============================================================================

// The output of nah_compose() - everything needed to launch an application.
//
// The contract is self-contained: no additional lookups are needed to execute
// the app. All paths are absolute, all environment variables are resolved,
// and the exact binary and arguments are specified.
//
// To execute a contract:
//
//     if (result.ok) {
//         const auto& c = result.contract;
//
//         // Set environment
//         for (const auto& [key, value] : c.environment) {
//             setenv(key.c_str(), value.c_str(), 1);
//         }
//
//         // Set library path
//         std::string lib_path;
//         for (const auto& p : c.execution.library_paths) {
//             if (!lib_path.empty()) lib_path += ":";
//             lib_path += p;
//         }
//         setenv(c.execution.library_path_env_key.c_str(), lib_path.c_str(), 1);
//
//         // Change to working directory
//         chdir(c.execution.cwd.c_str());
//
//         // Build argv: [binary, ...arguments]
//         std::vector<char*> argv;
//         argv.push_back(const_cast<char*>(c.execution.binary.c_str()));
//         for (const auto& arg : c.execution.arguments) {
//             argv.push_back(const_cast<char*>(arg.c_str()));
//         }
//         argv.push_back(nullptr);
//
//         // Execute
//         execv(c.execution.binary.c_str(), argv.data());
//     }
//
struct LaunchContract {
    // App identity and paths (all absolute)
    struct {
        std::string id;          ///< App identifier
        std::string version;     ///< App version
        std::string root;        ///< Absolute path to app installation
        std::string entrypoint;  ///< Absolute path to entrypoint file
    } app;

    // Runtime info (empty if standalone app)
    struct {
        std::string id;            ///< Runtime identifier
        std::string version;       ///< Runtime version
        std::string root;          ///< Absolute path to runtime
        std::string resource_root; ///< Resource path (often same as root)
        std::string record_ref;    ///< Key used to look up this runtime
    } nak;

    // How to execute the app
    struct {
        std::string binary;        ///< What to exec() - interpreter or app binary
        std::vector<std::string> arguments;  ///< Arguments (entrypoint may be here)
        std::string cwd;           ///< Working directory
        std::string library_path_env_key;    ///< "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", etc.
        std::vector<std::string> library_paths;  ///< Library search paths
    } execution;

    // Complete environment map (ready to pass to exec)
    std::unordered_map<std::string, std::string> environment;

    // Capability/permission requirements for sandboxing
    struct {
        std::vector<std::string> filesystem;  ///< Filesystem permissions
        std::vector<std::string> network;     ///< Network permissions
    } enforcement;

    // Trust/verification state from install record
    TrustInfo trust;

    // Exported assets (id -> absolute path)
    std::unordered_map<std::string, AssetExport> exports;

    // Summary of capability usage
    CapabilityUsage capability_usage;
};

// ============================================================================
// POLICY VIOLATION
// ============================================================================

// Describes a policy violation (e.g., path traversal attempt).
struct PolicyViolation {
    std::string type;     ///< Violation type (e.g., "path_traversal")
    std::string target;   ///< What was violated (e.g., "entrypoint")
    std::string context;  ///< Human-readable description
};

// ============================================================================
// COMPOSITION OPTIONS
// ============================================================================

// Options passed to nah_compose().
struct CompositionOptions {
    bool enable_trace = false;       ///< If true, result.trace will be populated
    std::string now;                 ///< Current time (RFC3339) for trust staleness checks
    std::string loader_override;     ///< Override loader selection (empty = use install record)
};

// ============================================================================
// COMPOSITION RESULT
// ============================================================================

// The result of calling nah_compose().
//
// Check result.ok to see if composition succeeded. If true, result.contract
// contains the launch specification. If false, check critical_error and
// critical_error_context for what went wrong.
//
// Warnings are always populated (even on success) for non-fatal issues.
//
// Example:
//
//     CompositionResult result = nah_compose(app, profile, install, inventory);
//     if (!result.ok) {
//         std::cerr << "Composition failed: " << result.critical_error_context << "\n";
//         return 1;
//     }
//     for (const auto& w : result.warnings) {
//         std::cerr << "Warning: " << w.key << "\n";
//     }
//     // Use result.contract...
//
struct CompositionResult {
    bool ok = false;                              ///< True if composition succeeded
    std::optional<CriticalError> critical_error;  ///< Error type (if !ok)
    std::string critical_error_context;           ///< Human-readable error message
    LaunchContract contract;                      ///< The launch contract (valid if ok)
    std::vector<WarningObject> warnings;          ///< Non-fatal warnings
    std::vector<PolicyViolation> policy_violations;
    std::optional<CompositionTrace> trace;        ///< Detailed trace (if options.enable_trace)
};

// ============================================================================
// PURE FUNCTIONS - Path Utilities
// ============================================================================

/**
 * Check if a path is absolute.
 * 
 * On Unix: starts with /
 * On Windows: starts with drive letter or UNC path
 */
inline bool is_absolute_path(const std::string& path) {
    if (path.empty()) return false;
#ifdef _WIN32
    if (path.size() >= 2) {
        if (path[1] == ':') return true;
        if (path[0] == '\\' && path[1] == '\\') return true;
    }
#endif
    return path[0] == '/';
}

/**
 * Normalize path separators to forward slashes.
 */
inline std::string normalize_separators(const std::string& path) {
    std::string result = path;
    for (char& c : result) {
        if (c == '\\') c = '/';
    }
    return result;
}

/**
 * Check if a path escapes its root via traversal.
 * 
 * Detects attempts to escape using ".." components.
 * This is a pure string operation - no filesystem access.
 */
inline bool path_escapes_root(const std::string& root, const std::string& path) {
    std::string norm_root = normalize_separators(root);
    std::string norm_path = normalize_separators(path);
    
    while (!norm_root.empty() && norm_root.back() == '/') {
        norm_root.pop_back();
    }
    
    // Path must start with root
    if (norm_path.find(norm_root) != 0) {
        return true;
    }
    
    // Path must either be exactly root, or have a / after the root prefix
    // This prevents /app matching /application
    std::string rel = norm_path.substr(norm_root.size());
    if (!rel.empty() && rel[0] != '/') {
        return true;  // e.g., /application doesn't have / after /app
    }
    if (!rel.empty() && rel[0] == '/') {
        rel = rel.substr(1);
    }
    
    int depth = 0;
    size_t pos = 0;
    while (pos < rel.size()) {
        size_t next = rel.find('/', pos);
        std::string component = (next == std::string::npos) 
            ? rel.substr(pos) 
            : rel.substr(pos, next - pos);
        
        if (component == "..") {
            depth--;
            if (depth < 0) return true;
        } else if (!component.empty() && component != ".") {
            depth++;
        }
        
        if (next == std::string::npos) break;
        pos = next + 1;
    }
    
    return false;
}

/**
 * Join two path components.
 */
inline std::string join_path(const std::string& base, const std::string& rel) {
    if (base.empty()) return rel;
    if (rel.empty()) return base;
    
    std::string result = base;
    if (result.back() != '/' && result.back() != '\\') {
        result += '/';
    }
    
    size_t start = 0;
    while (start < rel.size() && (rel[start] == '/' || rel[start] == '\\')) {
        start++;
    }
    
    result += rel.substr(start);
    return normalize_separators(result);
}

/**
 * Get the platform-specific library path environment key.
 */
inline std::string get_library_path_env_key() {
#if defined(__APPLE__)
    return "DYLD_LIBRARY_PATH";
#elif defined(_WIN32)
    return "PATH";
#else
    return "LD_LIBRARY_PATH";
#endif
}

/**
 * Get the platform-specific path separator.
 */
inline char get_path_separator() {
#ifdef _WIN32
    return ';';
#else
    return ':';
#endif
}

// ============================================================================
// PURE FUNCTIONS - Validation
// ============================================================================

/**
 * Validation result with errors and warnings.
 */
struct ValidationResult {
    bool ok = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/**
 * Validate an app declaration.
 * 
 * Checks:
 * - Required fields present (id, version, entrypoint_path)
 * - entrypoint_path is relative
 * - lib_dirs are relative
 * - asset_exports have relative paths
 */
inline ValidationResult validate_declaration(const AppDeclaration& decl) {
    ValidationResult result;
    
    if (decl.id.empty()) {
        result.ok = false;
        result.errors.push_back("app.id is required");
    }
    
    if (decl.version.empty()) {
        result.ok = false;
        result.errors.push_back("app.version is required");
    }
    
    if (decl.entrypoint_path.empty()) {
        result.ok = false;
        result.errors.push_back("entrypoint_path is required");
    }
    
    if (!decl.entrypoint_path.empty() && is_absolute_path(decl.entrypoint_path)) {
        result.ok = false;
        result.errors.push_back("entrypoint_path must be relative");
    }
    
    for (const auto& lib_dir : decl.lib_dirs) {
        if (is_absolute_path(lib_dir)) {
            result.ok = false;
            result.errors.push_back("lib_dir must be relative: " + lib_dir);
        }
    }
    
    for (const auto& exp : decl.asset_exports) {
        if (is_absolute_path(exp.path)) {
            result.ok = false;
            result.errors.push_back("asset_export path must be relative: " + exp.path);
        }
    }
    
    if (!decl.nak_id.empty() && decl.nak_version_req.empty()) {
        result.warnings.push_back("nak_id specified but nak_version_req is empty");
    }
    
    return result;
}

/**
 * Validate an install record.
 */
inline ValidationResult validate_install_record(const InstallRecord& record) {
    ValidationResult result;
    
    if (record.install.instance_id.empty()) {
        result.ok = false;
        result.errors.push_back("install.instance_id is required");
    }
    
    if (record.paths.install_root.empty()) {
        result.ok = false;
        result.errors.push_back("paths.install_root is required");
    }
    
    if (!record.paths.install_root.empty() && !is_absolute_path(record.paths.install_root)) {
        result.ok = false;
        result.errors.push_back("paths.install_root must be absolute");
    }
    
    return result;
}

/**
 * Validate a runtime descriptor.
 */
inline ValidationResult validate_runtime(const RuntimeDescriptor& runtime) {
    ValidationResult result;
    
    if (runtime.nak.id.empty()) {
        result.ok = false;
        result.errors.push_back("nak.id is required");
    }
    
    if (runtime.nak.version.empty()) {
        result.ok = false;
        result.errors.push_back("nak.version is required");
    }
    
    if (runtime.paths.root.empty()) {
        result.ok = false;
        result.errors.push_back("paths.root is required");
    }
    
    if (!runtime.paths.root.empty() && !is_absolute_path(runtime.paths.root)) {
        result.ok = false;
        result.errors.push_back("paths.root must be absolute");
    }
    
    for (const auto& lib_dir : runtime.paths.lib_dirs) {
        if (!is_absolute_path(lib_dir)) {
            result.ok = false;
            result.errors.push_back("lib_dir must be absolute: " + lib_dir);
        }
    }
    
    for (const auto& [name, loader] : runtime.loaders) {
        if (!loader.exec_path.empty() && !is_absolute_path(loader.exec_path)) {
            result.ok = false;
            result.errors.push_back("loader exec_path must be absolute: " + name);
        }
    }
    
    return result;
}



// ============================================================================
// PURE FUNCTIONS - Environment
// ============================================================================

/**
 * Apply an environment operation.
 * 
 * Returns the new value, or nullopt for unset.
 */
inline std::optional<std::string> apply_env_op(
    const std::string& key,
    const EnvValue& env_val,
    const std::unordered_map<std::string, std::string>& current_env)
{
    switch (env_val.op) {
        case EnvOp::Set:
            return env_val.value;
            
        case EnvOp::Prepend: {
            auto it = current_env.find(key);
            if (it != current_env.end() && !it->second.empty()) {
                return env_val.value + env_val.separator + it->second;
            }
            return env_val.value;
        }
        
        case EnvOp::Append: {
            auto it = current_env.find(key);
            if (it != current_env.end() && !it->second.empty()) {
                return it->second + env_val.separator + env_val.value;
            }
            return env_val.value;
        }
        
        case EnvOp::Unset:
            return std::nullopt;
    }
    
    return env_val.value;
}

// ============================================================================
// PURE FUNCTIONS - Placeholder Expansion
// ============================================================================

/**
 * Result of placeholder expansion.
 */
struct ExpansionResult {
    bool ok = true;
    std::string value;
    std::string error;
};

/**
 * Expand {VAR} placeholders in a string.
 * 
 * Single-pass, no recursion. Missing variables become empty strings.
 * Enforces size and count limits to prevent DoS.
 */
inline ExpansionResult expand_placeholders(
    const std::string& input,
    const std::unordered_map<std::string, std::string>& env)
{
    ExpansionResult result;
    result.value.reserve(input.size());
    
    size_t placeholder_count = 0;
    size_t i = 0;
    
    while (i < input.size()) {
        if (input[i] == '{') {
            size_t end = input.find('}', i + 1);
            if (end != std::string::npos) {
                std::string var_name = input.substr(i + 1, end - i - 1);
                
                placeholder_count++;
                if (placeholder_count > MAX_PLACEHOLDERS) {
                    result.ok = false;
                    result.error = "placeholder_limit";
                    return result;
                }
                
                auto it = env.find(var_name);
                if (it != env.end()) {
                    result.value += it->second;
                }
                
                if (result.value.size() > MAX_EXPANDED_SIZE) {
                    result.ok = false;
                    result.error = "expansion_overflow";
                    return result;
                }
                
                i = end + 1;
                continue;
            }
        }
        
        result.value += input[i];
        i++;
        
        if (result.value.size() > MAX_EXPANDED_SIZE) {
            result.ok = false;
            result.error = "expansion_overflow";
            return result;
        }
    }
    
    return result;
}

/**
 * Expand placeholders in a vector of strings.
 */
inline std::vector<std::string> expand_string_vector(
    const std::vector<std::string>& inputs,
    const std::unordered_map<std::string, std::string>& env)
{
    std::vector<std::string> result;
    result.reserve(inputs.size());
    
    for (const auto& input : inputs) {
        auto expanded = expand_placeholders(input, env);
        result.push_back(expanded.ok ? expanded.value : input);
    }
    
    return result;
}

// ============================================================================
// PURE FUNCTIONS - Runtime Resolution
// ============================================================================

/**
 * Runtime resolution result.
 */
struct RuntimeResolutionResult {
    bool resolved = false;
    std::string record_ref;
    RuntimeDescriptor runtime;
    std::string selection_reason;
    std::vector<std::string> warnings;
};

/**
 * Resolve runtime from inventory.
 * 
 * Uses the pinned record_ref from install record to look up the runtime.
 */
inline RuntimeResolutionResult resolve_runtime(
    const AppDeclaration& app,
    const InstallRecord& install,
    const RuntimeInventory& inventory)
{
    RuntimeResolutionResult result;
    
    // Standalone apps don't need runtime resolution
    if (app.nak_id.empty()) {
        result.resolved = true;
        result.selection_reason = "standalone_app";
        return result;
    }
    
    // Get record_ref from install record
    std::string record_ref = install.nak.record_ref;
    
    if (record_ref.empty()) {
        result.warnings.push_back("nak.record_ref is empty in install record");
        return result;
    }
    
    auto it = inventory.runtimes.find(record_ref);
    if (it == inventory.runtimes.end()) {
        result.warnings.push_back("NAK not found in inventory: " + record_ref);
        return result;
    }
    
    result.resolved = true;
    result.record_ref = record_ref;
    result.runtime = it->second;
    result.selection_reason = "pinned_from_install_record";
    
    return result;
}

// ============================================================================
// PURE FUNCTIONS - Path Binding
// ============================================================================

/**
 * Path binding result.
 */
struct PathBindingResult {
    bool ok = true;
    std::string entrypoint;
    std::vector<std::string> library_paths;
    std::unordered_map<std::string, AssetExport> exports;
    std::vector<PolicyViolation> violations;
};

/**
 * Bind relative paths to absolute paths.
 */
inline PathBindingResult bind_paths(
    const AppDeclaration& decl,
    const InstallRecord& install,
    const RuntimeDescriptor* runtime,
    const HostEnvironment& host_env)
{
    PathBindingResult result;
    const std::string& app_root = install.paths.install_root;
    
    // Entrypoint
    std::string entrypoint = join_path(app_root, decl.entrypoint_path);
    if (path_escapes_root(app_root, entrypoint)) {
        result.ok = false;
        result.violations.push_back({
            "path_traversal", "entrypoint", "entrypoint escapes app root"
        });
        return result;
    }
    result.entrypoint = entrypoint;
    
    // Library paths in order: host prepend, install overrides, NAK, app, host append
    for (const auto& path : host_env.paths.library_prepend) {
        if (is_absolute_path(path)) {
            result.library_paths.push_back(path);
        }
    }
    
    for (const auto& path : install.overrides.paths.library_prepend) {
        if (is_absolute_path(path)) {
            result.library_paths.push_back(path);
        }
    }
    
    if (runtime) {
        for (const auto& lib_dir : runtime->paths.lib_dirs) {
            result.library_paths.push_back(lib_dir);
        }
    }
    
    for (const auto& lib_dir : decl.lib_dirs) {
        std::string abs_lib = join_path(app_root, lib_dir);
        if (path_escapes_root(app_root, abs_lib)) {
            result.ok = false;
            result.violations.push_back({
                "path_traversal", "lib_dir", "lib_dir escapes app root: " + lib_dir
            });
            return result;
        }
        result.library_paths.push_back(abs_lib);
    }
    
    for (const auto& path : host_env.paths.library_append) {
        if (is_absolute_path(path)) {
            result.library_paths.push_back(path);
        }
    }
    
    // Asset exports
    for (const auto& exp : decl.asset_exports) {
        std::string abs_path = join_path(app_root, exp.path);
        if (path_escapes_root(app_root, abs_path)) {
            result.ok = false;
            result.violations.push_back({
                "path_traversal", "asset_export", "asset export escapes app root: " + exp.id
            });
            return result;
        }
        result.exports[exp.id] = {exp.id, abs_path, exp.type};
    }
    
    return result;
}

// ============================================================================
// PURE FUNCTIONS - Environment Composition
// ============================================================================

/**
 * Compose environment from all sources.
 * 
 * Precedence (highest to lowest):
 * 1. NAH standard variables (NAH_APP_*, NAH_NAK_*)
 * 2. Install record overrides
 * 3. App manifest defaults (fill-only)
 * 4. NAK environment
 * 5. Host environment
 */
inline std::unordered_map<std::string, std::string> compose_environment(
    const AppDeclaration& decl,
    const InstallRecord& install,
    const RuntimeDescriptor* runtime,
    const HostEnvironment& host_env,
    const LaunchContract& contract,
    CompositionTrace* trace = nullptr)
{
    std::unordered_map<std::string, std::string> env;
    
    auto record = [&](const std::string& key, const std::string& value,
                      const std::string& kind, const std::string& path,
                      int rank, EnvOp op, bool accepted) {
        if (trace) {
            TraceContribution contrib;
            contrib.value = value;
            contrib.source_kind = kind;
            contrib.source_path = path;
            contrib.precedence_rank = rank;
            contrib.operation = op;
            contrib.accepted = accepted;
            trace->environment[key].history.push_back(contrib);
        }
    };
    
    // Layer 1: Host environment (rank 5)
    for (const auto& [key, val] : host_env.vars) {
        auto result = apply_env_op(key, val, env);
        if (result.has_value()) {
            env[key] = *result;
            record(key, *result, trace_source::HOST, host_env.source_path, 5, val.op, true);
        } else {
            env.erase(key);
            record(key, "", trace_source::HOST, host_env.source_path, 5, val.op, true);
        }
    }
    
    // Layer 2: NAK environment (rank 4)
    if (runtime) {
        for (const auto& [key, val] : runtime->environment) {
            auto result = apply_env_op(key, val, env);
            if (result.has_value()) {
                env[key] = *result;
                record(key, *result, trace_source::NAK_RECORD, runtime->source_path, 4, val.op, true);
            } else {
                env.erase(key);
                record(key, "", trace_source::NAK_RECORD, runtime->source_path, 4, val.op, true);
            }
        }
    }
    
    // Layer 3: App manifest defaults (rank 3, fill-only)
    for (const auto& env_var : decl.env_vars) {
        auto eq = env_var.find('=');
        if (eq != std::string::npos) {
            std::string key = env_var.substr(0, eq);
            std::string val = env_var.substr(eq + 1);
            bool accepted = (env.find(key) == env.end());
            if (accepted) {
                env[key] = val;
            }
            record(key, val, trace_source::MANIFEST, "manifest", 3, EnvOp::Set, accepted);
        }
    }
    
    // Layer 4: Install record overrides (rank 2)
    for (const auto& [key, val] : install.overrides.environment) {
        auto result = apply_env_op(key, val, env);
        if (result.has_value()) {
            env[key] = *result;
            record(key, *result, trace_source::INSTALL_RECORD, install.source_path, 2, val.op, true);
        } else {
            env.erase(key);
            record(key, "", trace_source::INSTALL_RECORD, install.source_path, 2, val.op, true);
        }
    }
    
    // Layer 5: NAH standard variables (rank 1, always set)
    env["NAH_APP_ID"] = contract.app.id;
    record("NAH_APP_ID", contract.app.id, trace_source::NAH_STANDARD, "nah", 1, EnvOp::Set, true);
    
    env["NAH_APP_VERSION"] = contract.app.version;
    record("NAH_APP_VERSION", contract.app.version, trace_source::NAH_STANDARD, "nah", 1, EnvOp::Set, true);
    
    env["NAH_APP_ROOT"] = contract.app.root;
    record("NAH_APP_ROOT", contract.app.root, trace_source::NAH_STANDARD, "nah", 1, EnvOp::Set, true);
    
    env["NAH_APP_ENTRY"] = contract.app.entrypoint;
    record("NAH_APP_ENTRY", contract.app.entrypoint, trace_source::NAH_STANDARD, "nah", 1, EnvOp::Set, true);
    
    if (runtime) {
        env["NAH_NAK_ID"] = runtime->nak.id;
        record("NAH_NAK_ID", runtime->nak.id, trace_source::NAH_STANDARD, "nah", 1, EnvOp::Set, true);
        
        env["NAH_NAK_VERSION"] = runtime->nak.version;
        record("NAH_NAK_VERSION", runtime->nak.version, trace_source::NAH_STANDARD, "nah", 1, EnvOp::Set, true);
        
        env["NAH_NAK_ROOT"] = runtime->paths.root;
        record("NAH_NAK_ROOT", runtime->paths.root, trace_source::NAH_STANDARD, "nah", 1, EnvOp::Set, true);
    }
    
    return env;
}

// ============================================================================
// PURE FUNCTIONS - Timestamp Comparison
// ============================================================================

/**
 * Normalize RFC3339 timestamp.
 * 
 * Converts +00:00/-00:00 to Z for consistent comparison.
 */
inline std::string normalize_rfc3339(const std::string& ts) {
    if (ts.empty()) return ts;
    
    std::string result = ts;
    if (result.size() >= 6) {
        std::string suffix = result.substr(result.size() - 6);
        if (suffix == "+00:00" || suffix == "-00:00") {
            result = result.substr(0, result.size() - 6) + "Z";
        }
    }
    
    return result;
}

/**
 * Compare RFC3339 timestamps.
 * 
 * Returns true if a < b.
 */
inline bool timestamp_before(const std::string& a, const std::string& b) {
    return normalize_rfc3339(a) < normalize_rfc3339(b);
}

// ============================================================================
// MAIN COMPOSITION FUNCTION
// ============================================================================

// Compose a launch contract from app declaration and host state.
//
// This is the main entry point. Given:
//   - app:       What the application declares it needs
//   - host_env:  Host-provided environment variables
//   - install:   Where the app is installed and which runtime to use
//   - inventory: Available runtimes on the host
//
// Returns a CompositionResult. Check result.ok - if true, result.contract
// contains everything needed to launch the application.
//
// This function is pure: no I/O, no syscalls, no side effects. Same inputs
// always produce the same output. This makes it safe to call from any context
// and easy to test.
//
// Example:
//
//     AppDeclaration app;
//     app.id = "com.example.game";
//     app.version = "1.0.0";
//     app.entrypoint_path = "main.lua";
//     app.nak_id = "lua";
//
//     InstallRecord install;
//     install.install.instance_id = "abc123";
//     install.paths.install_root = "/apps/game";
//     install.nak.record_ref = "lua@5.4.6.json";
//
//     HostEnvironment host_env;  // Empty = no host overrides
//
//     RuntimeInventory inventory;
//     inventory.runtimes["lua@5.4.6.json"] = lua_runtime;
//
//     auto result = nah_compose(app, host_env, install, inventory);
//     if (result.ok) {
//         // result.contract.execution.binary = "/runtimes/lua/bin/lua"
//         // result.contract.execution.arguments = ["/apps/game/main.lua"]
//         // result.contract.environment = {"LUA_PATH": "...", "NAH_APP_ID": "com.example.game", ...}
//     }
//
inline CompositionResult nah_compose(
    const AppDeclaration& app,
    const HostEnvironment& host_env,
    const InstallRecord& install,
    const RuntimeInventory& inventory,
    const CompositionOptions& options = {})
{
    CompositionResult result;
    
    // Initialize trace if enabled
    CompositionTrace* trace_ptr = nullptr;
    if (options.enable_trace) {
        result.trace = CompositionTrace{};
        trace_ptr = &(*result.trace);
        trace_ptr->decisions.push_back("Starting composition");
    }
    
    // Validate declaration
    auto decl_valid = validate_declaration(app);
    if (!decl_valid.ok) {
        result.critical_error = CriticalError::MANIFEST_MISSING;
        result.critical_error_context = decl_valid.errors.empty() ? 
            "invalid declaration" : decl_valid.errors[0];
        for (const auto& err : decl_valid.errors) {
            result.warnings.push_back({
                warning_to_string(Warning::invalid_manifest), "error", {{"reason", err}}
            });
        }
        if (trace_ptr) trace_ptr->decisions.push_back("FAILED: Declaration validation failed");
        return result;
    }
    if (trace_ptr) trace_ptr->decisions.push_back("Declaration validated");
    
    // Validate install record
    auto install_valid = validate_install_record(install);
    if (!install_valid.ok) {
        result.critical_error = CriticalError::INSTALL_RECORD_INVALID;
        result.critical_error_context = install_valid.errors.empty() ?
            "invalid install record" : install_valid.errors[0];
        if (trace_ptr) trace_ptr->decisions.push_back("FAILED: Install record validation failed");
        return result;
    }
    if (trace_ptr) trace_ptr->decisions.push_back("Install record validated");
    
    // Resolve runtime
    auto runtime_result = resolve_runtime(app, install, inventory);
    for (const auto& warn : runtime_result.warnings) {
        result.warnings.push_back({
            warning_to_string(Warning::nak_not_found), "warn", {{"reason", warn}}
        });
    }
    
    RuntimeDescriptor* runtime_ptr = runtime_result.resolved && !runtime_result.runtime.nak.id.empty()
        ? &runtime_result.runtime : nullptr;
    
    if (trace_ptr) {
        if (runtime_ptr) {
            trace_ptr->decisions.push_back("Runtime resolved: " + runtime_ptr->nak.id + "@" + runtime_ptr->nak.version);
        } else if (app.nak_id.empty()) {
            trace_ptr->decisions.push_back("Standalone app (no runtime)");
        } else {
            trace_ptr->decisions.push_back("Runtime not found");
        }
    }
    
    // Validate runtime if present
    if (runtime_ptr) {
        auto runtime_valid = validate_runtime(*runtime_ptr);
        if (!runtime_valid.ok) {
            result.critical_error = CriticalError::PATH_TRAVERSAL;
            result.critical_error_context = runtime_valid.errors.empty() ?
                "invalid runtime" : runtime_valid.errors[0];
            if (trace_ptr) trace_ptr->decisions.push_back("FAILED: Runtime validation failed");
            return result;
        }
    }
    
    // Populate basic contract fields
    LaunchContract& contract = result.contract;
    
    contract.app.id = app.id;
    contract.app.version = app.version;
    contract.app.root = install.paths.install_root;
    
    if (runtime_ptr) {
        contract.nak.id = runtime_ptr->nak.id;
        contract.nak.version = runtime_ptr->nak.version;
        contract.nak.root = runtime_ptr->paths.root;
        contract.nak.resource_root = runtime_ptr->paths.resource_root.empty() ?
            runtime_ptr->paths.root : runtime_ptr->paths.resource_root;
        contract.nak.record_ref = runtime_result.record_ref;
    }
    
    // Bind paths
    auto paths = bind_paths(app, install, runtime_ptr, host_env);
    if (!paths.ok) {
        result.critical_error = CriticalError::PATH_TRAVERSAL;
        result.critical_error_context = paths.violations.empty() ?
            "path binding failed" : paths.violations[0].context;
        result.policy_violations = paths.violations;
        if (trace_ptr) trace_ptr->decisions.push_back("FAILED: Path binding failed");
        return result;
    }
    
    contract.app.entrypoint = paths.entrypoint;
    contract.exports = paths.exports;
    if (trace_ptr) trace_ptr->decisions.push_back("Paths bound successfully");
    
    // Compose environment
    auto env = compose_environment(app, install, runtime_ptr, host_env, contract, trace_ptr);
    
    // Determine execution binary and arguments
    std::string pinned_loader = install.nak.loader;
    
    // Override loader if specified in options
    if (!options.loader_override.empty()) {
        pinned_loader = options.loader_override;
        if (trace_ptr) trace_ptr->decisions.push_back("Loader override requested: " + pinned_loader);
    }
    
    if (runtime_ptr && runtime_ptr->has_loaders()) {
        std::string effective_loader = pinned_loader;
        
        if (effective_loader.empty()) {
            if (runtime_ptr->loaders.count("default")) {
                effective_loader = "default";
                if (trace_ptr) trace_ptr->decisions.push_back("Auto-selected 'default' loader");
            } else if (runtime_ptr->loaders.size() == 1) {
                effective_loader = runtime_ptr->loaders.begin()->first;
                if (trace_ptr) trace_ptr->decisions.push_back("Auto-selected single loader: " + effective_loader);
            } else {
                result.warnings.push_back({
                    warning_to_string(Warning::nak_loader_required),
                    "warn",
                    {{"reason", "multiple loaders but none specified"}}
                });
                contract.execution.binary = contract.app.entrypoint;
                if (trace_ptr) trace_ptr->decisions.push_back("WARNING: Multiple loaders, using entrypoint");
            }
        } else {
            if (trace_ptr) trace_ptr->decisions.push_back("Using pinned loader: " + effective_loader);
        }
        
        if (!effective_loader.empty()) {
            auto it = runtime_ptr->loaders.find(effective_loader);
            if (it == runtime_ptr->loaders.end()) {
                result.critical_error = CriticalError::NAK_LOADER_INVALID;
                result.critical_error_context = "loader not found: " + effective_loader;
                if (trace_ptr) trace_ptr->decisions.push_back("FAILED: Loader not found");
                return result;
            }
            
            contract.execution.binary = it->second.exec_path;
            contract.execution.arguments = expand_string_vector(it->second.args_template, env);
        }
    } else {
        contract.execution.binary = contract.app.entrypoint;
        if (trace_ptr) trace_ptr->decisions.push_back("Using app entrypoint as binary");
    }
    
    // Apply argument overrides
    auto expanded_prepend = expand_string_vector(install.overrides.arguments.prepend, env);
    contract.execution.arguments.insert(
        contract.execution.arguments.begin(),
        expanded_prepend.begin(),
        expanded_prepend.end());
    
    auto expanded_entry_args = expand_string_vector(app.entrypoint_args, env);
    contract.execution.arguments.insert(
        contract.execution.arguments.end(),
        expanded_entry_args.begin(),
        expanded_entry_args.end());
    
    auto expanded_append = expand_string_vector(install.overrides.arguments.append, env);
    contract.execution.arguments.insert(
        contract.execution.arguments.end(),
        expanded_append.begin(),
        expanded_append.end());
    
    // Determine cwd
    if (runtime_ptr && runtime_ptr->execution.present && !runtime_ptr->execution.cwd.empty()) {
        auto cwd_expanded = expand_placeholders(runtime_ptr->execution.cwd, env);
        if (cwd_expanded.ok && is_absolute_path(cwd_expanded.value)) {
            contract.execution.cwd = cwd_expanded.value;
        } else if (cwd_expanded.ok) {
            contract.execution.cwd = join_path(runtime_ptr->paths.root, cwd_expanded.value);
        } else {
            contract.execution.cwd = contract.app.root;
        }
    } else {
        contract.execution.cwd = contract.app.root;
    }
    
    // Library paths
    contract.execution.library_path_env_key = get_library_path_env_key();
    contract.execution.library_paths = paths.library_paths;
    
    // Expand environment placeholders
    for (auto& [key, val] : env) {
        auto expanded = expand_placeholders(val, env);
        if (expanded.ok) {
            val = expanded.value;
        }
    }
    contract.environment = env;
    
    // Enforcement
    for (const auto& perm : app.permissions_filesystem) {
        contract.enforcement.filesystem.push_back(perm);
    }
    for (const auto& perm : app.permissions_network) {
        contract.enforcement.network.push_back(perm);
    }
    
    // Capability usage
    if (!app.permissions_filesystem.empty() || !app.permissions_network.empty()) {
        contract.capability_usage.present = true;
        for (const auto& perm : app.permissions_filesystem) {
            contract.capability_usage.required_capabilities.push_back("fs." + perm);
        }
        for (const auto& perm : app.permissions_network) {
            contract.capability_usage.required_capabilities.push_back("net." + perm);
        }
    }
    
    // Trust
    contract.trust = install.trust;
    
    if (install.trust.source.empty() && install.trust.evaluated_at.empty()) {
        contract.trust.state = TrustState::Unknown;
        result.warnings.push_back({warning_to_string(Warning::trust_state_unknown), "warn", {}});
    } else {
        switch (install.trust.state) {
            case TrustState::Verified:
                break;
            case TrustState::Unverified:
                result.warnings.push_back({warning_to_string(Warning::trust_state_unverified), "warn", {}});
                break;
            case TrustState::Failed:
                result.warnings.push_back({warning_to_string(Warning::trust_state_failed), "warn", {}});
                break;
            case TrustState::Unknown:
                result.warnings.push_back({warning_to_string(Warning::trust_state_unknown), "warn", {}});
                break;
        }
    }
    
    // Check trust staleness
    if (!install.trust.expires_at.empty() && !options.now.empty()) {
        if (timestamp_before(install.trust.expires_at, options.now)) {
            result.warnings.push_back({warning_to_string(Warning::trust_state_stale), "warn", {}});
            if (trace_ptr) trace_ptr->decisions.push_back("WARNING: Trust verification has expired");
        }
    }
    
    if (trace_ptr) trace_ptr->decisions.push_back("Composition completed successfully");
    
    result.ok = true;
    return result;
}

// ============================================================================
// JSON SERIALIZATION (Pure, No External Dependencies)
// ============================================================================

namespace json {

/**
 * Escape a string for JSON output.
 */
inline std::string escape(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

/**
 * Format a string as JSON.
 */
inline std::string str(const std::string& s) {
    return "\"" + escape(s) + "\"";
}

/**
 * Format a string map as JSON object (sorted keys).
 */
inline std::string object(const std::unordered_map<std::string, std::string>& m, size_t indent = 0) {
    if (m.empty()) return "{}";
    
    std::vector<std::string> keys;
    for (const auto& [k, _] : m) keys.push_back(k);
    std::sort(keys.begin(), keys.end());
    
    std::string pad(indent + 2, ' ');
    std::string result = "{\n";
    for (size_t i = 0; i < keys.size(); i++) {
        result += pad + str(keys[i]) + ": " + str(m.at(keys[i]));
        if (i < keys.size() - 1) result += ",";
        result += "\n";
    }
    result += std::string(indent, ' ') + "}";
    return result;
}

/**
 * Format a string vector as JSON array.
 */
inline std::string array(const std::vector<std::string>& v, size_t indent = 0) {
    if (v.empty()) return "[]";
    
    std::string pad(indent + 2, ' ');
    std::string result = "[\n";
    for (size_t i = 0; i < v.size(); i++) {
        result += pad + str(v[i]);
        if (i < v.size() - 1) result += ",";
        result += "\n";
    }
    result += std::string(indent, ' ') + "]";
    return result;
}

} // namespace json

/**
 * Serialize a launch contract to JSON.
 * 
 * Produces deterministic output (sorted keys, consistent formatting).
 */
inline std::string serialize_contract(const LaunchContract& c) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": \"" << NAH_CONTRACT_SCHEMA << "\",\n";
    
    // app
    out << "  \"app\": {\n";
    out << "    \"id\": " << json::str(c.app.id) << ",\n";
    out << "    \"version\": " << json::str(c.app.version) << ",\n";
    out << "    \"root\": " << json::str(c.app.root) << ",\n";
    out << "    \"entrypoint\": " << json::str(c.app.entrypoint) << "\n";
    out << "  },\n";
    
    // nak
    out << "  \"nak\": {\n";
    out << "    \"id\": " << json::str(c.nak.id) << ",\n";
    out << "    \"version\": " << json::str(c.nak.version) << ",\n";
    out << "    \"root\": " << json::str(c.nak.root) << ",\n";
    out << "    \"resource_root\": " << json::str(c.nak.resource_root) << ",\n";
    out << "    \"record_ref\": " << json::str(c.nak.record_ref) << "\n";
    out << "  },\n";
    
    // execution
    out << "  \"execution\": {\n";
    out << "    \"binary\": " << json::str(c.execution.binary) << ",\n";
    out << "    \"arguments\": " << json::array(c.execution.arguments, 4) << ",\n";
    out << "    \"cwd\": " << json::str(c.execution.cwd) << ",\n";
    out << "    \"library_path_env_key\": " << json::str(c.execution.library_path_env_key) << ",\n";
    out << "    \"library_paths\": " << json::array(c.execution.library_paths, 4) << "\n";
    out << "  },\n";
    
    // environment
    out << "  \"environment\": " << json::object(c.environment, 2) << ",\n";
    
    // enforcement
    out << "  \"enforcement\": {\n";
    out << "    \"filesystem\": " << json::array(c.enforcement.filesystem, 4) << ",\n";
    out << "    \"network\": " << json::array(c.enforcement.network, 4) << "\n";
    out << "  },\n";
    
    // trust
    out << "  \"trust\": {\n";
    out << "    \"state\": " << json::str(trust_state_to_string(c.trust.state)) << ",\n";
    out << "    \"source\": " << json::str(c.trust.source) << ",\n";
    out << "    \"evaluated_at\": " << json::str(c.trust.evaluated_at) << ",\n";
    out << "    \"expires_at\": " << json::str(c.trust.expires_at) << "\n";
    out << "  },\n";
    
    // capability_usage
    out << "  \"capability_usage\": {\n";
    out << "    \"present\": " << (c.capability_usage.present ? "true" : "false") << ",\n";
    out << "    \"required_capabilities\": " << json::array(c.capability_usage.required_capabilities, 4) << "\n";
    out << "  }\n";
    
    out << "}";
    return out.str();
}

/**
 * Serialize a composition result to JSON.
 */
inline std::string serialize_result(const CompositionResult& r) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"ok\": " << (r.ok ? "true" : "false") << ",\n";
    
    if (r.critical_error.has_value()) {
        out << "  \"critical_error\": " << json::str(critical_error_to_string(*r.critical_error)) << ",\n";
        out << "  \"critical_error_context\": " << json::str(r.critical_error_context) << ",\n";
    } else {
        out << "  \"critical_error\": null,\n";
    }
    
    // warnings
    out << "  \"warnings\": [\n";
    for (size_t i = 0; i < r.warnings.size(); i++) {
        const auto& w = r.warnings[i];
        out << "    {\n";
        out << "      \"key\": " << json::str(w.key) << ",\n";
        out << "      \"action\": " << json::str(w.action) << ",\n";
        out << "      \"fields\": " << json::object(w.fields, 6) << "\n";
        out << "    }";
        if (i < r.warnings.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ],\n";
    
    if (r.ok) {
        out << "  \"contract\": " << serialize_contract(r.contract) << "\n";
    } else {
        out << "  \"contract\": null\n";
    }
    
    out << "}";
    return out.str();
}

} // namespace core
} // namespace nah

#endif // __cplusplus

#endif // NAH_CORE_H
