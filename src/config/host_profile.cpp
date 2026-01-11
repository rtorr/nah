#include "nah/host_profile.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

#include <nlohmann/json.hpp>

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

// Helper to safely get a string from JSON
std::optional<std::string> get_string(const nlohmann::json& j, const std::string& key) {
    if (j.contains(key) && j[key].is_string()) {
        return j[key].get<std::string>();
    }
    return std::nullopt;
}

// Helper to safely get a string array from JSON
std::vector<std::string> get_string_array(const nlohmann::json& j, const std::string& key) {
    std::vector<std::string> result;
    if (j.contains(key) && j[key].is_array()) {
        for (const auto& elem : j[key]) {
            if (elem.is_string()) {
                result.push_back(elem.get<std::string>());
            }
        }
    }
    return result;
}

} // namespace

HostProfile get_builtin_empty_profile() {
    HostProfile profile;
    profile.schema = "nah.host.profile.v2";
    profile.nak.binding_mode = BindingMode::Canonical;
    // Default warning actions per SPEC Built-in Empty Profile
    profile.warnings["nak_not_found"] = WarningAction::Warn;
    profile.warnings["nak_version_unsupported"] = WarningAction::Warn;
    profile.warnings["profile_missing"] = WarningAction::Warn;
    profile.overrides.mode = OverrideMode::Allow;
    return profile;
}

HostProfileParseResult parse_host_profile_full(const std::string& json_str,
                                                const std::string& source_path) {
    HostProfileParseResult result;
    result.profile.source_path = source_path;
    
    try {
        auto j = nlohmann::json::parse(json_str);
        
        if (!j.is_object()) {
            result.error = "JSON must be an object";
            return result;
        }
        
        // $schema (REQUIRED)
        if (auto schema = get_string(j, "$schema")) {
            result.profile.schema = trim(*schema);
        } else {
            result.error = "$schema missing";
            return result;
        }
        
        if (result.profile.schema != "nah.host.profile.v2") {
            result.error = "$schema mismatch: expected nah.host.profile.v2";
            return result;
        }
        
        // "nak" section
        if (j.contains("nak") && j["nak"].is_object()) {
            const auto& nak = j["nak"];
            
            // binding_mode
            if (auto mode = get_string(nak, "binding_mode")) {
                auto parsed = parse_binding_mode(*mode);
                if (parsed) {
                    result.profile.nak.binding_mode = *parsed;
                } else {
                    result.warnings.push_back("invalid_configuration:invalid_binding_mode");
                    result.profile.nak.binding_mode = BindingMode::Canonical;
                }
            }
            
            // allow_versions
            result.profile.nak.allow_versions = get_string_array(nak, "allow_versions");
            
            // deny_versions
            result.profile.nak.deny_versions = get_string_array(nak, "deny_versions");
            
            // "map" for mapped mode
            if (nak.contains("map") && nak["map"].is_object()) {
                for (auto& [key, val] : nak["map"].items()) {
                    if (val.is_string()) {
                        result.profile.nak.map[key] = val.get<std::string>();
                    }
                }
            }
        }
        
        // "environment" section
        if (j.contains("environment") && j["environment"].is_object()) {
            for (auto& [key, val] : j["environment"].items()) {
                if (val.is_string()) {
                    result.profile.environment[key] = val.get<std::string>();
                }
            }
        }
        
        // "paths" section
        if (j.contains("paths") && j["paths"].is_object()) {
            const auto& paths = j["paths"];
            result.profile.paths.library_prepend = get_string_array(paths, "library_prepend");
            result.profile.paths.library_append = get_string_array(paths, "library_append");
        }
        
        // "warnings" section
        if (j.contains("warnings") && j["warnings"].is_object()) {
            for (auto& [key, val] : j["warnings"].items()) {
                if (val.is_string()) {
                    std::string key_str = to_lower(key);
                    auto action = parse_warning_action(val.get<std::string>());
                    if (action) {
                        result.profile.warnings[key_str] = *action;
                    } else {
                        result.warnings.push_back("invalid_configuration:invalid_warning_action:" + key_str);
                    }
                }
            }
        }
        
        // "capabilities" section
        if (j.contains("capabilities") && j["capabilities"].is_object()) {
            for (auto& [key, val] : j["capabilities"].items()) {
                if (val.is_string()) {
                    result.profile.capabilities[key] = val.get<std::string>();
                }
            }
        }
        
        // "overrides" section
        if (j.contains("overrides") && j["overrides"].is_object()) {
            const auto& ovr = j["overrides"];
            
            if (auto mode = get_string(ovr, "mode")) {
                auto parsed = parse_override_mode(*mode);
                if (parsed) {
                    result.profile.overrides.mode = *parsed;
                } else {
                    result.warnings.push_back("invalid_configuration:invalid_override_mode");
                }
            }
            
            result.profile.overrides.allow_keys = get_string_array(ovr, "allow_keys");
        }
        
        result.ok = true;
        return result;
        
    } catch (const nlohmann::json::parse_error& e) {
        result.error = std::string("parse error: ") + e.what();
        return result;
    } catch (const nlohmann::json::exception& e) {
        result.error = std::string("JSON error: ") + e.what();
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
HostProfileValidation parse_host_profile(const std::string& json_str, HostProfileRecord& out) {
    auto result = parse_host_profile_full(json_str);
    if (!result.ok) {
        return {false, result.error};
    }
    out.schema = result.profile.schema;
    out.binding_mode = binding_mode_to_string(result.profile.nak.binding_mode);
    return {true, {}};
}

} // namespace nah
