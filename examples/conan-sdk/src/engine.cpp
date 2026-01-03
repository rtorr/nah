#include "sdk/engine.hpp"
#include "sdk/network.hpp"
#include "sdk/assets.hpp"
#include "sdk/crypto.hpp"

#include <spdlog/spdlog.h>
#include <cstdlib>
#include <chrono>

namespace gameengine {

struct Engine::Impl {
    EngineConfig config;
    std::unique_ptr<NetworkManager> network_mgr;
    std::unique_ptr<AssetManager> asset_mgr;
    std::unique_ptr<CryptoProvider> crypto_prov;
    bool running = false;
    bool quit_requested = false;
};

Engine::Engine() : impl_(std::make_unique<Impl>()) {}
Engine::~Engine() = default;

std::unique_ptr<Engine> Engine::create(const EngineConfig& config) {
    auto engine = std::unique_ptr<Engine>(new Engine());
    engine->impl_->config = config;
    
    // Read from NAH environment if enabled
    if (config.use_nah_env && is_nah_managed()) {
        if (const char* id = std::getenv("NAH_APP_ID")) {
            engine->impl_->config.app_id = id;
        }
        if (const char* root = std::getenv("NAH_APP_ROOT")) {
            engine->impl_->config.app_root = root;
        }
        if (const char* nak_root = std::getenv("NAH_NAK_ROOT")) {
            engine->impl_->config.engine_root = nak_root;
        }
        if (const char* level = std::getenv("GAMEENGINE_LOG_LEVEL")) {
            engine->impl_->config.log_level = level;
        }
    }
    
    return engine;
}

bool Engine::initialize() {
    // Set up logging
    if (impl_->config.log_level == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else if (impl_->config.log_level == "warn") {
        spdlog::set_level(spdlog::level::warn);
    } else if (impl_->config.log_level == "error") {
        spdlog::set_level(spdlog::level::err);
    } else {
        spdlog::set_level(spdlog::level::info);
    }
    
    spdlog::info("GameEngine {} initializing", version());
    spdlog::info("  App ID: {}", impl_->config.app_id);
    spdlog::info("  App Root: {}", impl_->config.app_root);
    spdlog::info("  Engine Root: {}", impl_->config.engine_root);
    
    // Initialize subsystems
    impl_->network_mgr = std::make_unique<NetworkManager>();
    impl_->asset_mgr = std::make_unique<AssetManager>(
        impl_->config.app_root, 
        impl_->config.engine_root
    );
    impl_->crypto_prov = std::make_unique<CryptoProvider>();
    
    spdlog::info("GameEngine initialized successfully");
    return true;
}

void Engine::shutdown() {
    spdlog::info("GameEngine shutting down");
    impl_->crypto_prov.reset();
    impl_->asset_mgr.reset();
    impl_->network_mgr.reset();
}

NetworkManager& Engine::network() { return *impl_->network_mgr; }
AssetManager& Engine::assets() { return *impl_->asset_mgr; }
CryptoProvider& Engine::crypto() { return *impl_->crypto_prov; }

const std::string& Engine::app_id() const { return impl_->config.app_id; }
const std::string& Engine::app_root() const { return impl_->config.app_root; }
const std::string& Engine::engine_root() const { return impl_->config.engine_root; }

void Engine::run(UpdateCallback update) {
    impl_->running = true;
    impl_->quit_requested = false;
    
    auto last_time = std::chrono::steady_clock::now();
    
    while (impl_->running && !impl_->quit_requested) {
        auto now = std::chrono::steady_clock::now();
        float delta = std::chrono::duration<float>(now - last_time).count();
        last_time = now;
        
        // Poll network
        impl_->network_mgr->poll();
        
        // Call update
        if (!update(delta)) {
            impl_->quit_requested = true;
        }
    }
    
    impl_->running = false;
}

void Engine::request_quit() {
    impl_->quit_requested = true;
}

bool is_nah_managed() {
    return std::getenv("NAH_APP_ID") != nullptr;
}

const char* version() {
    return "1.0.0";
}

} // namespace gameengine
