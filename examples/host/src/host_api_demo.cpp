/**
 * Host API Demo
 * ==============
 * Demonstrates the NAH C++ API for host integration.
 */

#include <nah/nahhost.hpp>
#include <iostream>

void print_separator() {
    std::cout << std::string(60, '-') << "\n";
}

int main(int argc, char* argv[]) {
    std::string nah_root = "/nah";
    
    if (argc > 1) {
        nah_root = argv[1];
    }
    
    std::cout << "NAH Host API Demo\n";
    std::cout << "=================\n\n";
    std::cout << "NAH Root: " << nah_root << "\n\n";
    
    auto host = nah::NahHost::create(nah_root);
    
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
    
    // List profiles
    print_separator();
    std::cout << "Available Profiles:\n";
    print_separator();
    
    auto profiles = host->listProfiles();
    if (profiles.empty()) {
        std::cout << "  (no profiles found)\n";
    } else {
        for (const auto& p : profiles) {
            std::cout << "  " << p << "\n";
        }
    }
    std::cout << "\n";
    
    // Show active profile
    print_separator();
    std::cout << "Active Profile:\n";
    print_separator();
    
    auto profile_result = host->getActiveHostProfile();
    if (profile_result.isOk()) {
        const auto& profile = profile_result.value();
        std::cout << "  Schema: " << profile.schema << "\n";
        std::cout << "  Binding Mode: " << nah::binding_mode_to_string(profile.nak.binding_mode) << "\n";
        if (!profile.environment.empty()) {
            std::cout << "  Environment:\n";
            for (const auto& [k, v] : profile.environment) {
                std::cout << "    " << k << "=" << v << "\n";
            }
        }
    } else {
        std::cout << "  (no active profile)\n";
    }
    std::cout << "\n";
    
    // Get launch contract for first app
    if (!apps.empty()) {
        print_separator();
        std::cout << "Launch Contract for " << apps[0].id << ":\n";
        print_separator();
        
        auto contract_result = host->getLaunchContract(apps[0].id, apps[0].version, "", false);
        if (contract_result.isOk()) {
            const auto& envelope = contract_result.value();
            const auto& c = envelope.contract;
            
            std::cout << "  App: " << c.app.id << " v" << c.app.version << "\n";
            std::cout << "  NAK: " << c.nak.id << " v" << c.nak.version << "\n";
            std::cout << "  Binary: " << c.execution.binary << "\n";
            std::cout << "  CWD: " << c.execution.cwd << "\n";
            
            if (!envelope.warnings.empty()) {
                std::cout << "  Warnings: " << envelope.warnings.size() << "\n";
            }
        } else {
            std::cout << "  Error: " << contract_result.error().message() << "\n";
        }
    }
    
    std::cout << "\n";
    print_separator();
    std::cout << "Demo complete.\n";
    
    return 0;
}
