#include "nah/capabilities.hpp"

namespace nah {

// ============================================================================
// Permission Parsing
// ============================================================================

std::optional<ParsedPermission> parse_permission_string(const std::string& permission) {
    // Format: "type:operation:resource"
    auto first_colon = permission.find(':');
    if (first_colon == std::string::npos) {
        return std::nullopt;
    }
    
    auto second_colon = permission.find(':', first_colon + 1);
    if (second_colon == std::string::npos) {
        return std::nullopt;
    }
    
    std::string type = permission.substr(0, first_colon);
    std::string operation = permission.substr(first_colon + 1, second_colon - first_colon - 1);
    std::string resource = permission.substr(second_colon + 1);
    
    if (type.empty() || operation.empty() || resource.empty()) {
        return std::nullopt;
    }
    
    return ParsedPermission{type, operation, resource};
}

// ============================================================================
// Capability Derivation
// ============================================================================

std::optional<Capability> derive_capability(const std::string& operation, 
                                             const std::string& resource) {
    // Map operation to capability key (per SPEC L1118-L1134)
    std::string prefix;
    
    // Filesystem operations
    if (operation == "read" || operation == "write" || operation == "execute") {
        prefix = "fs";
    }
    // Network operations
    else if (operation == "connect" || operation == "listen" || operation == "bind") {
        prefix = "net";
    }
    // Unknown operation
    else {
        return std::nullopt;
    }
    
    Capability cap;
    cap.operation = operation;
    cap.resource = resource;
    cap.key = prefix + "." + operation + "." + resource;
    
    return cap;
}

std::vector<Capability> derive_capabilities_from_permissions(
    const std::vector<std::string>& permissions) {
    
    std::vector<Capability> result;
    
    for (const auto& perm : permissions) {
        auto parsed = parse_permission_string(perm);
        if (parsed) {
            auto cap = derive_capability(parsed->operation, parsed->resource);
            if (cap) {
                result.push_back(*cap);
            }
        }
    }
    
    return result;
}

std::optional<std::string> derive_enforcement(const std::string& capability_key,
                                               const HostProfile& profile) {
    auto it = profile.capabilities.find(capability_key);
    if (it != profile.capabilities.end()) {
        return it->second;
    }
    return std::nullopt;
}

Capability derive_capability(const std::string& permission, WarningCollector& warnings) {
    // Find the first colon
    auto colon = permission.find(':');
    
    if (colon == std::string::npos) {
        // Malformed - no colon
        warnings.emit(Warning::capability_malformed,
                      nah::warnings::capability_malformed(permission));
        return {permission, "", "", ""};
    }
    
    std::string operation = permission.substr(0, colon);
    std::string selector = permission.substr(colon + 1);
    
    // Map operation to capability key (per SPEC L1118-L1134)
    std::string key;
    
    // Filesystem permissions
    if (operation == "read") {
        key = "filesystem.read";
    } else if (operation == "write") {
        key = "filesystem.write";
    } else if (operation == "execute") {
        key = "filesystem.execute";
    }
    // Network permissions
    else if (operation == "connect") {
        key = "network.connect";
    } else if (operation == "listen") {
        key = "network.listen";
    } else if (operation == "bind") {
        key = "network.bind";
    }
    // Unknown operation
    else {
        warnings.emit(Warning::capability_unknown,
                      nah::warnings::capability_unknown(operation));
        key = operation;
    }
    
    return {key, selector, operation, selector};
}

EnforcementResult derive_enforcement(
    const std::vector<std::string>& filesystem_permissions,
    const std::vector<std::string>& network_permissions,
    const HostProfile& profile,
    WarningCollector& warnings) {
    
    EnforcementResult result;
    
    // Check if any permissions exist
    result.capability_usage.present = 
        !filesystem_permissions.empty() || !network_permissions.empty();
    
    // Process filesystem permissions (in manifest order per SPEC)
    for (const auto& perm : filesystem_permissions) {
        auto cap = derive_capability(perm, warnings);
        
        // Build the full capability string for capability_usage
        std::string full_cap = cap.key;
        if (!cap.selector.empty()) {
            full_cap += ":" + cap.selector;
        }
        result.capability_usage.required_capabilities.push_back(full_cap);
        
        // Look up enforcement mapping in profile
        auto it = profile.capabilities.find(cap.key);
        if (it != profile.capabilities.end()) {
            // Add enforcement ID to filesystem list
            result.filesystem.push_back(it->second);
        } else {
            // Missing capability mapping
            warnings.emit(Warning::capability_missing,
                          nah::warnings::capability_missing(cap.key));
        }
    }
    
    // Process network permissions (in manifest order per SPEC)
    for (const auto& perm : network_permissions) {
        auto cap = derive_capability(perm, warnings);
        
        // Build the full capability string for capability_usage
        std::string full_cap = cap.key;
        if (!cap.selector.empty()) {
            full_cap += ":" + cap.selector;
        }
        result.capability_usage.required_capabilities.push_back(full_cap);
        
        // Look up enforcement mapping in profile
        auto it = profile.capabilities.find(cap.key);
        if (it != profile.capabilities.end()) {
            // Add enforcement ID to network list
            result.network.push_back(it->second);
        } else {
            // Missing capability mapping
            warnings.emit(Warning::capability_missing,
                          nah::warnings::capability_missing(cap.key));
        }
    }
    
    // optional_capabilities and critical_capabilities are empty in v1.0
    
    return result;
}

} // namespace nah
