/**
 * Conan test_package - verify NAH headers work correctly
 */

#define NAH_HOST_IMPLEMENTATION
#include <nah/nah.h>
#include <iostream>

int main() {
    std::cout << "NAH Core Version: " << nah::core::NAH_CORE_VERSION << "\n";

    // Test basic types
    nah::core::AppDeclaration app;
    app.id = "com.test.app";
    app.version = "1.0.0";
    app.entrypoint_path = "main";

    auto validation = nah::core::validate_declaration(app);
    if (!validation.ok) {
        std::cerr << "Validation failed!\n";
        return 1;
    }

    // Test JSON parsing
    std::string json = R"({
        "id": "com.example.app",
        "version": "1.0.0",
        "entrypoint": "main.lua",
        "nak": { "id": "lua", "version_req": ">=5.4" }
    })";

    auto result = nah::json::parse_app_declaration(json);
    if (!result.ok) {
        std::cerr << "JSON parsing failed: " << result.error << "\n";
        return 1;
    }

    std::cout << "Parsed app: " << result.value.id << " v" << result.value.version << "\n";
    std::cout << "NAH test_package: OK\n";

    return 0;
}
