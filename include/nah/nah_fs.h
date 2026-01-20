/*
 * NAH FS - Filesystem Operations for NAH
 *
 * This file provides filesystem operations needed by NAH hosts.
 * Uses standard C++ filesystem library.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NAH_FS_H
#define NAH_FS_H

#ifdef __cplusplus

#include "nah_core.h"
#include "nah_json.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace nah {
namespace fs {

namespace stdfs = std::filesystem;

// ============================================================================
// FILE OPERATIONS
// ============================================================================

/**
 * Read entire file contents as string.
 */
inline std::optional<std::string> read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

/**
 * Write string to file.
 */
inline bool write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }
    file << content;
    return file.good();
}

/**
 * Check if path exists.
 */
inline bool exists(const std::string& path) {
    return stdfs::exists(path);
}

/**
 * Check if path is a regular file.
 */
inline bool is_file(const std::string& path) {
    return stdfs::is_regular_file(path);
}

/**
 * Check if path is a directory.
 */
inline bool is_directory(const std::string& path) {
    return stdfs::is_directory(path);
}

/**
 * Check if path is a symlink.
 */
inline bool is_symlink(const std::string& path) {
    return stdfs::is_symlink(path);
}

/**
 * Get file size in bytes.
 */
inline std::optional<std::uintmax_t> file_size(const std::string& path) {
    std::error_code ec;
    auto size = stdfs::file_size(path, ec);
    if (ec) {
        return std::nullopt;
    }
    return size;
}

/**
 * Get parent directory of a path.
 */
inline std::string parent_path(const std::string& path) {
    return core::normalize_separators(stdfs::path(path).parent_path().string());
}

/**
 * Get filename from a path.
 */
inline std::string filename(const std::string& path) {
    return stdfs::path(path).filename().string();
}

/**
 * Create directories recursively.
 */
inline bool create_directories(const std::string& path) {
    std::error_code ec;
    stdfs::create_directories(path, ec);
    return !ec;
}

/**
 * Remove a file.
 */
inline bool remove_file(const std::string& path) {
    std::error_code ec;
    stdfs::remove(path, ec);
    return !ec;
}

/**
 * Remove a directory and all its contents.
 */
inline bool remove_directory(const std::string& path) {
    std::error_code ec;
    stdfs::remove_all(path, ec);
    return !ec;
}

/**
 * Copy a file.
 */
inline bool copy_file(const std::string& src, const std::string& dst) {
    std::error_code ec;
    stdfs::copy_file(src, dst, stdfs::copy_options::overwrite_existing, ec);
    return !ec;
}

/**
 * List directory entries.
 */
inline std::vector<std::string> list_directory(const std::string& path) {
    std::vector<std::string> entries;
    std::error_code ec;
    for (const auto& entry : stdfs::directory_iterator(path, ec)) {
        entries.push_back(core::normalize_separators(entry.path().string()));
    }
    return entries;
}

/**
 * Get current working directory.
 */
inline std::string current_path() {
    return core::normalize_separators(stdfs::current_path().string());
}

/**
 * Set current working directory.
 */
inline bool set_current_path(const std::string& path) {
    std::error_code ec;
    stdfs::current_path(path, ec);
    return !ec;
}

/**
 * Get absolute path.
 */
inline std::string absolute_path(const std::string& path) {
    return core::normalize_separators(stdfs::absolute(path).string());
}

/**
 * Get canonical (resolved) path.
 */
inline std::optional<std::string> canonical_path(const std::string& path) {
    std::error_code ec;
    auto result = stdfs::canonical(path, ec);
    if (ec) {
        return std::nullopt;
    }
    return core::normalize_separators(result.string());
}

// ============================================================================
// RUNTIME INVENTORY LOADING
// ============================================================================

/**
 * Load a RuntimeInventory from a directory of NAK install records.
 * 
 * Expects directory structure:
 *   nak_root/
 *     <nak_id>@<version>.json  (e.g., lua@5.4.6.json)
 * 
 * Each JSON file should be a valid RuntimeDescriptor.
 */
inline core::RuntimeInventory load_inventory_from_directory(
    const std::string& nak_records_dir,
    std::vector<std::string>* errors = nullptr)
{
    core::RuntimeInventory inventory;
    
    if (!is_directory(nak_records_dir)) {
        if (errors) {
            errors->push_back("NAK records directory does not exist: " + nak_records_dir);
        }
        return inventory;
    }
    
    for (const auto& entry : list_directory(nak_records_dir)) {
        // Only process .json files
        if (entry.size() < 5 || entry.substr(entry.size() - 5) != ".json") {
            continue;
        }
        
        auto content = read_file(entry);
        if (!content) {
            if (errors) {
                errors->push_back("Failed to read: " + entry);
            }
            continue;
        }
        
        // Extract record_ref from filename
        std::string record_ref = filename(entry);

        // Parse the RuntimeDescriptor JSON
        auto result = json::parse_runtime_descriptor(*content, entry);
        if (result.ok) {
            result.value.source_path = entry;  // Track source for debugging
            inventory.runtimes[record_ref] = result.value;
        } else if (errors) {
            errors->push_back("Failed to parse " + entry + ": " + result.error);
        }
    }
    
    return inventory;
}

} // namespace fs
} // namespace nah

#endif // __cplusplus

#endif // NAH_FS_H
