/**
 * @file c_api_tests.cpp
 * @brief Tests for the NAH C API
 *
 * These tests verify the C API wrapper functions work correctly,
 * focusing on NULL safety, error handling, and memory management.
 * 
 * Full integration tests that require a NAH root are in integration tests.
 */

#include <doctest/doctest.h>
#include <nah/nah.h>

#include <cstring>

// =============================================================================
// Version Tests
// =============================================================================

TEST_CASE("C API: nah_abi_version returns correct version") {
    CHECK(nah_abi_version() == NAH_ABI_VERSION);
}

TEST_CASE("C API: nah_version_string returns non-empty string") {
    const char* version = nah_version_string();
    REQUIRE(version != nullptr);
    CHECK(strlen(version) > 0);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("C API: error handling") {
    SUBCASE("initial state has no error") {
        nah_clear_error();
        CHECK(nah_get_last_error_code() == NAH_OK);
        CHECK(strlen(nah_get_last_error()) == 0);
    }
    
    SUBCASE("NULL root_path sets error") {
        NahHost* host = nah_host_create(nullptr);
        CHECK(host == nullptr);
        CHECK(nah_get_last_error_code() == NAH_ERROR_INVALID_ARGUMENT);
        CHECK(strlen(nah_get_last_error()) > 0);
    }
    
    SUBCASE("clear_error resets state") {
        nah_host_create(nullptr);  // Set an error
        nah_clear_error();
        CHECK(nah_get_last_error_code() == NAH_OK);
    }
}

// =============================================================================
// Host Lifecycle Tests
// =============================================================================

TEST_CASE("C API: host destroy NULL is safe") {
    nah_host_destroy(nullptr);  // Should not crash
}

TEST_CASE("C API: host_root with NULL returns empty") {
    CHECK(strcmp(nah_host_root(nullptr), "") == 0);
}

// NOTE: Full host create/destroy tests are in integration tests.
// Unit tests focus on NULL safety and error handling.

// =============================================================================
// App List Tests
// =============================================================================

TEST_CASE("C API: app list NULL safety") {
    CHECK(nah_app_list_count(nullptr) == 0);
    CHECK(nah_app_list_id(nullptr, 0) == nullptr);
    CHECK(nah_app_list_version(nullptr, 0) == nullptr);
    nah_app_list_destroy(nullptr);  // Should not crash
}

TEST_CASE("C API: list_apps with NULL host returns NULL") {
    NahAppList* apps = nah_host_list_apps(nullptr);
    CHECK(apps == nullptr);
    CHECK(nah_get_last_error_code() == NAH_ERROR_INVALID_ARGUMENT);
}

// =============================================================================
// Profile Tests
// =============================================================================

TEST_CASE("C API: profile NULL safety") {
    NahStatus status = nah_host_set_profile(nullptr, "default");
    CHECK(status == NAH_ERROR_INVALID_ARGUMENT);
    
    NahStringList* profiles = nah_host_list_profiles(nullptr);
    CHECK(profiles == nullptr);
}

// =============================================================================
// String List Tests
// =============================================================================

TEST_CASE("C API: string list") {
    SUBCASE("NULL list is safe") {
        CHECK(nah_string_list_count(nullptr) == 0);
        CHECK(nah_string_list_get(nullptr, 0) == nullptr);
        nah_string_list_destroy(nullptr);  // Should not crash
    }
}

// =============================================================================
// Contract Tests
// =============================================================================

TEST_CASE("C API: contract NULL safety") {
    NahContract* contract = nah_host_get_contract(nullptr, "com.example.app", nullptr, nullptr);
    CHECK(contract == nullptr);
    CHECK(nah_get_last_error_code() == NAH_ERROR_INVALID_ARGUMENT);
    
    contract = nah_host_get_contract(nullptr, nullptr, nullptr, nullptr);
    CHECK(contract == nullptr);
    
    nah_contract_destroy(nullptr);  // Should not crash
}

// =============================================================================
// Contract Accessor Tests (NULL safety)
// =============================================================================

TEST_CASE("C API: contract accessors with NULL") {
    CHECK(strcmp(nah_contract_binary(nullptr), "") == 0);
    CHECK(strcmp(nah_contract_cwd(nullptr), "") == 0);
    CHECK(nah_contract_argc(nullptr) == 0);
    CHECK(nah_contract_argv(nullptr, 0) == nullptr);
    CHECK(strcmp(nah_contract_library_path_env_key(nullptr), "") == 0);
    CHECK(nah_contract_library_path_count(nullptr) == 0);
    CHECK(nah_contract_library_path(nullptr, 0) == nullptr);
    CHECK(strcmp(nah_contract_app_id(nullptr), "") == 0);
    CHECK(strcmp(nah_contract_app_version(nullptr), "") == 0);
    CHECK(strcmp(nah_contract_app_root(nullptr), "") == 0);
    CHECK(strcmp(nah_contract_nak_id(nullptr), "") == 0);
    CHECK(strcmp(nah_contract_nak_version(nullptr), "") == 0);
    CHECK(strcmp(nah_contract_nak_root(nullptr), "") == 0);
    CHECK(nah_contract_warning_count(nullptr) == 0);
    CHECK(nah_contract_warning_key(nullptr, 0) == nullptr);
    CHECK(nah_contract_environment_get(nullptr, "FOO") == nullptr);
}

// =============================================================================
// Memory Management Tests
// =============================================================================

TEST_CASE("C API: memory management") {
    SUBCASE("free_string with NULL is safe") {
        nah_free_string(nullptr);  // Should not crash
    }
    
    SUBCASE("library_paths_joined with NULL returns empty") {
        char* paths = nah_contract_library_paths_joined(nullptr);
        REQUIRE(paths != nullptr);
        CHECK(strcmp(paths, "") == 0);
        nah_free_string(paths);
    }
    
    SUBCASE("environment_json with NULL returns empty object") {
        char* json = nah_contract_environment_json(nullptr);
        REQUIRE(json != nullptr);
        CHECK(strcmp(json, "{}") == 0);
        nah_free_string(json);
    }
    
    SUBCASE("warnings_json with NULL returns empty array") {
        char* json = nah_contract_warnings_json(nullptr);
        REQUIRE(json != nullptr);
        CHECK(strcmp(json, "[]") == 0);
        nah_free_string(json);
    }
    
    SUBCASE("contract_to_json with NULL returns empty object") {
        char* json = nah_contract_to_json(nullptr);
        REQUIRE(json != nullptr);
        CHECK(strcmp(json, "{}") == 0);
        nah_free_string(json);
    }
}


