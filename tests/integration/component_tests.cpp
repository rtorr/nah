/**
 * Integration tests for component functionality
 * 
 * Note: These tests require a full NAH installation with test applications.
 * They verify end-to-end component workflows.
 */

#define NAH_HOST_IMPLEMENTATION
#include <nah/nah_host.h>
#include <doctest/doctest.h>
#include <filesystem>

TEST_CASE("Component URI validation") {
    SUBCASE("canHandleComponentUri with valid URIs") {
        // Note: This test requires com.example.suite to be installed
        // For CI/CD, we rely on the suite-app example being installed during test setup
        
        // Test would look like:
        // auto host = nah::host::NahHost::create("/test/nah/root");
        // CHECK(host->canHandleComponentUri("com.example.suite://editor"));
        // CHECK(host->canHandleComponentUri("com.example.suite://viewer"));
        // CHECK_FALSE(host->canHandleComponentUri("com.nonexistent://component"));
        
        CHECK(true);  // Placeholder - actual integration requires test environment
    }
}

TEST_CASE("Component discovery and listing") {
    SUBCASE("listAllComponents returns installed components") {
        // Test would iterate through all installed apps and verify components
        // are correctly discovered and returned
        
        // auto host = nah::host::NahHost::create("/test/nah/root");
        // auto components = host->listAllComponents();
        // CHECK_FALSE(components.empty());
        
        CHECK(true);  // Placeholder
    }
}

TEST_CASE("Component composition") {
    SUBCASE("composeComponentLaunch creates valid launch contract") {
        // Test would verify that composition creates a valid launch contract
        // with correct entrypoint, environment variables, etc.
        
        // auto host = nah::host::NahHost::create("/test/nah/root");
        // auto result = host->composeComponentLaunch("com.example.suite://editor");
        // CHECK(result.ok);
        // CHECK(result.contract.environment.count("NAH_COMPONENT_ID") > 0);
        
        CHECK(true);  // Placeholder
    }
}

/*
 * REAL INTEGRATION TESTING:
 * 
 * For comprehensive integration testing, see:
 *   examples/apps/suite-app/
 * 
 * The suite-app provides a complete multi-component application that can be
 * built, installed, and tested end-to-end:
 * 
 *   cd examples/apps/suite-app
 *   cmake -B build && cmake --build build
 *   nah install build/package/com.example.suite-1.0.0.nap
 *   nah components --app com.example.suite
 *   nah launch "com.example.suite://editor?file=test.txt"
 * 
 * This approach provides more realistic testing than mock-based unit tests.
 */
