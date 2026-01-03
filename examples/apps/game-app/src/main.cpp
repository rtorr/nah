/**
 * My Game - Example Game Application
 * ===================================
 * Demonstrates an app that targets the Game Engine SDK (conan-sdk NAK).
 * 
 * This app uses SDK features powered by Conan dependencies:
 * - HTTP client (libcurl)
 * - JSON parsing (nlohmann_json) 
 * - Logging (spdlog)
 * - Compression (zlib)
 * - Encryption (openssl)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Game Engine SDK headers
#include <sdk/engine.hpp>
#include <sdk/http.hpp>
#include <sdk/config.hpp>
#include <sdk/compression.hpp>
#include <sdk/crypto.hpp>

namespace {

void print_environment() {
    printf("My Game v1.0.0\n");
    printf("==============\n\n");
    
    const char* app_id = std::getenv("NAH_APP_ID");
    const char* app_root = std::getenv("NAH_APP_ROOT");
    const char* nak_id = std::getenv("NAH_NAK_ID");
    const char* nak_root = std::getenv("NAH_NAK_ROOT");
    
    if (app_id) {
        printf("Running in NAH-managed environment\n");
        printf("  NAH_APP_ID=%s\n", app_id);
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
    sdk::EngineConfig config;
    config.app_name = "My Game";
    config.log_level = sdk::LogLevel::Debug;
    
    sdk::Engine engine(config);
    if (!engine.initialize()) {
        fprintf(stderr, "Failed to initialize game engine\n");
        return 1;
    }
    
    printf("Game Engine initialized successfully!\n");
    printf("  Engine version: %s\n", engine.version().c_str());
    printf("\n");
    
    // Demonstrate SDK features
    
    // 1. Configuration (JSON parsing via nlohmann_json)
    printf("Loading game configuration...\n");
    sdk::ConfigManager cfg;
    if (cfg.load_from_file("assets/game_config.json")) {
        printf("  Config loaded: %s\n", cfg.get_string("game.title", "Unknown").c_str());
    } else {
        printf("  Using default configuration\n");
    }
    
    // 2. Compression (zlib)
    printf("\nTesting compression...\n");
    const char* test_data = "Hello from My Game! This is test data for compression.";
    auto compressed = sdk::compress(test_data, strlen(test_data));
    printf("  Original: %zu bytes\n", strlen(test_data));
    printf("  Compressed: %zu bytes\n", compressed.size());
    
    // 3. Crypto (openssl)
    printf("\nTesting crypto...\n");
    auto hash = sdk::sha256(test_data, strlen(test_data));
    printf("  SHA256: %s\n", sdk::to_hex(hash).c_str());
    
    // 4. HTTP client (libcurl) - just show capability
    printf("\nHTTP client available: %s\n", sdk::HttpClient::available() ? "yes" : "no");
    
    // 5. Logging (spdlog)
    printf("\nLogging test:\n");
    engine.log_info("Game started successfully");
    engine.log_debug("Debug message from game");
    
    printf("\nGame initialization complete!\n");
    printf("In a real game, the main loop would start here...\n\n");
    
    engine.shutdown();
    printf("Game Engine shut down cleanly.\n");
    
    return 0;
}
