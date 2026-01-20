#include "sdk/assets.hpp"

#include <zlib.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <unordered_map>
#include <filesystem>

namespace fs = std::filesystem;

namespace gameengine {

struct AssetManager::Impl {
    std::string app_root;
    std::string engine_root;
    std::unordered_map<std::string, std::vector<uint8_t>> cache;
};

AssetManager::AssetManager(const std::string& app_root, const std::string& engine_root)
    : impl_(std::make_unique<Impl>()) {
    impl_->app_root = app_root;
    impl_->engine_root = engine_root;
    spdlog::debug("AssetManager initialized");
    spdlog::debug("  App root: {}", app_root);
    spdlog::debug("  Engine root: {}", engine_root);
}

AssetManager::~AssetManager() = default;

std::string AssetManager::resolve_path(const std::string& path) const {
    // First try app assets, then engine resources
    fs::path app_path = fs::path(impl_->app_root) / "share" / path;
    if (fs::exists(app_path)) {
        return app_path.string();
    }
    
    fs::path engine_path = fs::path(impl_->engine_root) / "resources" / path;
    return engine_path.string();
}

bool AssetManager::exists(const std::string& path) const {
    return fs::exists(resolve_path(path));
}

std::optional<std::vector<uint8_t>> AssetManager::load(const std::string& path) {
    // Check cache
    auto it = impl_->cache.find(path);
    if (it != impl_->cache.end()) {
        return it->second;
    }
    
    std::string full_path = resolve_path(path);
    std::ifstream file(full_path, std::ios::binary | std::ios::ate);
    if (!file) {
        spdlog::warn("Asset not found: {}", path);
        return std::nullopt;
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        spdlog::error("Failed to read asset: {}", path);
        return std::nullopt;
    }
    
    impl_->cache[path] = data;
    return data;
}

std::optional<std::string> AssetManager::load_text(const std::string& path) {
    auto data = load(path);
    if (!data) return std::nullopt;
    return std::string(data->begin(), data->end());
}

std::optional<std::vector<uint8_t>> AssetManager::load_compressed(const std::string& path) {
    auto compressed = load(path);
    if (!compressed) return std::nullopt;
    
    // Decompress with zlib
    z_stream strm = {};
    if (inflateInit(&strm) != Z_OK) {
        spdlog::error("Failed to init zlib for: {}", path);
        return std::nullopt;
    }
    
    strm.avail_in = compressed->size();
    strm.next_in = compressed->data();
    
    std::vector<uint8_t> decompressed;
    uint8_t buffer[4096];
    
    int ret;
    do {
        strm.avail_out = sizeof(buffer);
        strm.next_out = buffer;
        ret = inflate(&strm, Z_NO_FLUSH);
        
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&strm);
            spdlog::error("Decompression failed for: {}", path);
            return std::nullopt;
        }
        
        size_t have = sizeof(buffer) - strm.avail_out;
        decompressed.insert(decompressed.end(), buffer, buffer + have);
    } while (ret != Z_STREAM_END);
    
    inflateEnd(&strm);
    return decompressed;
}

std::optional<std::vector<uint8_t>> AssetManager::load_engine_resource(const std::string& path) {
    fs::path full_path = fs::path(impl_->engine_root) / "resources" / path;
    std::ifstream file(full_path, std::ios::binary | std::ios::ate);
    if (!file) {
        return std::nullopt;
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        return std::nullopt;
    }
    
    return data;
}

void AssetManager::preload(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        load(path);
    }
}

void AssetManager::clear_cache() {
    impl_->cache.clear();
}

size_t AssetManager::cache_size() const {
    size_t total = 0;
    for (const auto& [_, data] : impl_->cache) {
        total += data.size();
    }
    return total;
}

} // namespace gameengine
