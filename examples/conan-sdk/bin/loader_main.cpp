/**
 * GameEngine Loader
 * 
 * This is the NAK loader binary that NAH invokes to launch applications.
 * It bootstraps the engine, sets up the environment, and executes the app.
 */

#include "sdk/engine.hpp"
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <iostream>

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " [options]\n"
              << "\n"
              << "Options:\n"
              << "  --app-entry <path>    Path to application binary\n"
              << "  --app-root <path>     Application root directory\n"
              << "  --app-id <id>         Application identifier\n"
              << "  --engine-root <path>  Engine root directory\n"
              << "  --help                Show this help\n"
              << "\n"
              << "Environment variables (used if options not provided):\n"
              << "  NAH_APP_ENTRY   Application binary path\n"
              << "  NAH_APP_ROOT    Application root\n"
              << "  NAH_APP_ID      Application ID\n"
              << "  NAH_NAK_ROOT    Engine root\n";
}

int main(int argc, char* argv[]) {
    std::string app_entry;
    std::string app_root;
    std::string app_id;
    std::string engine_root;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--app-entry") == 0 && i + 1 < argc) {
            app_entry = argv[++i];
        } else if (strcmp(argv[i], "--app-root") == 0 && i + 1 < argc) {
            app_root = argv[++i];
        } else if (strcmp(argv[i], "--app-id") == 0 && i + 1 < argc) {
            app_id = argv[++i];
        } else if (strcmp(argv[i], "--engine-root") == 0 && i + 1 < argc) {
            engine_root = argv[++i];
        }
    }
    
    // Fall back to environment variables
    if (app_entry.empty()) {
        if (const char* env = std::getenv("NAH_APP_ENTRY")) {
            app_entry = env;
        }
    }
    if (app_root.empty()) {
        if (const char* env = std::getenv("NAH_APP_ROOT")) {
            app_root = env;
        }
    }
    if (app_id.empty()) {
        if (const char* env = std::getenv("NAH_APP_ID")) {
            app_id = env;
        }
    }
    if (engine_root.empty()) {
        if (const char* env = std::getenv("NAH_NAK_ROOT")) {
            engine_root = env;
        }
    }
    
    // Validate required fields
    if (app_entry.empty()) {
        std::cerr << "Error: --app-entry or NAH_APP_ENTRY required\n";
        return 1;
    }
    
    spdlog::info("GameEngine Loader starting");
    spdlog::info("  Engine version: {}", gameengine::version());
    spdlog::info("  App entry: {}", app_entry);
    spdlog::info("  App root: {}", app_root);
    spdlog::info("  App ID: {}", app_id);
    spdlog::info("  Engine root: {}", engine_root);
    
    // Set up additional environment for the app
    setenv("GAMEENGINE_LOADER", "1", 1);
    
    // Execute the application
    spdlog::info("Executing application: {}", app_entry);
    
    // Build argv for execv
    // The app gets its original argv starting at the binary name
    char* app_argv[] = {
        const_cast<char*>(app_entry.c_str()),
        nullptr
    };
    
    execv(app_entry.c_str(), app_argv);
    
    // If we get here, exec failed
    spdlog::error("Failed to execute {}: {}", app_entry, strerror(errno));
    return 1;
}
