#pragma once

#include "nah/types.hpp"
#include "nah/host_profile.hpp"

#include <string>
#include <vector>
#include <unordered_map>

namespace nah {

// ============================================================================
// Warning Helpers
// ============================================================================

// Create warning fields map from vector of warning objects
std::unordered_map<std::string, std::string> create_warning_fields(
    const std::vector<WarningObject>& warnings);

// ============================================================================
// Warning Collector (per SPEC Warning System)
// ============================================================================

class WarningCollector {
public:
    // Default constructor with no policy
    WarningCollector() = default;
    
    // Constructor with policy map
    explicit WarningCollector(const std::unordered_map<std::string, WarningAction>& policy)
        : policy_(policy) {}
    
    // Constructor with profile pointer (for backward compatibility)
    explicit WarningCollector(const HostProfile* profile);
    
    // Set/update the profile for warning policy
    void set_profile(const HostProfile* profile);
    
    // Emit a warning with fields
    void emit(Warning warning, const std::unordered_map<std::string, std::string>& fields);
    
    // Emit a warning with no fields
    void emit(Warning warning);
    
    // Emit a warning with context string (convenience)
    void emit_with_context(Warning warning, const std::string& context);
    
    // Emit a warning by key string (for dynamic warning keys)
    void emit(const std::string& warning_key, std::unordered_map<std::string, std::string> fields = {});
    
    // Apply override to warning policy (per SPEC NAH_OVERRIDE_WARNINGS_*)
    void apply_override(const std::string& warning_key, WarningAction action);
    
    // Get all emitted warnings after policy application
    // Warnings with action "ignore" are excluded
    std::vector<WarningObject> get_warnings() const;
    
    // Check if any warning was upgraded to error
    bool has_errors() const;
    
    // Check if any effective warnings remain (excluding ignored)
    bool has_effective_warnings() const;
    
    // Clear all collected warnings
    void clear();

private:
    struct CollectedWarning {
        std::string key;
        std::unordered_map<std::string, std::string> fields;
        WarningAction effective_action;
    };
    
    std::unordered_map<std::string, WarningAction> policy_;
    std::vector<CollectedWarning> warnings_;
    std::unordered_map<std::string, WarningAction> overrides_;
    
    WarningAction get_effective_action(const std::string& key) const;
};

// ============================================================================
// Convenience functions for emitting specific warnings
// ============================================================================

namespace warnings {

// missing_env_var (per SPEC L1087)
inline std::unordered_map<std::string, std::string> missing_env_var(
    const std::string& var_name,
    const std::string& source_path) {
    return {{"missing", var_name}, {"source_path", source_path}};
}

// override_denied (per SPEC L1088)
inline std::unordered_map<std::string, std::string> override_denied(
    const std::string& target,
    const std::string& source_kind,
    const std::string& source_ref) {
    return {{"target", target}, {"source_kind", source_kind}, {"source_ref", source_ref}};
}

// override_invalid (per SPEC L1089)
inline std::unordered_map<std::string, std::string> override_invalid(
    const std::string& target,
    const std::string& reason,
    const std::string& source_kind,
    const std::string& source_ref) {
    return {{"target", target}, {"reason", reason}, 
            {"source_kind", source_kind}, {"source_ref", source_ref}};
}

// capability_missing (per SPEC L1090)
inline std::unordered_map<std::string, std::string> capability_missing(
    const std::string& capability) {
    return {{"capability", capability}};
}

// capability_malformed (per SPEC L1091)
inline std::unordered_map<std::string, std::string> capability_malformed(
    const std::string& permission) {
    return {{"permission", permission}};
}

// capability_unknown (per SPEC L1092)
inline std::unordered_map<std::string, std::string> capability_unknown(
    const std::string& operation) {
    return {{"operation", operation}};
}

// invalid_library_path (per SPEC L1093)
inline std::unordered_map<std::string, std::string> invalid_library_path(
    const std::string& value,
    const std::string& source_path) {
    return {{"value", value}, {"source_path", source_path}};
}

// invalid_configuration (per SPEC L1086)
inline std::unordered_map<std::string, std::string> invalid_configuration(
    const std::string& reason,
    const std::string& source_path,
    const std::string& fields = "") {
    std::unordered_map<std::string, std::string> result = {
        {"reason", reason}, {"source_path", source_path}
    };
    if (!fields.empty()) {
        result["fields"] = fields;
    }
    return result;
}

// profile_parse_error
inline std::unordered_map<std::string, std::string> profile_parse_error(
    const std::string& source_path) {
    return {{"source_path", source_path}};
}

} // namespace warnings

} // namespace nah
