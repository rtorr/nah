/**
 * My Game - Example Game Application
 * ===================================
 * Demonstrates an app that targets the Game Engine SDK (conan-sdk NAK).
 * 
 * This app uses SDK features powered by Conan dependencies:
 * - HTTP client (libcurl)
 * - Asset loading with compression (zlib)
 * - Cryptography (openssl)
 * - Logging (spdlog)
 */

#include <cstdio>
#include <cstdlib>

// Game Engine SDK headers
#include <sdk/engine.hpp>
#include <sdk/network.hpp>
#include <sdk/assets.hpp>
#include <sdk/crypto.hpp>

namespace {

void print_environment() {
    printf("My Game v1.0.0\n");
    printf("==============\n\n");
    
    if (gameengine::is_nah_managed()) {
        printf("Running in NAH-managed environment\n");
        const char* app_id = std::getenv("NAH_APP_ID");
        const char* app_root = std::getenv("NAH_APP_ROOT");
        const char* nak_id = std::getenv("NAH_NAK_ID");
        const char* nak_root = std::getenv("NAH_NAK_ROOT");
        printf("  NAH_APP_ID=%s\n", app_id ? app_id : "(not set)");
        printf("  NAH_APP_ROOT=%s\n", app_root ? app_root : "(not set)");
        printf("  NAH_NAK_ID=%s\n", nak_id ? nak_id : "(not set)");
        printf("  NAH_NAK_ROOT=%s\n", nak_root ? nak_root : "(not set)");
    } else {
        printf("Running standalone (not NAH-managed)\n");
    }
    printf("\n");
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    print_environment();
    
    // Initialize the Game Engine SDK
    gameengine::EngineConfig config;
    config.app_id = "com.example.mygame";
    config.use_nah_env = true;  // Override with NAH_* if available
    
    auto engine = gameengine::Engine::create(config);
    if (!engine || !engine->initialize()) {
        fprintf(stderr, "Failed to initialize game engine\n");
        return 1;
    }
    
    printf("Game Engine initialized!\n");
    printf("  Engine version: %s\n", gameengine::version());
    printf("  App ID: %s\n", engine->app_id().c_str());
    printf("  App root: %s\n", engine->app_root().c_str());
    printf("\n");
    
    // Demonstrate SDK features
    
    // 1. Asset loading
    printf("Testing asset loading...\n");
    auto& assets = engine->assets();
    auto config_text = assets.load_text("game_config.json");
    if (config_text) {
        printf("  Loaded game_config.json (%zu bytes)\n", config_text->size());
    } else {
        printf("  game_config.json not found (using defaults)\n");
    }
    
    // 2. Cryptography
    printf("\nTesting crypto...\n");
    auto& crypto = engine->crypto();
    std::string test_data = "Hello from My Game!";
    auto hash = crypto.sha256(test_data);
    printf("  SHA256(\"%s\") = %s\n", test_data.c_str(), hash.c_str());
    
    auto random = crypto.random_hex(16);
    printf("  Random (16 bytes): %s\n", random.c_str());
    
    // 3. Network (show capability)
    printf("\nTesting network...\n");
    auto& network = engine->network();
    printf("  NetworkManager ready\n");
    // In a real app: auto response = network.get("https://api.example.com/status");
    
    printf("\nGame initialization complete!\n");
    printf("In a real game, the main loop would start here.\n\n");
    
    engine->shutdown();
    printf("Game Engine shut down cleanly.\n");
    
    return 0;
}
