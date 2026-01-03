#pragma once

#include "nah/types.hpp"
#include "nah/warnings.hpp"
#include "nah/host_profile.hpp"

#include <string>
#include <vector>
#include <optional>

namespace nah {

// ============================================================================
// Permission Parsing
// ============================================================================

// Parsed permission structure
struct ParsedPermission {
    std::string type;       // "fs" or "net"
    std::string operation;  // "read", "write", "execute", "connect", "listen", "bind"
    std::string resource;   // The resource path or URL
};

// Parse a permission string in the format "type:operation:resource"
std::optional<ParsedPermission> parse_permission_string(const std::string& permission);

// ============================================================================
// Capability Derivation (per SPEC L1096-L1141)
// ============================================================================

// Derive a capability from an operation and resource
// Returns nullopt for unknown operations
std::optional<Capability> derive_capability(const std::string& operation, 
                                             const std::string& resource);

// Derive a capability from a permission string (with warning collection)
Capability derive_capability(const std::string& permission, WarningCollector& warnings);

// Derive capabilities from a list of permissions
std::vector<Capability> derive_capabilities_from_permissions(
    const std::vector<std::string>& permissions);

// ============================================================================
// Enforcement Mapping (per SPEC Step 9 of Composition)
// ============================================================================

// Map a capability key to an enforcement ID using the profile
std::optional<std::string> derive_enforcement(const std::string& capability_key,
                                               const HostProfile& profile);

struct EnforcementResult {
    std::vector<std::string> filesystem;
    std::vector<std::string> network;
    CapabilityUsage capability_usage;
};

// Derive capabilities and enforcement from manifest permissions
// Collects all permissions, derives capability keys, and maps to enforcement IDs
EnforcementResult derive_enforcement(
    const std::vector<std::string>& filesystem_permissions,
    const std::vector<std::string>& network_permissions,
    const HostProfile& profile,
    WarningCollector& warnings);

} // namespace nah
