/**
 * Application C - C++ App Example
 * ================================
 * 
 * This example demonstrates a C++ application using the NAH framework v1.1.0.
 * The manifest is now a JSON file (nap.json) packaged with the application,
 * rather than embedded in the binary.
 * 
 * This approach provides better developer ergonomics and uses standard tooling.
 */

#include <iostream>
#include <cstdlib>

// =============================================================================
// Helper Functions
// =============================================================================

bool is_nah_managed() {
    return std::getenv("NAH_APP_ID") != nullptr;
}

std::string get_env(const char* name, const char* default_value = "") {
    const char* value = std::getenv(name);
    return value ? value : default_value;
}

// =============================================================================
// Application Code
// =============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    std::cout << "Application C v1.0.0\n";
    std::cout << "====================\n\n";
    
    std::cout << "Manifest Info:\n";
    std::cout << "  Format: JSON (nap.json in package)\n";
    std::cout << "  Location: Package root\n\n";
    
    // Check NAH environment
    if (is_nah_managed()) {
        std::cout << "Running in NAH-managed environment:\n";
        std::cout << "  NAH_APP_ID=" << get_env("NAH_APP_ID") << "\n";
        std::cout << "  NAH_APP_VERSION=" << get_env("NAH_APP_VERSION") << "\n";
        std::cout << "  NAH_APP_ROOT=" << get_env("NAH_APP_ROOT") << "\n";
        std::cout << "  NAH_NAK_ID=" << get_env("NAH_NAK_ID") << "\n";
        std::cout << "  NAH_NAK_ROOT=" << get_env("NAH_NAK_ROOT") << "\n";
        std::cout << "  APP_MODE=" << get_env("APP_MODE", "(not set)") << "\n";
    } else {
        std::cout << "Running standalone (not NAH-managed)\n";
        std::cout << "  Install via 'nah install' to run in NAH environment.\n";
    }
    std::cout << "\n";
    
    std::cout << "Application C completed successfully.\n";
    
    return 0;
}
