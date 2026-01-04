#include <nah/nahhost.hpp>
#include <nah/manifest.hpp>
#include <nah/semver.hpp>
#include <iostream>

int main() {
    std::cout << "NAH library test\n";
    std::cout << "================\n\n";

    // Test version parsing
    auto version = nah::parse_version("1.2.3");
    if (version) {
        std::cout << "Parsed version: " << version->major() << "."
                  << version->minor() << "." << version->patch() << "\n";
    } else {
        std::cerr << "Failed to parse version\n";
        return 1;
    }

    // Test range parsing
    auto range = nah::parse_range(">=1.0.0 <2.0.0");
    if (range) {
        std::cout << "Parsed range with " << range->sets.size() << " comparator set(s)\n";
        std::cout << "Selection key: " << range->selection_key() << "\n";
    } else {
        std::cerr << "Failed to parse range\n";
        return 1;
    }

    // Test range satisfaction
    if (nah::satisfies(*version, *range)) {
        std::cout << "Version 1.2.3 satisfies >=1.0.0 <2.0.0: yes\n";
    } else {
        std::cout << "Version 1.2.3 satisfies >=1.0.0 <2.0.0: no\n";
    }

    std::cout << "\nNAH library is working correctly!\n";
    return 0;
}
