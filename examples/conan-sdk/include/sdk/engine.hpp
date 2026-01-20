#pragma once

#include <string>
#include <memory>
#include <functional>

namespace gameengine {

// Forward declarations
class NetworkManager;
class AssetManager;
class CryptoProvider;

/**
 * Engine initialization options.
 */
struct EngineConfig {
    std::string app_id;
    std::string app_root;
    std::string engine_root;
    std::string log_level = "info";
    bool use_nah_env = true;  // Read from NAH_* environment variables
};

/**
 * Main engine context.
 * 
 * This is the primary interface apps use. It provides access to
 * networking, assets, crypto, and other engine subsystems.
 */
class Engine {
public:
    ~Engine();
    
    // Factory
    static std::unique_ptr<Engine> create(const EngineConfig& config);
    
    // Lifecycle
    bool initialize();
    void shutdown();
    
    // Subsystems
    NetworkManager& network();
    AssetManager& assets();
    CryptoProvider& crypto();
    
    // App info (from NAH or config)
    const std::string& app_id() const;
    const std::string& app_root() const;
    const std::string& engine_root() const;
    
    // Main loop support
    using UpdateCallback = std::function<bool(float delta_time)>;
    void run(UpdateCallback update);
    void request_quit();

private:
    Engine();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * Check if running under NAH.
 */
bool is_nah_managed();

/**
 * Get engine version.
 */
const char* version();

} // namespace gameengine
