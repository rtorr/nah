#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace gameengine {

/**
 * Asset manager for loading and caching game assets.
 * Uses zlib for compressed assets.
 */
class AssetManager {
public:
    AssetManager(const std::string& app_root, const std::string& engine_root);
    ~AssetManager();
    
    // Load raw bytes from app assets
    std::optional<std::vector<uint8_t>> load(const std::string& path);
    
    // Load text file
    std::optional<std::string> load_text(const std::string& path);
    
    // Load compressed asset (auto-decompress with zlib)
    std::optional<std::vector<uint8_t>> load_compressed(const std::string& path);
    
    // Check if asset exists
    bool exists(const std::string& path) const;
    
    // Get full path to asset
    std::string resolve_path(const std::string& path) const;
    
    // Engine resources (from NAK)
    std::optional<std::vector<uint8_t>> load_engine_resource(const std::string& path);
    
    // Cache control
    void preload(const std::vector<std::string>& paths);
    void clear_cache();
    size_t cache_size() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gameengine
