#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nah {

// ============================================================================
// Manifest Builder (per SPEC L1569-L1596)
// ============================================================================

class ManifestBuilder {
public:
    ManifestBuilder& id(const std::string& value);
    ManifestBuilder& version(const std::string& value);
    ManifestBuilder& nak_id(const std::string& value);
    ManifestBuilder& nak_version_req(const std::string& value);
    ManifestBuilder& nak_loader(const std::string& value);
    ManifestBuilder& entrypoint(const std::string& value);
    ManifestBuilder& entrypoint_arg(const std::string& value);
    ManifestBuilder& env(const std::string& key, const std::string& value);
    ManifestBuilder& lib_dir(const std::string& value);
    ManifestBuilder& asset_dir(const std::string& value);
    ManifestBuilder& asset_export(const std::string& id, const std::string& path, 
                                   const std::string& type = "");
    ManifestBuilder& filesystem_permission(const std::string& value);
    ManifestBuilder& network_permission(const std::string& value);
    ManifestBuilder& description(const std::string& value);
    ManifestBuilder& author(const std::string& value);
    ManifestBuilder& license(const std::string& value);
    ManifestBuilder& homepage(const std::string& value);
    
    // Build the manifest blob (header + TLV payload)
    std::vector<uint8_t> build() const;

private:
    std::string id_;
    std::string version_;
    std::string nak_id_;
    std::string nak_version_req_;
    std::string nak_loader_;
    std::string entrypoint_;
    std::vector<std::string> entrypoint_args_;
    std::vector<std::string> env_vars_;
    std::vector<std::string> lib_dirs_;
    std::vector<std::string> asset_dirs_;
    std::vector<std::string> asset_exports_;
    std::vector<std::string> filesystem_permissions_;
    std::vector<std::string> network_permissions_;
    std::string description_;
    std::string author_;
    std::string license_;
    std::string homepage_;
};

// Factory function for fluent building
inline ManifestBuilder manifest() {
    return ManifestBuilder();
}

} // namespace nah

// ============================================================================
// Platform-Specific Section Attributes (per SPEC L1601-L1627)
// ============================================================================

#if defined(__APPLE__)
    #define NAH_MANIFEST_SECTION \
        __attribute__((used)) \
        __attribute__((section("__NAH,__manifest"))) \
        __attribute__((aligned(16)))
#elif defined(__linux__) || defined(__unix__)
    #define NAH_MANIFEST_SECTION \
        __attribute__((used)) \
        __attribute__((section(".nah_manifest"))) \
        __attribute__((aligned(16)))
#elif defined(_WIN32)
    #pragma section(".nah", read)
    #define NAH_MANIFEST_SECTION \
        __declspec(allocate(".nah"))
#else
    #define NAH_MANIFEST_SECTION
#endif

// ============================================================================
// NAH_APP_MANIFEST Macro (per SPEC L1569-L1596)
// ============================================================================

// Helper to define manifest data at compile time
#define NAH_APP_MANIFEST(builder) \
    namespace { \
        static const auto _nah_manifest_data = (builder); \
        NAH_MANIFEST_SECTION \
        static const uint8_t _nah_manifest[sizeof(_nah_manifest_data)] = {}; \
    } \
    namespace nah { \
        namespace detail { \
            [[gnu::constructor]] \
            void _init_manifest() { \
                auto data = (builder); \
                std::memcpy(const_cast<uint8_t*>(_nah_manifest), data.data(), data.size()); \
            } \
        } \
    }

// Simpler version that embeds pre-built manifest bytes
#define NAH_EMBED_MANIFEST(bytes, size) \
    NAH_MANIFEST_SECTION \
    static const uint8_t _nah_manifest_data[size] = bytes
