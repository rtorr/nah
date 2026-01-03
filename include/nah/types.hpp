#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nah {

// ============================================================================
// Warning System (per SPEC L1359-L1383)
// ============================================================================

enum class Warning {
    invalid_manifest,
    invalid_configuration,
    profile_invalid,
    profile_missing,
    profile_parse_error,
    nak_pin_invalid,
    nak_not_found,           // Install-time only; MUST NOT be emitted by compose_contract
    nak_version_unsupported,
    binary_not_found,        // Diagnostic only; MUST NOT be emitted by compose_contract
    capability_missing,
    capability_malformed,
    capability_unknown,
    missing_env_var,
    invalid_trust_state,
    override_denied,
    override_invalid,
    invalid_library_path,
    trust_state_unknown,
    trust_state_unverified,
    trust_state_failed,
    trust_state_stale,
};

// Convert warning enum to canonical lowercase snake_case string
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
        default: return "unknown";
    }
}

// Parse warning key string to enum (case-insensitive)
std::optional<Warning> parse_warning_key(const std::string& key);

// ============================================================================
// Warning Action (per SPEC Host Profile warnings section)
// ============================================================================

enum class WarningAction {
    Warn,
    Ignore,
    Error
};

inline const char* action_to_string(WarningAction a) {
    switch (a) {
        case WarningAction::Warn: return "warn";
        case WarningAction::Ignore: return "ignore";
        case WarningAction::Error: return "error";
        default: return "warn";
    }
}

std::optional<WarningAction> parse_warning_action(const std::string& s);

// ============================================================================
// Critical Errors (per SPEC L1386-L1391)
// ============================================================================

enum class CriticalError {
    MANIFEST_MISSING,
    ENTRYPOINT_NOT_FOUND,
    PATH_TRAVERSAL,
    INSTALL_RECORD_INVALID,
};

inline const char* critical_error_to_string(CriticalError e) {
    switch (e) {
        case CriticalError::MANIFEST_MISSING: return "MANIFEST_MISSING";
        case CriticalError::ENTRYPOINT_NOT_FOUND: return "ENTRYPOINT_NOT_FOUND";
        case CriticalError::PATH_TRAVERSAL: return "PATH_TRAVERSAL";
        case CriticalError::INSTALL_RECORD_INVALID: return "INSTALL_RECORD_INVALID";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Warning Object (per SPEC L1636-L1640)
// ============================================================================

struct WarningObject {
    std::string key;                                      // lowercase snake_case
    std::string action;                                   // "warn" | "error"
    std::unordered_map<std::string, std::string> fields;  // warning-specific
};

// ============================================================================
// Trust State (per SPEC L470-L471)
// ============================================================================

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
        default: return "unknown";
    }
}

std::optional<TrustState> parse_trust_state(const std::string& s);

// ============================================================================
// Trust Info (per SPEC L339-L359, L1280-L1285)
// ============================================================================

struct TrustInfo {
    TrustState state = TrustState::Unknown;
    std::string source;
    std::string evaluated_at;  // RFC3339 timestamp
    std::string expires_at;    // RFC3339 timestamp (optional)
    std::string inputs_hash;   // optional
    std::unordered_map<std::string, std::string> details;  // host-defined opaque metadata
};

// ============================================================================
// Asset Export (per SPEC L1287, L1305)
// ============================================================================

struct AssetExport {
    std::string id;
    std::string path;  // absolute path under app.root
    std::string type;  // optional
};

// ============================================================================
// Capability Usage (per SPEC L1290-L1294)
// ============================================================================

struct CapabilityUsage {
    bool present = false;
    std::vector<std::string> required_capabilities;
    std::vector<std::string> optional_capabilities;   // empty in v1.0
    std::vector<std::string> critical_capabilities;   // empty in v1.0
};

// ============================================================================
// Launch Contract (per SPEC L1248-L1296)
// ============================================================================

struct LaunchContract {
    struct {
        std::string id;
        std::string version;
        std::string root;
        std::string entrypoint;
    } app;

    struct {
        std::string id;
        std::string version;
        std::string root;
        std::string resource_root;
        std::string record_ref;
    } nak;

    struct {
        std::string binary;
        std::vector<std::string> arguments;
        std::string cwd;
        std::string library_path_env_key;
        std::vector<std::string> library_paths;
    } execution;

    std::unordered_map<std::string, std::string> environment;

    struct {
        std::vector<std::string> filesystem;
        std::vector<std::string> network;
    } enforcement;

    TrustInfo trust;

    std::unordered_map<std::string, AssetExport> exports;

    CapabilityUsage capability_usage;
};

// ============================================================================
// Trace Entry (per SPEC L1642-L1647)
// ============================================================================

struct TraceEntry {
    std::string value;
    std::string source_kind;   // profile | nak_record | manifest | install_record | process_env | overrides_file | standard
    std::string source_path;
    int precedence_rank;       // 1..7
};

// ============================================================================
// Contract Envelope (per SPEC L1654-L1658)
// ============================================================================

struct ContractEnvelope {
    LaunchContract contract;
    std::vector<WarningObject> warnings;
    std::optional<std::unordered_map<std::string, std::unordered_map<std::string, TraceEntry>>> trace;
};

// ============================================================================
// Override Mode (per SPEC Host Profile overrides section)
// ============================================================================

enum class OverrideMode {
    Allow,
    Deny,
    Allowlist
};

inline const char* override_mode_to_string(OverrideMode m) {
    switch (m) {
        case OverrideMode::Allow: return "allow";
        case OverrideMode::Deny: return "deny";
        case OverrideMode::Allowlist: return "allowlist";
        default: return "allow";
    }
}

std::optional<OverrideMode> parse_override_mode(const std::string& s);

// ============================================================================
// Binding Mode (per SPEC L637-L652)
// ============================================================================

enum class BindingMode {
    Canonical,
    Mapped
};

inline const char* binding_mode_to_string(BindingMode m) {
    switch (m) {
        case BindingMode::Canonical: return "canonical";
        case BindingMode::Mapped: return "mapped";
        default: return "canonical";
    }
}

std::optional<BindingMode> parse_binding_mode(const std::string& s);

// ============================================================================
// Capability (per SPEC L1105-L1108)
// ============================================================================

struct Capability {
    std::string key;        // Full capability key (e.g., "fs.read./path")
    std::string selector;   // Resource selector (opaque) - deprecated, use resource
    std::string operation;  // The operation (read, write, execute, connect, listen, bind)
    std::string resource;   // The resource path or URL
};

// ============================================================================
// NAK Pin (per SPEC L1154-L1158)
// ============================================================================

struct NakPin {
    std::string id;
    std::string version;
    std::string record_ref;
};

// PinnedNakLoadResult is defined in nak_selection.hpp with full NakInstallRecord

} // namespace nah
