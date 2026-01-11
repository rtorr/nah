#pragma once

#include "nah/nak_record.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace nah {

// ============================================================================
// Deterministic Packaging (per SPEC L1826-L1845)
// ============================================================================

// Entry type in a tar archive
enum class TarEntryType {
    RegularFile,
    Directory,
    Symlink,    // NOT permitted - detection only
    Hardlink,   // NOT permitted - detection only
    Other       // NOT permitted - detection only
};

// A tar entry for deterministic packing
struct TarEntry {
    std::string path;           // Relative path within archive
    TarEntryType type;
    std::vector<uint8_t> data;  // File content (empty for directories)
    bool executable = false;    // True if file should be 0755
};

// Result of a pack operation
struct PackResult {
    bool ok = false;
    std::string error;
    std::vector<uint8_t> archive_data;  // The complete .tar.gz archive
};

// Result of an unpack operation
struct UnpackResult {
    bool ok = false;
    std::string error;
    std::vector<std::string> entries;  // Paths of extracted entries
};

// ============================================================================
// Deterministic Tar+Gzip Archive Creation
// ============================================================================

// Create a deterministic gzip-compressed tar archive from entries
// Per SPEC L1826-L1845:
//   - Entry ordering: lexicographic by full path, directories before files
//   - Metadata: uid=0, gid=0, uname="", gname="", mtime=0
//   - Permissions: dirs=0755, files=0644 (or 0755 if executable)
//   - Gzip: mtime=0, no filename, OS=255
//   - Symlinks/hardlinks NOT permitted (error if present)
PackResult create_deterministic_archive(const std::vector<TarEntry>& entries);

// Collect entries from a directory for packing
// Returns entries sorted in deterministic order
// Fails if symlinks or hardlinks are encountered
struct CollectResult {
    bool ok = false;
    std::string error;
    std::vector<TarEntry> entries;
};

CollectResult collect_directory_entries(const std::string& dir_path);

// Convenience: pack a directory to an archive
PackResult pack_directory(const std::string& dir_path);

// ============================================================================
// Safe Archive Extraction
// ============================================================================

// Extraction safety checks per SPEC L1836-L1845:
//   - Reject absolute paths
//   - Reject paths with .. or escaping extraction root
//   - Reject symlinks, hardlinks, device files, FIFOs, sockets
//   - Materialize only regular files and directories

// Validate a path for extraction safety
struct PathValidation {
    bool safe = false;
    std::string error;
    std::string normalized_path;  // Normalized relative path
};

PathValidation validate_extraction_path(const std::string& entry_path, 
                                         const std::string& extraction_root);

// Extract a gzip tar archive to a staging directory
// Uses safety validation on all entries
// If any entry fails validation, extraction fails and staging is cleaned up
UnpackResult extract_archive_safe(const std::vector<uint8_t>& archive_data,
                                   const std::string& staging_dir);

// Extract from a file path
UnpackResult extract_archive_safe(const std::string& archive_path,
                                   const std::string& staging_dir);

// ============================================================================
// NAP Package Operations (per SPEC L2637-L2680)
// ============================================================================

struct NapPackageInfo {
    bool ok = false;
    std::string error;
    
    // Extracted manifest info
    std::string app_id;
    std::string app_version;
    std::string nak_id;
    std::string nak_version_req;
    std::string entrypoint;
    
    // Package structure
    bool has_embedded_manifest = false;
    bool has_manifest_file = false;
    std::string manifest_source;  // "embedded:<binary>" or "file:manifest.nah"
    std::vector<std::string> binaries;
    std::vector<std::string> libraries;
    std::vector<std::string> assets;
};

// Validate and inspect a NAP package without extracting
NapPackageInfo inspect_nap_package(const std::string& package_path);
NapPackageInfo inspect_nap_package(const std::vector<uint8_t>& archive_data);

// Pack a directory as a NAP package
// Validates structure and manifest presence
PackResult pack_nap(const std::string& dir_path);

// ============================================================================
// NAK Pack Operations (per SPEC L2685-L2760)
// ============================================================================

struct NakPackInfo {
    bool ok = false;
    std::string error;
    
    // From META/nak.json
    std::string nak_id;
    std::string nak_version;
    std::string resource_root;
    std::vector<std::string> lib_dirs;
    EnvMap environment;
    std::unordered_map<std::string, LoaderConfig> loaders;
    std::string execution_cwd;
    
    // Helper methods
    bool has_loaders() const { return !loaders.empty(); }
    
    // Package structure
    std::vector<std::string> resources;
    std::vector<std::string> libraries;
    std::vector<std::string> binaries;
};

// Validate and inspect a NAK pack without extracting
NakPackInfo inspect_nak_pack(const std::string& pack_path);
NakPackInfo inspect_nak_pack(const std::vector<uint8_t>& archive_data);

// Pack a directory as a NAK pack
// Validates structure and META/nak.json presence
PackResult pack_nak(const std::string& dir_path);

// ============================================================================
// Installation Operations
// ============================================================================

struct AppInstallOptions {
    std::string nah_root = "/nah";
    std::string profile_name;           // Optional: profile for NAK selection
    bool force = false;                 // Overwrite existing installation
    bool skip_verification = false;     // Skip signature verification
    
    // Provenance (for remote materialization)
    std::string source;                 // Original source (URL or path), recorded in install record
    std::string installed_by;           // Who installed this (e.g., "ci-pipeline")
    std::string expected_hash;          // Expected SHA-256 hash (required for HTTPS)
};

struct AppInstallResult {
    bool ok = false;
    std::string error;
    std::string install_root;           // e.g., /nah/apps/com.example.app-1.0.0
    std::string record_path;            // e.g., /nah/registry/apps/com.example.app@1.0.0.json
    std::string instance_id;
    std::string nak_id;
    std::string nak_version;
    std::string app_id;
    std::string app_version;
    std::string package_hash;           // SHA-256 of the package
};

// Install a NAP package from a local file path
// Per SPEC:
//   1. Extract to staging directory
//   2. Validate manifest and structure
//   3. Select NAK version at install time
//   4. Atomically rename to final location
//   5. Write App Install Record atomically
AppInstallResult install_nap_package(const std::string& package_path,
                                      const AppInstallOptions& options);

// Install a NAP package from any source
// Accepts:
//   - Local file path (e.g., "./app.nap", "/path/to/app.nap")
//   - file: URL (e.g., "file:./app.nap")
//   - https: URL with SHA-256 (e.g., "https://example.com/app.nap#sha256=abc...")
// For HTTPS sources, SHA-256 verification is mandatory.
// Provenance is automatically recorded in the App Install Record.
AppInstallResult install_app(const std::string& source,
                              const AppInstallOptions& options);

struct NakInstallOptions {
    std::string nah_root = "/nah";
    bool force = false;                 // Overwrite existing installation
    
    // Provenance (for remote materialization)
    std::string source;                 // Original source (URL or path), recorded in install record
    std::string installed_by;           // Who installed this (e.g., "ci-pipeline")
    std::string expected_hash;          // Expected SHA-256 hash (required for HTTPS)
};

struct NakInstallResult {
    bool ok = false;
    std::string error;
    std::string install_root;           // e.g., /nah/naks/com.example.nak/1.0.0
    std::string record_path;            // e.g., /nah/registry/naks/com.example.nak@1.0.0.json
    std::string nak_id;
    std::string nak_version;
    std::string package_hash;           // SHA-256 of the package
};

// Install a NAK pack from a local file path
// Per SPEC:
//   1. Extract to staging directory
//   2. Validate META/nak.json schema and required fields
//   3. Atomically rename to final location
//   4. Write NAK Install Record atomically with resolved absolute paths
NakInstallResult install_nak_pack(const std::string& pack_path,
                                   const NakInstallOptions& options);

// Install a NAK pack from any source
// Accepts:
//   - Local file path (e.g., "./sdk.nak", "/path/to/sdk.nak")
//   - file: URL (e.g., "file:./sdk.nak")
//   - https: URL with SHA-256 (e.g., "https://example.com/sdk.nak#sha256=abc...")
// For HTTPS sources, SHA-256 verification is mandatory.
// Provenance is automatically recorded in the NAK Install Record.
NakInstallResult install_nak(const std::string& source,
                              const NakInstallOptions& options);

// ============================================================================
// Uninstallation Operations
// ============================================================================

struct UninstallResult {
    bool ok = false;
    std::string error;
};

// Uninstall an application
// Removes app directory and install record atomically
UninstallResult uninstall_app(const std::string& nah_root,
                               const std::string& app_id,
                               const std::string& version = "");

// Uninstall a NAK
// Fails if any installed apps reference this NAK version
UninstallResult uninstall_nak(const std::string& nah_root,
                               const std::string& nak_id,
                               const std::string& version);

// ============================================================================
// Verification Operations
// ============================================================================

struct VerifyResult {
    bool ok = false;
    std::string error;
    std::vector<std::string> issues;
    bool manifest_valid = false;
    bool structure_valid = false;
    bool nak_available = false;
};

// Verify an installed application
VerifyResult verify_app(const std::string& nah_root,
                         const std::string& app_id,
                         const std::string& version = "");

} // namespace nah
