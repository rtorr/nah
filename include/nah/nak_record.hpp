#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace nah {

// ============================================================================
// NAK Install Record (per SPEC L381-L468)
// ============================================================================

struct NakInstallRecord {
    std::string schema;  // MUST be "nah.nak.install.v1"
    
    // [nak] section
    struct {
        std::string id;
        std::string version;
    } nak;
    
    // [paths] section
    struct {
        std::string root;           // Absolute NAK root path
        std::string resource_root;  // Absolute, defaults to root if omitted
        std::vector<std::string> lib_dirs;  // Absolute paths
    } paths;
    
    // [environment] section
    std::unordered_map<std::string, std::string> environment;
    
    // [loader] section (OPTIONAL per SPEC L395)
    struct {
        bool present = false;
        std::string exec_path;  // Absolute path to loader executable
        std::vector<std::string> args_template;  // Template with {NAME} placeholders
    } loader;
    
    // [execution] section (OPTIONAL per SPEC L402)
    struct {
        bool present = false;
        std::string cwd;  // Template, resolved at composition time
    } execution;
    
    // [provenance] section (optional)
    struct {
        std::string package_hash;
        std::string installed_at;
        std::string installed_by;
        std::string source;
    } provenance;
    
    // Source path for trace
    std::string source_path;
};

// ============================================================================
// NAK Install Record Parsing Result
// ============================================================================

struct NakInstallRecordParseResult {
    bool ok = false;
    std::string error;
    NakInstallRecord record;
    std::vector<std::string> warnings;
};

// Parse a NAK Install Record from JSON string
NakInstallRecordParseResult parse_nak_install_record_full(const std::string& json_str,
                                                           const std::string& source_path = "");

// ============================================================================
// NAK Install Record Validation
// ============================================================================

// Validate required fields per SPEC Presence semantics
// Returns true if all required fields are present and valid
bool validate_nak_install_record(const NakInstallRecord& record, std::string& error);

// ============================================================================
// NAK Pack Manifest (per SPEC L2677-L2772)
// ============================================================================

struct NakPackManifest {
    std::string schema;  // MUST be "nah.nak.pack.v1"
    
    struct {
        std::string id;
        std::string version;
    } nak;
    
    struct {
        std::string resource_root;  // Relative to pack root
        std::vector<std::string> lib_dirs;  // Relative paths
    } paths;
    
    std::unordered_map<std::string, std::string> environment;
    
    struct {
        bool present = false;
        std::string exec_path;  // Relative path
        std::vector<std::string> args_template;
    } loader;
    
    struct {
        bool present = false;
        std::string cwd;
    } execution;
};

struct NakPackManifestParseResult {
    bool ok = false;
    std::string error;
    NakPackManifest manifest;
    std::vector<std::string> warnings;
};

// Parse a NAK pack manifest from JSON string
NakPackManifestParseResult parse_nak_pack_manifest(const std::string& json_str);

// ============================================================================
// Legacy API for backward compatibility
// ============================================================================

struct NakInstallValidation {
    bool ok;
    std::string error;
};

NakInstallValidation parse_nak_install_record(const std::string& json, NakInstallRecord& out);

} // namespace nah
