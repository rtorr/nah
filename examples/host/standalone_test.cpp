// Standalone test demonstrating direct usage of NAH header-only library
// This shows how hosts can integrate NAH without the CLI

#define NAH_HOST_IMPLEMENTATION
#include <nah/nah_host.h>
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    // Get NAH root from environment or use default
    const char* nah_root_env = std::getenv("NAH_ROOT");
    std::string nah_root = nah_root_env ? nah_root_env : "/nah";

    std::cout << "Standalone NAH Host Test\n";
    std::cout << "========================\n";
    std::cout << "NAH Root: " << nah_root << "\n\n";

    // Create host instance
    auto host = nah::host::NahHost::create(nah_root);
    if (!host) {
        std::cerr << "Failed to create NAH host\n";
        return 1;
    }

    // List installed applications
    std::cout << "Installed Applications:\n";
    auto apps = host->listApplications();

    if (apps.empty()) {
        std::cout << "  (no applications installed)\n";
    } else {
        for (const auto& app : apps) {
            std::cout << "  - " << app.id << " v" << app.version << "\n";
        }
    }

    // If an app ID was provided, try to execute it
    if (argc > 1) {
        std::string app_id = argv[1];
        std::cout << "\nExecuting " << app_id << "...\n";
        std::cout << "------------------------------------------------------------\n";

        int exit_code = host->executeApplication(app_id, {});

        std::cout << "------------------------------------------------------------\n";
        std::cout << "Application exited with code: " << exit_code << "\n";
        return exit_code;
    } else {
        std::cout << "\nUsage: " << argv[0] << " [app_id]\n";
        std::cout << "  Lists installed apps, or executes the specified app\n";
    }

    return 0;
}