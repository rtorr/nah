#include "nah/types.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

namespace nah {

namespace {

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

} // namespace

std::optional<Warning> parse_warning_key(const std::string& key) {
    std::string lower = to_lower(key);
    
    if (lower == "invalid_manifest") return Warning::invalid_manifest;
    if (lower == "invalid_configuration") return Warning::invalid_configuration;
    if (lower == "profile_invalid") return Warning::profile_invalid;
    if (lower == "profile_missing") return Warning::profile_missing;
    if (lower == "profile_parse_error") return Warning::profile_parse_error;
    if (lower == "nak_pin_invalid") return Warning::nak_pin_invalid;
    if (lower == "nak_not_found") return Warning::nak_not_found;
    if (lower == "nak_version_unsupported") return Warning::nak_version_unsupported;
    if (lower == "binary_not_found") return Warning::binary_not_found;
    if (lower == "capability_missing") return Warning::capability_missing;
    if (lower == "capability_malformed") return Warning::capability_malformed;
    if (lower == "capability_unknown") return Warning::capability_unknown;
    if (lower == "missing_env_var") return Warning::missing_env_var;
    if (lower == "invalid_trust_state") return Warning::invalid_trust_state;
    if (lower == "override_denied") return Warning::override_denied;
    if (lower == "override_invalid") return Warning::override_invalid;
    if (lower == "invalid_library_path") return Warning::invalid_library_path;
    if (lower == "trust_state_unknown") return Warning::trust_state_unknown;
    if (lower == "trust_state_unverified") return Warning::trust_state_unverified;
    if (lower == "trust_state_failed") return Warning::trust_state_failed;
    if (lower == "trust_state_stale") return Warning::trust_state_stale;
    
    return std::nullopt;
}

std::optional<WarningAction> parse_warning_action(const std::string& s) {
    std::string lower = to_lower(s);
    if (lower == "warn") return WarningAction::Warn;
    if (lower == "ignore") return WarningAction::Ignore;
    if (lower == "error") return WarningAction::Error;
    return std::nullopt;
}

std::optional<TrustState> parse_trust_state(const std::string& s) {
    std::string lower = to_lower(s);
    if (lower == "verified") return TrustState::Verified;
    if (lower == "unverified") return TrustState::Unverified;
    if (lower == "failed") return TrustState::Failed;
    if (lower == "unknown") return TrustState::Unknown;
    return std::nullopt;
}

std::optional<OverrideMode> parse_override_mode(const std::string& s) {
    std::string lower = to_lower(s);
    if (lower == "allow") return OverrideMode::Allow;
    if (lower == "deny") return OverrideMode::Deny;
    if (lower == "allowlist") return OverrideMode::Allowlist;
    return std::nullopt;
}

std::optional<BindingMode> parse_binding_mode(const std::string& s) {
    std::string lower = to_lower(s);
    if (lower == "canonical") return BindingMode::Canonical;
    if (lower == "mapped") return BindingMode::Mapped;
    return std::nullopt;
}

} // namespace nah
