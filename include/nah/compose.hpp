#pragma once

#include "nah/nak_record.hpp"
#include "nah/packaging.hpp"
#include "nah/types.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace nah {

// ============================================================================
// NAK Composition (per NAK-COMPOSITION.md specification)
// ============================================================================

// Conflict resolution strategy for file merging
enum class ConflictStrategy {
    Error,  // Fail on conflict (default)
    First,  // Use file from first NAK in argument order
    Last    // Use file from last NAK in argument order
};

// Source type for composition inputs
enum class ComposeSourceType {
    Installed,   // Installed NAK (resolved via registry)
    NakFile,     // .nak archive file
    Directory    // Unpacked NAK directory
};

// Input NAK reference for composition
struct ComposeInput {
    std::string id;
    std::string version;
    std::string source;  // Original reference (path or id@version)
    ComposeSourceType source_type = ComposeSourceType::Installed;
    
    // Resolved during composition
    NakPackInfo pack_info;
    std::string root_path;  // Absolute path to NAK content
    std::string content_hash;  // SHA-256 of content for integrity verification
};

// Options for NAK composition
struct ComposeOptions {
    std::string nah_root;
    
    // Required output fields
    std::string output_id;
    std::string output_version;
    std::string output_path;  // .nak file or directory
    
    // Conflict resolution
    ConflictStrategy on_conflict = ConflictStrategy::Error;
    
    // Loader selection (required if multiple NAKs have loaders)
    std::optional<std::string> loader_from;  // NAK ID to use loaders from
    
    // Environment overrides
    std::vector<std::pair<std::string, std::string>> add_env;  // KEY=VALUE pairs
    
    // Additional lib_dirs
    std::vector<std::string> add_lib_dirs;
    
    // Resource root override
    std::optional<std::string> resource_root;
    
    // Output options
    bool dry_run = false;
    bool verbose = false;
    bool include_provenance = true;
    
    // Manifest output
    std::optional<std::string> emit_manifest;  // Path to write composition manifest
};

// File conflict information
struct FileConflict {
    std::string relative_path;
    std::string source_a;
    std::string source_b;
    std::string hash_a;
    std::string hash_b;
};

// Composition result
struct ComposeResult {
    bool ok = false;
    std::string error;
    
    // On success
    std::string output_path;
    std::string nak_id;
    std::string nak_version;
    
    // Dry-run information
    std::vector<std::string> files_to_copy;
    std::vector<FileConflict> conflicts;
    std::vector<std::string> lib_dirs;
    EnvMap merged_environment;
    std::optional<std::string> selected_loader_from;
    
    // Provenance tracking
    std::vector<ComposeInput> sources;
};

// Composition manifest for reproducible builds
struct ComposeManifest {
    std::string output_id;
    std::string output_version;
    
    struct SourceEntry {
        std::string id;
        std::string version;
        std::string source_type;  // "installed", "file", "directory"
        std::string source;       // Original path/reference
        std::string sha256;       // Content hash for verification
    };
    std::vector<SourceEntry> inputs;
    
    struct {
        ConflictStrategy on_conflict = ConflictStrategy::Error;
        std::optional<std::string> loader_from;
    } options;
    
    struct {
        std::vector<std::pair<std::string, std::string>> environment;
        std::vector<std::string> lib_dirs_append;
    } overrides;
};

// ============================================================================
// Composition Functions
// ============================================================================

// Resolve a NAK reference to a ComposeInput
// Accepts:
//   - Installed NAK: "nak_id@version" or "nak_id" (finds installed version)
//   - File path: "./path/to/nak.nak" or directory
ComposeInput resolve_compose_input(const std::string& ref, 
                                   const std::string& nah_root,
                                   std::string& error);

// Compose multiple NAKs into one
// The core composition algorithm:
//   1. Load and validate each input NAK
//   2. Merge file trees with conflict detection
//   3. Concatenate lib_dirs in input order
//   4. Apply environment algebra in input order
//   5. Select loader (error if multiple without --loader-from)
//   6. Generate output META/nak.json
//   7. Package as .nak
ComposeResult compose_naks(const std::vector<std::string>& input_refs,
                           const ComposeOptions& options);

// Parse a composition manifest from JSON
struct ComposeManifestParseResult {
    bool ok = false;
    std::string error;
    ComposeManifest manifest;
};
ComposeManifestParseResult parse_compose_manifest(const std::string& json_str);

// Compose from a manifest file
ComposeResult compose_from_manifest(const std::string& manifest_path,
                                    const std::string& output_path,
                                    const std::string& nah_root,
                                    bool verbose = false);

// ============================================================================
// Helper Functions
// ============================================================================

// Compute SHA-256 of a file
std::string compute_file_sha256(const std::string& path);

// Convert conflict strategy to string
std::string conflict_strategy_to_string(ConflictStrategy strategy);

// Parse conflict strategy from string
ConflictStrategy parse_conflict_strategy(const std::string& str);

// Convert source type to string
std::string source_type_to_string(ComposeSourceType type);

// Parse source type from string
ComposeSourceType parse_source_type(const std::string& str);

} // namespace nah
