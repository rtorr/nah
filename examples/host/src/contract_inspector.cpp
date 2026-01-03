/**
 * Contract Inspector
 * ==================
 * Inspect and compare launch contracts for applications.
 */

#include <nah/nahhost.hpp>
#include <nah/contract.hpp>
#include <iostream>

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <nah_root> <app_id> [profile]\n";
    std::cerr << "\nInspects the launch contract for an application.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string nah_root = argv[1];
    std::string app_id = argv[2];
    std::string profile = argc > 3 ? argv[3] : "";
    
    std::cout << "Contract Inspector\n";
    std::cout << "==================\n\n";
    std::cout << "NAH Root: " << nah_root << "\n";
    std::cout << "App ID: " << app_id << "\n";
    if (!profile.empty()) {
        std::cout << "Profile: " << profile << "\n";
    }
    std::cout << "\n";
    
    auto host = nah::NahHost::create(nah_root);
    
    auto result = host->getLaunchContract(app_id, "", profile, true);
    
    if (result.isErr()) {
        std::cerr << "Error: " << result.error().message() << "\n";
        return 1;
    }
    
    const auto& envelope = result.value();
    const auto& c = envelope.contract;
    
    std::cout << "=== Application ===\n";
    std::cout << "ID: " << c.app.id << "\n";
    std::cout << "Version: " << c.app.version << "\n";
    std::cout << "Root: " << c.app.root << "\n";
    std::cout << "Entrypoint: " << c.app.entrypoint << "\n";
    std::cout << "\n";
    
    std::cout << "=== NAK ===\n";
    std::cout << "ID: " << c.nak.id << "\n";
    std::cout << "Version: " << c.nak.version << "\n";
    std::cout << "Root: " << c.nak.root << "\n";
    std::cout << "\n";
    
    std::cout << "=== Execution ===\n";
    std::cout << "Binary: " << c.execution.binary << "\n";
    std::cout << "CWD: " << c.execution.cwd << "\n";
    std::cout << "Library Path Key: " << c.execution.library_path_env_key << "\n";
    std::cout << "Library Paths:\n";
    for (const auto& p : c.execution.library_paths) {
        std::cout << "  " << p << "\n";
    }
    std::cout << "\n";
    
    std::cout << "=== Environment (NAH_*) ===\n";
    for (const auto& [k, v] : c.environment) {
        if (k.rfind("NAH_", 0) == 0) {
            std::cout << k << "=" << v << "\n";
        }
    }
    std::cout << "\n";
    
    if (!envelope.warnings.empty()) {
        std::cout << "=== Warnings ===\n";
        for (const auto& w : envelope.warnings) {
            std::cout << "[" << w.action << "] " << w.key << "\n";
        }
        std::cout << "\n";
    }
    
    return 0;
}
