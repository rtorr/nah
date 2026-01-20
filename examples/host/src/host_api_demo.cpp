/**
 * Host API Demo
 * ==============
 * Demonstrates the NAH host library for integration.
 */

#define NAH_HOST_IMPLEMENTATION
#include <nah/nah.h>
#include <iostream>

void print_separator() {
    std::cout << std::string(60, '-') << "\n";
}

int main(int argc, char* argv[]) {
    std::string nah_root;

    // Check for command line argument
    if (argc > 1) {
        nah_root = argv[1];
    }

    std::cout << "NAH Host API Demo\n";
    std::cout << "=================\n\n";

    // Create host instance (automatically resolves root)
    auto host = nah::host::NahHost::create(nah_root);
    std::cout << "NAH Root: " << host->root() << "\n\n";

    // Validate root
    std::string validation = host->validateRoot();
    if (!validation.empty()) {
        std::cerr << "Error: " << validation << "\n";
        std::cerr << "Please run setup_host.sh first.\n";
        return 1;
    }

    // List installed applications
    print_separator();
    std::cout << "Installed Applications:\n";
    print_separator();

    auto apps = host->listApplications();
    if (apps.empty()) {
        std::cout << "  (no applications installed)\n";
    } else {
        for (const auto& app : apps) {
            std::cout << "  " << app.id << "@" << app.version << "\n";
            std::cout << "    Instance: " << app.instance_id << "\n";
            std::cout << "    Root: " << app.install_root << "\n";
        }
    }
    std::cout << "\n";

    // Show host environment
    print_separator();
    std::cout << "Host Environment:\n";
    print_separator();

    auto host_env = host->getHostEnvironment();

    if (!host_env.vars.empty()) {
        std::cout << "  Environment variables:\n";
        for (const auto& [k, v] : host_env.vars) {
            std::cout << "    " << k << "=" << v.value << "\n";
        }
    } else {
        std::cout << "  (no environment variables configured)\n";
    }

    if (!host_env.paths.library_prepend.empty()) {
        std::cout << "  Library paths (prepend):\n";
        for (const auto& p : host_env.paths.library_prepend) {
            std::cout << "    " << p << "\n";
        }
    }
    std::cout << "\n";

    // Show runtime inventory
    print_separator();
    std::cout << "Runtime Inventory (NAKs):\n";
    print_separator();

    auto inventory = host->getInventory();
    if (inventory.runtimes.empty()) {
        std::cout << "  (no runtimes installed)\n";
    } else {
        for (const auto& [ref, runtime] : inventory.runtimes) {
            std::cout << "  " << runtime.nak.id << "@" << runtime.nak.version << "\n";
            std::cout << "    Root: " << runtime.paths.root << "\n";
            if (!runtime.loaders.empty()) {
                std::cout << "    Loaders: ";
                bool first = true;
                for (const auto& [name, _] : runtime.loaders) {
                    if (!first) std::cout << ", ";
                    std::cout << name;
                    first = false;
                }
                std::cout << "\n";
            }
        }
    }
    std::cout << "\n";

    // Get launch contract for first app
    if (!apps.empty()) {
        print_separator();
        std::cout << "Launch Contract for " << apps[0].id << ":\n";
        print_separator();

        auto result = host->getLaunchContract(apps[0].id, apps[0].version);

        if (result.ok) {
            const auto& c = result.contract;
            std::cout << "  App: " << c.app.id << " v" << c.app.version << "\n";

            if (!c.nak.id.empty()) {
                std::cout << "  NAK: " << c.nak.id << " v" << c.nak.version << "\n";
            }

            std::cout << "  Binary: " << c.execution.binary << "\n";
            std::cout << "  CWD: " << c.execution.cwd << "\n";

            if (!result.warnings.empty()) {
                std::cout << "  Warnings: " << result.warnings.size() << "\n";
                for (const auto& w : result.warnings) {
                    std::cout << "    [" << w.action << "] " << w.key << "\n";
                }
            }

            // Demonstrate direct execution (optional)
            std::cout << "\n";
            print_separator();
            std::cout << "Execute app? (y/n): ";
            std::string response;
            std::getline(std::cin, response);

            if (response == "y" || response == "Y") {
                std::cout << "\nExecuting " << apps[0].id << "...\n";
                print_separator();

                int exit_code = host->executeContract(result.contract);

                print_separator();
                std::cout << "Application exited with code: " << exit_code << "\n";
            }
        } else {
            if (result.critical_error) {
                std::cout << "  Error: " << nah::core::critical_error_to_string(*result.critical_error) << "\n";
            }
            std::cout << "  Details: " << result.critical_error_context << "\n";
        }
    }

    std::cout << "\n";
    print_separator();
    std::cout << "Demo complete.\n";

    // Show convenience functions
    std::cout << "\n";
    print_separator();
    std::cout << "Quick Examples:\n";
    print_separator();
    std::cout << "// List all apps\n";
    std::cout << "auto app_list = nah::host::listInstalledApps();\n\n";
    std::cout << "// Execute app directly\n";
    std::cout << "int code = nah::host::quickExecute(\"com.example.app\");\n\n";
    std::cout << "// Check if installed\n";
    std::cout << "bool installed = host->isApplicationInstalled(\"com.example.app\");\n";

    return 0;
}