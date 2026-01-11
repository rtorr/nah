#pragma once

#include "nah/types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace nah {

// ============================================================================
// App Install Record (per SPEC L285-L379)
// ============================================================================

struct AppInstallRecord {
    // [install] section
    struct {
        std::string instance_id;  // UUID string, unique per install
    } install;
    
    // [app] section (audit snapshots only; MUST NOT affect behavior)
    struct {
        std::string id;
        std::string version;
        std::string nak_id;
        std::string nak_version_req;
    } app;
    
    // [nak] section (pinned NAK)
    struct {
        std::string id;
        std::string version;
        std::string record_ref;  // "<nak_id>@<version>.json"
        std::string loader;      // Pinned loader name (resolved at install time)
        std::string selection_reason;  // optional audit-only string
    } nak;
    
    // [paths] section
    struct {
        std::string install_root;  // Absolute path to app root
    } paths;
    
    // [provenance] section
    struct {
        std::string package_hash;  // "sha256:..."
        std::string installed_at;  // RFC3339 timestamp
        std::string installed_by;
        std::string source;        // e.g., "package.nap"
    } provenance;
    
    // [trust] section
    TrustInfo trust;
    
    // [verification] section
    struct {
        std::string last_verified_at;
        std::string last_verifier_version;
    } verification;
    
    // [overrides] section
    struct {
        EnvMap environment;
        struct {
            std::vector<std::string> prepend;
            std::vector<std::string> append;
        } arguments;
        struct {
            std::vector<std::string> library_prepend;
        } paths;
    } overrides;
    
    // Source path for trace
    std::string source_path;
};

// ============================================================================
// App Install Record Parsing Result
// ============================================================================

struct AppInstallRecordParseResult {
    bool ok = false;
    bool is_critical_error = false;  // True if schema/required fields missing
    std::string error;
    AppInstallRecord record;
    std::vector<std::string> warnings;
};

// Parse an App Install Record from JSON string
AppInstallRecordParseResult parse_app_install_record_full(const std::string& json_str,
                                                           const std::string& source_path = "");

// ============================================================================
// App Install Record Validation
// ============================================================================

// Validate required fields per SPEC Presence semantics
// Returns true if all required fields are present and valid
bool validate_app_install_record(const AppInstallRecord& record, std::string& error);

// ============================================================================
// Legacy API for backward compatibility
// ============================================================================

struct InstallRecordValidation {
    bool ok;
    std::string error;
};

InstallRecordValidation parse_app_install_record(const std::string& json,
                                                  AppInstallRecord& out);

} // namespace nah
