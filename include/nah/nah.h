/**
 * @file nah.h
 * @brief NAH C API - Stable ABI for host integration
 *
 * This header provides a C-compatible API for NAH contract composition.
 * It is designed for:
 * - FFI from other languages (Rust, Go, Python, etc.)
 * - Hosts with C-only toolchains
 * - Stable ABI across NAH library updates
 *
 * ## Design Principles
 *
 * 1. **Opaque handles**: All types are pointers to opaque structs.
 *    Internal layout is never exposed, allowing ABI stability.
 *
 * 2. **Ownership**: Functions returning `char*` return newly allocated
 *    strings that the caller must free with `nah_free_string()`.
 *    Functions returning `const char*` return borrowed pointers valid
 *    only while the parent handle is alive.
 *
 * 3. **Error handling**: All fallible operations return a status code.
 *    Use `nah_get_last_error()` for details. Errors are thread-local.
 *
 * 4. **No exceptions**: The C++ implementation catches all exceptions
 *    and converts them to error codes.
 *
 * 5. **Versioning**: Use `nah_api_version()` to check ABI compatibility.
 *
 * ## Example
 *
 * ```c
 * #include <nah/nah.h>
 * #include <stdio.h>
 *
 * int main(void) {
 *     NahHost* host = nah_host_create("/nah");
 *     if (!host) {
 *         fprintf(stderr, "Error: %s\n", nah_get_last_error());
 *         return 1;
 *     }
 *
 *     NahContract* contract = nah_host_get_contract(host, "com.example.app", NULL, NULL);
 *     if (!contract) {
 *         fprintf(stderr, "Error: %s\n", nah_get_last_error());
 *         nah_host_destroy(host);
 *         return 1;
 *     }
 *
 *     printf("Binary: %s\n", nah_contract_binary(contract));
 *     printf("CWD: %s\n", nah_contract_cwd(contract));
 *
 *     // Get environment as JSON (caller must free)
 *     char* env_json = nah_contract_environment_json(contract);
 *     printf("Environment: %s\n", env_json);
 *     nah_free_string(env_json);
 *
 *     nah_contract_destroy(contract);
 *     nah_host_destroy(host);
 *     return 0;
 * }
 * ```
 *
 * ## Thread Safety
 *
 * - `NahHost` instances are NOT thread-safe. Use one per thread or
 *   synchronize externally.
 * - `nah_get_last_error()` is thread-local.
 * - Handles must be destroyed on the same thread that created them.
 */

#ifndef NAH_H
#define NAH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * ABI Version
 * ============================================================================
 *
 * NAH_ABI_VERSION is incremented when breaking changes are made to the C API.
 * This is independent of the library version (from VERSION file).
 *
 * Hosts should check ABI compatibility at startup:
 *   if (nah_abi_version() != NAH_ABI_VERSION) { ... }
 *
 * The library version (nah_version_string()) follows SemVer from VERSION file.
 */

#define NAH_ABI_VERSION 1

/* ============================================================================
 * Export Macros
 * ============================================================================ */

#if defined(_WIN32) || defined(_WIN64)
    #ifdef NAH_BUILDING_SHARED
        #define NAH_CAPI __declspec(dllexport)
    #elif defined(NAH_SHARED)
        #define NAH_CAPI __declspec(dllimport)
    #else
        #define NAH_CAPI
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #ifdef NAH_BUILDING_SHARED
        #define NAH_CAPI __attribute__((visibility("default")))
    #else
        #define NAH_CAPI
    #endif
#else
    #define NAH_CAPI
#endif

/* ============================================================================
 * Opaque Handle Types
 * ============================================================================
 *
 * All handles are pointers to opaque structs. The internal layout is
 * never exposed, ensuring ABI stability across library versions.
 */

/** @brief Opaque handle to a NAH host instance */
typedef struct NahHost NahHost;

/** @brief Opaque handle to a launch contract */
typedef struct NahContract NahContract;

/** @brief Opaque handle to an application list */
typedef struct NahAppList NahAppList;

/** @brief Opaque handle to a string list */
typedef struct NahStringList NahStringList;

/* ============================================================================
 * Status Codes
 * ============================================================================ */

typedef enum NahStatus {
    NAH_OK = 0,
    NAH_ERROR_INVALID_ARGUMENT = 1,
    NAH_ERROR_NOT_FOUND = 2,
    NAH_ERROR_IO = 3,
    NAH_ERROR_PARSE = 4,
    NAH_ERROR_MANIFEST_MISSING = 5,
    NAH_ERROR_ENTRYPOINT_NOT_FOUND = 6,
    NAH_ERROR_PATH_TRAVERSAL = 7,
    NAH_ERROR_INSTALL_RECORD_INVALID = 8,
    NAH_ERROR_NAK_LOADER_INVALID = 9,
    NAH_ERROR_INTERNAL = 99
} NahStatus;

/* ============================================================================
 * API Version
 * ============================================================================ */

/**
 * @brief Get the ABI version of the loaded library
 * @return ABI version number (compare with NAH_ABI_VERSION)
 *
 * Hosts should check this at startup:
 * ```c
 * if (nah_abi_version() != NAH_ABI_VERSION) {
 *     fprintf(stderr, "NAH ABI version mismatch: expected %d, got %d\n",
 *             NAH_ABI_VERSION, nah_abi_version());
 *     exit(1);
 * }
 * ```
 */
NAH_CAPI int32_t nah_abi_version(void);

/**
 * @brief Get the library version string
 * @return Version string (e.g., "1.2.3"). Pointer is valid for program lifetime.
 */
NAH_CAPI const char* nah_version_string(void);

/* ============================================================================
 * Error Handling
 * ============================================================================ */

/**
 * @brief Get the last error message (thread-local)
 * @return Error message string, or empty string if no error.
 *         Pointer valid until next NAH call on this thread.
 *
 * After any function returns NULL or an error status, call this
 * to get a human-readable error message.
 */
NAH_CAPI const char* nah_get_last_error(void);

/**
 * @brief Get the last error code (thread-local)
 * @return Status code from the last failed operation
 */
NAH_CAPI NahStatus nah_get_last_error_code(void);

/**
 * @brief Clear the last error (thread-local)
 */
NAH_CAPI void nah_clear_error(void);

/* ============================================================================
 * Memory Management
 * ============================================================================ */

/**
 * @brief Free a string returned by NAH functions
 * @param str String to free (NULL is safe)
 *
 * Functions that return `char*` (not `const char*`) allocate memory
 * that the caller must free with this function.
 */
NAH_CAPI void nah_free_string(char* str);

/* ============================================================================
 * Host Lifecycle
 * ============================================================================ */

/**
 * @brief Create a NAH host instance
 * @param root_path Path to the NAH root directory (e.g., "/nah")
 * @return Host handle, or NULL on error (check nah_get_last_error())
 *
 * The root path is copied; the caller can free it after this call.
 * The returned handle must be destroyed with nah_host_destroy().
 */
NAH_CAPI NahHost* nah_host_create(const char* root_path);

/**
 * @brief Destroy a NAH host instance
 * @param host Host handle (NULL is safe)
 *
 * All contracts and app lists obtained from this host become invalid.
 * Destroy them before destroying the host.
 */
NAH_CAPI void nah_host_destroy(NahHost* host);

/**
 * @brief Get the root path of a host
 * @param host Host handle
 * @return Root path. Pointer valid while host is alive.
 */
NAH_CAPI const char* nah_host_root(const NahHost* host);

/* ============================================================================
 * Application Listing
 * ============================================================================ */

/**
 * @brief List all installed applications
 * @param host Host handle
 * @return App list handle, or NULL on error
 *
 * The returned list must be destroyed with nah_app_list_destroy().
 */
NAH_CAPI NahAppList* nah_host_list_apps(NahHost* host);

/**
 * @brief Get the number of apps in a list
 * @param list App list handle
 * @return Number of apps
 */
NAH_CAPI int32_t nah_app_list_count(const NahAppList* list);

/**
 * @brief Get app ID at index
 * @param list App list handle
 * @param index Zero-based index
 * @return App ID, or NULL if index out of bounds. Valid while list is alive.
 */
NAH_CAPI const char* nah_app_list_id(const NahAppList* list, int32_t index);

/**
 * @brief Get app version at index
 * @param list App list handle
 * @param index Zero-based index
 * @return App version, or NULL if index out of bounds. Valid while list is alive.
 */
NAH_CAPI const char* nah_app_list_version(const NahAppList* list, int32_t index);

/**
 * @brief Destroy an app list
 * @param list App list handle (NULL is safe)
 */
NAH_CAPI void nah_app_list_destroy(NahAppList* list);

/* ============================================================================
 * Profile Management
 * ============================================================================ */

/**
 * @brief List available profile names
 * @param host Host handle
 * @return String list handle, or NULL on error
 */
NAH_CAPI NahStringList* nah_host_list_profiles(NahHost* host);

/**
 * @brief Get the active profile name
 * @param host Host handle
 * @return Profile name (caller must free), or NULL if none active
 */
NAH_CAPI char* nah_host_active_profile(NahHost* host);

/**
 * @brief Set the active profile
 * @param host Host handle
 * @param name Profile name
 * @return NAH_OK on success, error code on failure
 */
NAH_CAPI NahStatus nah_host_set_profile(NahHost* host, const char* name);

/* ============================================================================
 * String List
 * ============================================================================ */

/**
 * @brief Get the number of strings in a list
 * @param list String list handle
 * @return Number of strings
 */
NAH_CAPI int32_t nah_string_list_count(const NahStringList* list);

/**
 * @brief Get string at index
 * @param list String list handle
 * @param index Zero-based index
 * @return String, or NULL if index out of bounds. Valid while list is alive.
 */
NAH_CAPI const char* nah_string_list_get(const NahStringList* list, int32_t index);

/**
 * @brief Destroy a string list
 * @param list String list handle (NULL is safe)
 */
NAH_CAPI void nah_string_list_destroy(NahStringList* list);

/* ============================================================================
 * Contract Composition
 * ============================================================================ */

/**
 * @brief Get a launch contract for an application
 * @param host Host handle
 * @param app_id Application identifier (e.g., "com.example.app")
 * @param version Specific version, or NULL for latest
 * @param profile Profile name, or NULL for active profile
 * @return Contract handle, or NULL on error
 *
 * The returned contract must be destroyed with nah_contract_destroy().
 */
NAH_CAPI NahContract* nah_host_get_contract(
    NahHost* host,
    const char* app_id,
    const char* version,
    const char* profile
);

/**
 * @brief Destroy a contract
 * @param contract Contract handle (NULL is safe)
 */
NAH_CAPI void nah_contract_destroy(NahContract* contract);

/* ============================================================================
 * Contract Accessors - Execution
 * ============================================================================ */

/**
 * @brief Get the binary path to execute
 * @param contract Contract handle
 * @return Absolute path to binary. Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_binary(const NahContract* contract);

/**
 * @brief Get the working directory
 * @param contract Contract handle
 * @return Absolute path to cwd. Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_cwd(const NahContract* contract);

/**
 * @brief Get the number of command-line arguments
 * @param contract Contract handle
 * @return Number of arguments (0 if none)
 */
NAH_CAPI int32_t nah_contract_argc(const NahContract* contract);

/**
 * @brief Get command-line argument at index
 * @param contract Contract handle
 * @param index Zero-based index
 * @return Argument string, or NULL if index out of bounds. Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_argv(const NahContract* contract, int32_t index);

/* ============================================================================
 * Contract Accessors - Library Paths
 * ============================================================================ */

/**
 * @brief Get the library path environment variable name
 * @param contract Contract handle
 * @return Environment variable name (e.g., "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH").
 *         Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_library_path_env_key(const NahContract* contract);

/**
 * @brief Get the number of library paths
 * @param contract Contract handle
 * @return Number of library paths
 */
NAH_CAPI int32_t nah_contract_library_path_count(const NahContract* contract);

/**
 * @brief Get library path at index
 * @param contract Contract handle
 * @param index Zero-based index
 * @return Library path, or NULL if index out of bounds. Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_library_path(const NahContract* contract, int32_t index);

/**
 * @brief Get library paths as a single string with platform separator
 * @param contract Contract handle
 * @return Joined paths (caller must free), or NULL on error
 *
 * Example: "/nah/naks/sdk/1.0/lib:/nah/apps/app/lib"
 */
NAH_CAPI char* nah_contract_library_paths_joined(const NahContract* contract);

/* ============================================================================
 * Contract Accessors - Environment
 * ============================================================================ */

/**
 * @brief Get environment variables as JSON object
 * @param contract Contract handle
 * @return JSON string (caller must free), e.g., {"VAR1":"value1","VAR2":"value2"}
 *
 * The JSON object maps variable names to string values.
 */
NAH_CAPI char* nah_contract_environment_json(const NahContract* contract);

/**
 * @brief Get a specific environment variable
 * @param contract Contract handle
 * @param name Variable name
 * @return Variable value, or NULL if not set. Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_environment_get(const NahContract* contract, const char* name);

/* ============================================================================
 * Contract Accessors - App/NAK Info
 * ============================================================================ */

/**
 * @brief Get app ID from contract
 * @param contract Contract handle
 * @return App ID. Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_app_id(const NahContract* contract);

/**
 * @brief Get app version from contract
 * @param contract Contract handle
 * @return App version. Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_app_version(const NahContract* contract);

/**
 * @brief Get app root directory
 * @param contract Contract handle
 * @return Absolute path to app root. Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_app_root(const NahContract* contract);

/**
 * @brief Get NAK ID from contract
 * @param contract Contract handle
 * @return NAK ID. Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_nak_id(const NahContract* contract);

/**
 * @brief Get NAK version from contract
 * @param contract Contract handle
 * @return NAK version. Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_nak_version(const NahContract* contract);

/**
 * @brief Get NAK root directory
 * @param contract Contract handle
 * @return Absolute path to NAK root. Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_nak_root(const NahContract* contract);

/* ============================================================================
 * Contract Accessors - Warnings
 * ============================================================================ */

/**
 * @brief Get the number of warnings
 * @param contract Contract handle
 * @return Number of warnings
 */
NAH_CAPI int32_t nah_contract_warning_count(const NahContract* contract);

/**
 * @brief Get warning key at index
 * @param contract Contract handle
 * @param index Zero-based index
 * @return Warning key (e.g., "nak_not_found"), or NULL if index out of bounds.
 *         Valid while contract is alive.
 */
NAH_CAPI const char* nah_contract_warning_key(const NahContract* contract, int32_t index);

/**
 * @brief Get all warnings as JSON array
 * @param contract Contract handle
 * @return JSON array string (caller must free)
 *
 * Example: [{"key":"nak_not_found","action":"warn","nak_id":"com.example.sdk"}]
 */
NAH_CAPI char* nah_contract_warnings_json(const NahContract* contract);

/* ============================================================================
 * Contract Serialization
 * ============================================================================ */

/**
 * @brief Serialize entire contract to JSON
 * @param contract Contract handle
 * @return JSON string (caller must free)
 *
 * Returns the complete contract in the SPEC-defined JSON format.
 */
NAH_CAPI char* nah_contract_to_json(const NahContract* contract);

#ifdef __cplusplus
}
#endif

#endif /* NAH_H */
