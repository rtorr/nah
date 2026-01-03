#pragma once

#include "nah/types.hpp"
#include "nah/manifest.hpp"
#include "nah/install_record.hpp"
#include "nah/host_profile.hpp"

#include <optional>
#include <string>
#include <unordered_map>

namespace nah {

// ============================================================================
// Contract Composition Inputs (per SPEC L861-L876)
// ============================================================================

struct CompositionInputs {
    std::string nah_root;                    // NAH root path (default "/nah")
    Manifest manifest;                       // App Manifest
    AppInstallRecord install_record;         // App Install Record
    HostProfile profile;                     // Host Profile (active or explicit)
    std::unordered_map<std::string, std::string> process_env;  // Current process environment
    std::optional<std::string> overrides_file_path;  // Optional overrides file
    std::string now;                         // RFC3339 timestamp for trust staleness check
    bool enable_trace = false;               // Whether to generate trace output
};

// ============================================================================
// Contract Composition Result
// ============================================================================

struct CompositionResult {
    bool ok = false;
    std::optional<CriticalError> critical_error;
    ContractEnvelope envelope;
};

// ============================================================================
// Contract Composition (per SPEC L877-L1085)
// ============================================================================

// Compose a launch contract from the given inputs
// This is the core algorithm that produces the Launch Contract
CompositionResult compose_contract(const CompositionInputs& inputs);

// ============================================================================
// Override Parsing
// ============================================================================

struct OverridesFile {
    std::unordered_map<std::string, std::string> environment;
    std::unordered_map<std::string, std::string> warnings;
};

struct OverridesParseResult {
    bool ok = false;
    std::string error;
    OverridesFile overrides;
};

// Parse an overrides file (TOML or JSON)
OverridesParseResult parse_overrides_file(const std::string& content, const std::string& path);

// ============================================================================
// JSON Output
// ============================================================================

// Serialize the contract envelope to deterministic JSON
std::string serialize_contract_json(const ContractEnvelope& envelope, 
                                     bool include_trace = false,
                                     std::optional<CriticalError> critical_error = std::nullopt);

// ============================================================================
// Platform-specific helpers
// ============================================================================

// Get the platform-specific library path environment key
std::string get_library_path_env_key();

// Get the platform-specific path separator
char get_path_separator();

} // namespace nah
