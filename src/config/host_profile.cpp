#include "nah/host_profile.hpp"

#include <toml++/toml.h>
#include <algorithm>
#include <cctype>

namespace nah {

namespace {

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

} // namespace

HostProfile get_builtin_empty_profile() {
    HostProfile profile;
    profile.schema = "nah.host.profile.v1";
    profile.nak.binding_mode = BindingMode::Canonical;
    // Default warning actions per SPEC Built-in Empty Profile
    profile.warnings["nak_not_found"] = WarningAction::Warn;
    profile.warnings["nak_version_unsupported"] = WarningAction::Warn;
    profile.warnings["profile_missing"] = WarningAction::Warn;
    profile.overrides.mode = OverrideMode::Allow;
    return profile;
}

HostProfileParseResult parse_host_profile_full(const std::string& toml_str,
                                                const std::string& source_path) {
    HostProfileParseResult result;
    result.profile.source_path = source_path;
    
    try {
        auto tbl = toml::parse(toml_str);
        
        // schema (REQUIRED)
        if (auto schema = tbl["schema"].value<std::string>()) {
            result.profile.schema = trim(*schema);
        } else {
            result.error = "schema missing";
            return result;
        }
        
        if (result.profile.schema != "nah.host.profile.v1") {
            result.error = "schema mismatch: expected nah.host.profile.v1";
            return result;
        }
        
        // [nak] section
        if (auto nak_tbl = tbl["nak"].as_table()) {
            // binding_mode
            if (auto mode = (*nak_tbl)["binding_mode"].value<std::string>()) {
                auto parsed = parse_binding_mode(*mode);
                if (parsed) {
                    result.profile.nak.binding_mode = *parsed;
                } else {
                    result.warnings.push_back("invalid_configuration:invalid_binding_mode");
                    result.profile.nak.binding_mode = BindingMode::Canonical;
                }
            }
            
            // allow_versions
            if (auto arr = (*nak_tbl)["allow_versions"].as_array()) {
                for (const auto& elem : *arr) {
                    if (auto s = elem.value<std::string>()) {
                        result.profile.nak.allow_versions.push_back(*s);
                    }
                }
            }
            
            // deny_versions
            if (auto arr = (*nak_tbl)["deny_versions"].as_array()) {
                for (const auto& elem : *arr) {
                    if (auto s = elem.value<std::string>()) {
                        result.profile.nak.deny_versions.push_back(*s);
                    }
                }
            }
            
            // [nak.map] for mapped mode
            if (auto map_tbl = (*nak_tbl)["map"].as_table()) {
                for (const auto& [key, val] : *map_tbl) {
                    if (auto s = val.value<std::string>()) {
                        result.profile.nak.map[std::string(key.str())] = *s;
                    }
                }
            }
        }
        
        // [environment] section
        if (auto env_tbl = tbl["environment"].as_table()) {
            for (const auto& [key, val] : *env_tbl) {
                if (auto s = val.value<std::string>()) {
                    result.profile.environment[std::string(key.str())] = *s;
                }
            }
        }
        
        // [paths] section
        if (auto paths_tbl = tbl["paths"].as_table()) {
            if (auto arr = (*paths_tbl)["library_prepend"].as_array()) {
                for (const auto& elem : *arr) {
                    if (auto s = elem.value<std::string>()) {
                        result.profile.paths.library_prepend.push_back(*s);
                    }
                }
            }
            if (auto arr = (*paths_tbl)["library_append"].as_array()) {
                for (const auto& elem : *arr) {
                    if (auto s = elem.value<std::string>()) {
                        result.profile.paths.library_append.push_back(*s);
                    }
                }
            }
        }
        
        // [warnings] section
        if (auto warn_tbl = tbl["warnings"].as_table()) {
            for (const auto& [key, val] : *warn_tbl) {
                if (auto s = val.value<std::string>()) {
                    std::string key_str = to_lower(std::string(key.str()));
                    auto action = parse_warning_action(*s);
                    if (action) {
                        result.profile.warnings[key_str] = *action;
                    } else {
                        result.warnings.push_back("invalid_configuration:invalid_warning_action:" + key_str);
                    }
                }
            }
        }
        
        // [capabilities] section
        if (auto cap_tbl = tbl["capabilities"].as_table()) {
            for (const auto& [key, val] : *cap_tbl) {
                if (auto s = val.value<std::string>()) {
                    result.profile.capabilities[std::string(key.str())] = *s;
                }
            }
        }
        
        // [overrides] section
        if (auto ovr_tbl = tbl["overrides"].as_table()) {
            if (auto mode = (*ovr_tbl)["mode"].value<std::string>()) {
                auto parsed = parse_override_mode(*mode);
                if (parsed) {
                    result.profile.overrides.mode = *parsed;
                } else {
                    result.warnings.push_back("invalid_configuration:invalid_override_mode");
                }
            }
            
            if (auto arr = (*ovr_tbl)["allow_keys"].as_array()) {
                for (const auto& elem : *arr) {
                    if (auto s = elem.value<std::string>()) {
                        result.profile.overrides.allow_keys.push_back(*s);
                    }
                }
            }
        }
        
        result.ok = true;
        return result;
        
    } catch (const toml::parse_error& e) {
        result.error = std::string("parse error: ") + e.description().data();
        return result;
    }
}

WarningAction get_warning_action(const HostProfile& profile, Warning warning) {
    return get_warning_action(profile, warning_to_string(warning));
}

WarningAction get_warning_action(const HostProfile& profile, const std::string& warning_key) {
    std::string key = to_lower(warning_key);
    auto it = profile.warnings.find(key);
    if (it != profile.warnings.end()) {
        return it->second;
    }
    // Default: warn (per SPEC Default Warning Action)
    return WarningAction::Warn;
}

bool version_matches_pattern(const std::string& version, const std::string& pattern) {
    if (pattern.empty()) return false;
    
    // Check if pattern ends with *
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.size() - 1);
        return version.compare(0, prefix.size(), prefix) == 0;
    }
    
    // Exact match
    return version == pattern;
}

bool version_allowed_by_profile(const std::string& version, const HostProfile& profile) {
    // Deny rules take precedence over allow rules (per SPEC L695)
    for (const auto& pattern : profile.nak.deny_versions) {
        if (version_matches_pattern(version, pattern)) {
            return false;
        }
    }
    
    // If allow_versions is empty, all versions are allowed unless denied
    if (profile.nak.allow_versions.empty()) {
        return true;
    }
    
    // Check allow rules
    for (const auto& pattern : profile.nak.allow_versions) {
        if (version_matches_pattern(version, pattern)) {
            return true;
        }
    }
    
    return false;
}

bool is_override_permitted(const std::string& target, const HostProfile& profile) {
    // Supported override targets in v1.0 (per SPEC Override Policy)
    bool is_environment = (target == "ENVIRONMENT");
    bool is_warnings = (target.rfind("WARNINGS_", 0) == 0);
    bool is_supported = is_environment || is_warnings;
    
    if (!is_supported) {
        // Non-standard target - always denied
        return false;
    }
    
    switch (profile.overrides.mode) {
        case OverrideMode::Deny:
            return false;
            
        case OverrideMode::Allow:
            return true;
            
        case OverrideMode::Allowlist:
            // Check if target matches any allowlist pattern
            for (const auto& pattern : profile.overrides.allow_keys) {
                if (pattern.back() == '*') {
                    std::string prefix = pattern.substr(0, pattern.size() - 1);
                    if (target.compare(0, prefix.size(), prefix) == 0) {
                        return true;
                    }
                } else if (pattern == target) {
                    return true;
                }
            }
            return false;
    }
    
    return false;
}

// Legacy API implementation
HostProfileValidation parse_host_profile(const std::string& toml_str, HostProfileRecord& out) {
    auto result = parse_host_profile_full(toml_str);
    if (!result.ok) {
        return {false, result.error};
    }
    out.schema = result.profile.schema;
    out.binding_mode = binding_mode_to_string(result.profile.nak.binding_mode);
    return {true, {}};
}

} // namespace nah
