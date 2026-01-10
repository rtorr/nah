#pragma once

#include "nah/types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace nah {

// ============================================================================
// Host Profile (per SPEC L499-L500, L580-L795)
// ============================================================================

struct HostProfile {
    std::string schema;  // MUST be "nah.host.profile.v1"
    
    // [nak] section (per SPEC L637-L652)
    struct {
        BindingMode binding_mode = BindingMode::Canonical;
        std::vector<std::string> allow_versions;  // prefix patterns ending in *
        std::vector<std::string> deny_versions;   // prefix patterns ending in *
        std::unordered_map<std::string, std::string> map;  // selection_key -> record_ref
    } nak;
    
    // [environment] section
    std::unordered_map<std::string, std::string> environment;
    
    // [paths] section
    struct {
        std::vector<std::string> library_prepend;
        std::vector<std::string> library_append;
    } paths;
    
    // [warnings] section - maps warning key to action
    std::unordered_map<std::string, WarningAction> warnings;
    
    // [capabilities] section - maps capability key to enforcement ID
    std::unordered_map<std::string, std::string> capabilities;
    
    // [overrides] section
    struct {
        OverrideMode mode = OverrideMode::Allow;
        std::vector<std::string> allow_keys;
    } overrides;
    
    // Source path for trace
    std::string source_path;
};

// Built-in empty profile (per SPEC L614-L628)
HostProfile get_builtin_empty_profile();

// ============================================================================
// Host Profile Parsing Result
// ============================================================================

struct HostProfileParseResult {
    bool ok = false;
    std::string error;
    HostProfile profile;
    std::vector<std::string> warnings;
};

// Parse a Host Profile from JSON string
HostProfileParseResult parse_host_profile_full(const std::string& json_str,
                                                const std::string& source_path = "");

// ============================================================================
// Warning Policy Application
// ============================================================================

// Get the effective action for a warning key given a profile
// Returns "warn" if the key is not present (per SPEC Default Warning Action)
WarningAction get_warning_action(const HostProfile& profile, Warning warning);
WarningAction get_warning_action(const HostProfile& profile, const std::string& warning_key);

// ============================================================================
// NAK Version Allow/Deny Matching (per SPEC L695)
// ============================================================================

// Check if a version matches a pattern (prefix with optional *)
bool version_matches_pattern(const std::string& version, const std::string& pattern);

// Check if a version is allowed by profile rules
// Deny rules take precedence over allow rules
bool version_allowed_by_profile(const std::string& version, const HostProfile& profile);

// ============================================================================
// Override Policy (per SPEC L701-L717)
// ============================================================================

// Check if an override target is permitted by profile policy
// Returns true if permitted, false if denied
bool is_override_permitted(const std::string& target, const HostProfile& profile);

// Legacy API for backward compatibility
struct HostProfileRecord {
    std::string schema;
    std::string binding_mode;
};

struct HostProfileValidation {
    bool ok;
    std::string error;
};

HostProfileValidation parse_host_profile(const std::string& json, HostProfileRecord& out);

} // namespace nah
