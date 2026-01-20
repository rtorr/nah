/**
 * Contract Inspector
 * ==================
 * Inspect and compare launch contracts for applications.
 */

#define NAH_HOST_IMPLEMENTATION
#include <nah/nah.h>
#include <iostream>
#include <iomanip>

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <app_id> [options]\n";
    std::cerr << "\n";
    std::cerr << "Options:\n";
    std::cerr << "  --root <path>    NAH root directory (default: $NAH_ROOT or /nah)\n";
    std::cerr << "  --version <ver>  Specific app version (default: latest)\n";
    std::cerr << "  --trace          Include composition trace\n";
    std::cerr << "  --execute        Execute the app after inspection\n";
    std::cerr << "\n";
    std::cerr << "Example:\n";
    std::cerr << "  " << prog << " com.example.app --trace\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Parse arguments
    std::string app_id = argv[1];
    std::string nah_root;
    std::string version;
    bool enable_trace = false;
    bool execute = false;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            nah_root = argv[++i];
        } else if (arg == "--version" && i + 1 < argc) {
            version = argv[++i];
        } else if (arg == "--trace") {
            enable_trace = true;
        } else if (arg == "--execute") {
            execute = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    std::cout << "Contract Inspector\n";
    std::cout << "==================\n\n";

    // Create host
    auto host = nah::host::NahHost::create(nah_root);
    std::cout << "NAH Root: " << host->root() << "\n";
    std::cout << "App ID: " << app_id << "\n";

    if (!version.empty()) {
        std::cout << "Version: " << version << "\n";
    }
    std::cout << "\n";

    // Check if app exists
    if (!host->isApplicationInstalled(app_id, version)) {
        std::cerr << "Error: Application not installed: " << app_id;
        if (!version.empty()) {
            std::cerr << "@" << version;
        }
        std::cerr << "\n\n";

        // Show available apps
        auto apps = host->listApplications();
        if (!apps.empty()) {
            std::cerr << "Available applications:\n";
            for (const auto& app : apps) {
                std::cerr << "  " << app.id << "@" << app.version << "\n";
            }
        }
        return 1;
    }

    // Get launch contract
    auto result = host->getLaunchContract(app_id, version, enable_trace);

    if (!result.ok) {
        std::cerr << "Error composing contract:\n";
        if (result.critical_error) {
            std::cerr << "  " << nah::core::critical_error_to_string(*result.critical_error) << "\n";
        }
        std::cerr << "  " << result.critical_error_context << "\n";
        return 1;
    }

    const auto& c = result.contract;

    // Display contract details
    std::cout << "=== Application ===\n";
    std::cout << "ID: " << c.app.id << "\n";
    std::cout << "Version: " << c.app.version << "\n";
    std::cout << "Root: " << c.app.root << "\n";
    std::cout << "Entrypoint: " << c.app.entrypoint << "\n";
    std::cout << "\n";

    std::cout << "=== NAK ===\n";
    if (!c.nak.id.empty()) {
        std::cout << "ID: " << c.nak.id << "\n";
        std::cout << "Version: " << c.nak.version << "\n";
        std::cout << "Root: " << c.nak.root << "\n";
        // NAK binding is determined by context, not stored in contract
    } else {
        std::cout << "(no NAK)\n";
    }
    std::cout << "\n";

    std::cout << "=== Execution ===\n";
    std::cout << "Binary: " << c.execution.binary << "\n";
    std::cout << "CWD: " << c.execution.cwd << "\n";

    if (!c.execution.arguments.empty()) {
        std::cout << "Arguments:\n";
        for (const auto& arg : c.execution.arguments) {
            std::cout << "  " << arg << "\n";
        }
    }

    std::cout << "Library Path Key: " << c.execution.library_path_env_key << "\n";
    if (!c.execution.library_paths.empty()) {
        std::cout << "Library Paths:\n";
        for (const auto& p : c.execution.library_paths) {
            std::cout << "  " << p << "\n";
        }
    }
    std::cout << "\n";

    // Environment variables
    std::cout << "=== Environment (NAH_*) ===\n";
    for (const auto& [k, v] : c.environment) {
        if (k.rfind("NAH_", 0) == 0) {
            // Truncate long values
            if (v.length() > 60) {
                std::cout << k << "=" << v.substr(0, 57) << "...\n";
            } else {
                std::cout << k << "=" << v << "\n";
            }
        }
    }

    // Count non-NAH env vars
    int other_count = 0;
    for (const auto& [k, v] : c.environment) {
        if (k.rfind("NAH_", 0) != 0) {
            other_count++;
        }
    }
    if (other_count > 0) {
        std::cout << "\n(Plus " << other_count << " other environment variables)\n";
    }
    std::cout << "\n";

    // Trust state
    std::cout << "=== Trust ===\n";
    std::cout << "State: " << nah::core::trust_state_to_string(c.trust.state) << "\n";
    if (!c.trust.source.empty()) {
        std::cout << "Source: " << c.trust.source << "\n";
    }
    std::cout << "\n";

    // Warnings
    if (!result.warnings.empty()) {
        std::cout << "=== Warnings (" << result.warnings.size() << ") ===\n";
        for (const auto& w : result.warnings) {
            std::cout << "[" << std::setw(6) << w.action << "] " << w.key << "\n";
        }
        std::cout << "\n";
    }

    // Trace
    if (enable_trace && result.trace) {
        std::cout << "=== Composition Trace ===\n";
        for (const auto& decision : result.trace->decisions) {
            std::cout << "  " << decision << "\n";
        }
        std::cout << "\n";
    }

    // Execute if requested
    if (execute) {
        std::cout << "=== Execution ===\n";
        std::cout << "Launching " << app_id << "...\n";
        std::cout << std::string(60, '-') << "\n";

        int exit_code = host->executeContract(result.contract);

        std::cout << std::string(60, '-') << "\n";
        std::cout << "Application exited with code: " << exit_code << "\n";
        return exit_code;
    }

    return 0;
}