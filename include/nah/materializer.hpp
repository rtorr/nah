#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nah {

// ============================================================================
// Build-Time Remote Materialization Utilities (per SPEC L2815-L2890)
// ============================================================================
//
// This module provides utility functions for remote artifact handling:
// - Parsing artifact references (file: and https:// URLs)
// - SHA-256 hash computation and verification
// - HTTPS fetching
//
// These utilities are used by install_nak() and install_app() in packaging.hpp
// to support remote sources. For installation, use those functions directly.
//
// IMPORTANT: Contract composition (nah contract show / compose_contract)
// MUST NOT perform any network operations. It operates only on local state.

// ============================================================================
// Remote Artifact Reference Parsing
// ============================================================================

enum class ReferenceType {
    File,       // file:<path>
    Https,      // https://...#sha256=<hex>
    Invalid
};

struct ParsedReference {
    ReferenceType type = ReferenceType::Invalid;
    std::string path_or_url;        // File path or HTTPS URL (without fragment)
    std::string sha256_digest;      // Required for HTTPS, empty for file:
    std::string error;              // Error message if invalid
};

// Parse a NAK pack reference string
// Accepts:
//   - file:<absolute_or_relative_path>
//   - https://...#sha256=<64_hex_chars>
ParsedReference parse_artifact_reference(const std::string& reference);

// ============================================================================
// SHA-256 Hashing
// ============================================================================

struct HashResult {
    bool ok = false;
    std::string error;
    std::string hex_digest;     // Lowercase hex string (64 chars)
};

// Compute SHA-256 hash of data
HashResult compute_sha256(const std::vector<uint8_t>& data);
HashResult compute_sha256(const std::string& file_path);

// Verify data matches expected SHA-256 digest
struct Sha256VerifyResult {
    bool ok = false;
    std::string error;
    std::string actual_digest;
    std::string expected_digest;
};

Sha256VerifyResult verify_sha256(const std::vector<uint8_t>& data, 
                                  const std::string& expected_hex);

// ============================================================================
// HTTP Fetching
// ============================================================================

struct FetchResult {
    bool ok = false;
    std::string error;
    std::vector<uint8_t> data;
    long http_status = 0;
    std::string content_type;
};

// Fetch data from an HTTPS URL
// Follows redirects, verifies TLS certificates
FetchResult fetch_https(const std::string& url);

} // namespace nah
