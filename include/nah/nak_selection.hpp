#pragma once

#include "nah/types.hpp"
#include "nah/manifest.hpp"
#include "nah/nak_record.hpp"
#include "nah/host_profile.hpp"
#include "nah/warnings.hpp"

#include <string>
#include <vector>

namespace nah {

// ============================================================================
// NAK Registry Entry
// ============================================================================

struct NakRegistryEntry {
    std::string id;
    std::string version;
    std::string record_path;  // Full path to the record file
    std::string record_ref;   // Filename only (e.g., "com.example.nak@3.0.2.toml")
};

// ============================================================================
// Install-Time NAK Selection (per SPEC L1151-L1187)
// ============================================================================

struct NakSelectionResult {
    bool resolved = false;
    NakPin pin;
    std::string selection_reason;  // Audit-only
};

// Select a NAK for installation
// Returns the selected NAK pin or unresolved status
NakSelectionResult select_nak_for_install(
    const Manifest& manifest,
    const HostProfile& profile,
    const std::vector<NakRegistryEntry>& registry,
    WarningCollector& warnings);

// ============================================================================
// Compose-Time Pinned NAK Load (per SPEC L1189-L1236)
// ============================================================================

struct PinnedNakLoadResult {
    bool loaded = false;
    NakInstallRecord nak_record;
};

// Load a pinned NAK record for contract composition
// Validates schema, required fields, and compatibility with manifest
PinnedNakLoadResult load_pinned_nak(
    const NakPin& pin,
    const Manifest& manifest,
    const HostProfile& profile,
    const std::string& nah_root,
    WarningCollector& warnings);

// ============================================================================
// NAK Registry Scanning
// ============================================================================

// Scan the NAK registry directory and return all entries
std::vector<NakRegistryEntry> scan_nak_registry(const std::string& nah_root);

} // namespace nah
