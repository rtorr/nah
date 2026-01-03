#include "nah/warnings.hpp"

#include <algorithm>

namespace nah {

// ============================================================================
// Warning Helpers
// ============================================================================

std::unordered_map<std::string, std::string> create_warning_fields(
    const std::vector<WarningObject>& warnings) {
    std::unordered_map<std::string, std::string> result;
    for (const auto& w : warnings) {
        result[w.key] = w.action;
    }
    return result;
}

// ============================================================================
// WarningCollector Implementation
// ============================================================================

WarningCollector::WarningCollector(const HostProfile* profile) {
    if (profile) {
        policy_ = profile->warnings;
    }
}

void WarningCollector::set_profile(const HostProfile* profile) {
    if (profile) {
        policy_ = profile->warnings;
    } else {
        policy_.clear();
    }
}

void WarningCollector::emit(Warning warning, const std::unordered_map<std::string, std::string>& fields) {
    emit(warning_to_string(warning), fields);
}

void WarningCollector::emit(Warning warning) {
    emit(warning_to_string(warning), {});
}

void WarningCollector::emit_with_context(Warning warning, const std::string& context) {
    std::unordered_map<std::string, std::string> fields;
    if (!context.empty()) {
        fields["context"] = context;
    }
    emit(warning_to_string(warning), fields);
}

void WarningCollector::emit(const std::string& warning_key, 
                            std::unordered_map<std::string, std::string> fields) {
    WarningAction action = get_effective_action(warning_key);
    
    // Warnings with action "ignore" are still collected but marked
    warnings_.push_back({warning_key, std::move(fields), action});
}

void WarningCollector::apply_override(const std::string& warning_key, WarningAction action) {
    // Normalize key to lowercase
    std::string key = warning_key;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    overrides_[key] = action;
}

std::vector<WarningObject> WarningCollector::get_warnings() const {
    std::vector<WarningObject> result;
    
    for (const auto& w : warnings_) {
        // Skip warnings with "ignore" action (per SPEC)
        if (w.effective_action == WarningAction::Ignore) {
            continue;
        }
        
        WarningObject obj;
        obj.key = w.key;
        obj.action = action_to_string(w.effective_action);
        obj.fields = w.fields;
        result.push_back(std::move(obj));
    }
    
    return result;
}

bool WarningCollector::has_errors() const {
    for (const auto& w : warnings_) {
        if (w.effective_action == WarningAction::Error) {
            return true;
        }
    }
    return false;
}

bool WarningCollector::has_effective_warnings() const {
    for (const auto& w : warnings_) {
        if (w.effective_action != WarningAction::Ignore) {
            return true;
        }
    }
    return false;
}

void WarningCollector::clear() {
    warnings_.clear();
}

WarningAction WarningCollector::get_effective_action(const std::string& key) const {
    // Normalize key to lowercase
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    // Check overrides first (highest precedence)
    auto override_it = overrides_.find(lower_key);
    if (override_it != overrides_.end()) {
        return override_it->second;
    }
    
    // Check policy map
    auto policy_it = policy_.find(lower_key);
    if (policy_it != policy_.end()) {
        return policy_it->second;
    }
    
    // Default: warn
    return WarningAction::Warn;
}

} // namespace nah
