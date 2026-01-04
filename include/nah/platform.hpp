#pragma once

#include <optional>
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace nah {

// ============================================================================
// Platform Detection
// ============================================================================

enum class Platform {
    Linux,
    macOS,
    Windows,
    Unknown
};

Platform get_current_platform();

// ============================================================================
// Binary Section Reading (per SPEC L1599-L1627)
// ============================================================================

// Read the NAH manifest section from a binary file
// Returns the raw bytes of the manifest section, or empty on failure
struct SectionReadResult {
    bool ok = false;
    std::string error;
    std::vector<uint8_t> data;
};

// Read the NAH manifest section from a binary file
// Platform-specific:
//   - macOS: __NAH,__manifest section in Mach-O
//   - Linux: .nah_manifest section in ELF
//   - Windows: .nah section in PE/COFF
SectionReadResult read_manifest_section(const std::string& binary_path);

// Read the NAH manifest section from binary data in memory
SectionReadResult read_manifest_section(const std::vector<uint8_t>& binary_data);

// ============================================================================
// Atomic File Operations (per SPEC L569-L577)
// ============================================================================

struct AtomicWriteResult {
    bool ok = false;
    std::string error;
};

// Write content atomically using temp file + fsync + rename + fsync(dir)
AtomicWriteResult atomic_write_file(const std::string& path, const std::string& content);
AtomicWriteResult atomic_write_file(const std::string& path, const std::vector<uint8_t>& content);

// Create a directory atomically (mkdir with fsync on parent)
AtomicWriteResult atomic_create_directory(const std::string& path);

// Update symlink atomically (remove + create with fsync on parent)
AtomicWriteResult atomic_update_symlink(const std::string& link_path, const std::string& target);

// ============================================================================
// Path Utilities
// ============================================================================

// Convert a path to use forward slashes (portable format)
// NAH uses forward slashes internally for cross-platform consistency
// in tar archives, manifests, and all stored paths.
std::string to_portable_path(const std::string& path);

// Get the directory containing a file path
std::string get_parent_directory(const std::string& path);

// Get the filename from a path
std::string get_filename(const std::string& path);

// Join path components
std::string join_path(const std::string& base, const std::string& rel);

// Check if a path exists
bool path_exists(const std::string& path);

// Check if a path is a directory
bool is_directory(const std::string& path);

// Check if a path is a regular file
bool is_regular_file(const std::string& path);

// Check if a path is a symlink
bool is_symlink(const std::string& path);

// Read symlink target
std::optional<std::string> read_symlink(const std::string& path);

// List directory entries
std::vector<std::string> list_directory(const std::string& path);

// Create parent directories recursively
bool create_directories(const std::string& path);

// Remove a directory recursively
bool remove_directory(const std::string& path);

// Remove a file
bool remove_file(const std::string& path);

// Copy a file
bool copy_file(const std::string& src, const std::string& dst);

// ============================================================================
// Environment
// ============================================================================

// Get an environment variable
std::optional<std::string> get_env(const std::string& name);

// Get all environment variables as a map
std::unordered_map<std::string, std::string> get_all_env();

// Get current timestamp as RFC3339 string
std::string get_current_timestamp();

// Generate a UUID string
std::string generate_uuid();

} // namespace nah
