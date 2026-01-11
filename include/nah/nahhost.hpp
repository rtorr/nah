#pragma once

/**
 * @file nahhost.hpp
 * @brief Main NAH library interface for contract composition
 * 
 * This is the primary header for using NAH as a library. It provides:
 * - NahHost: The main class for interacting with a NAH root
 * - Result<T>: Error handling type for fallible operations
 * - AppInfo: Application metadata
 * 
 * @example
 * ```cpp
 * #include <nah/nahhost.hpp>
 * 
 * auto host = nah::NahHost::create("/nah");
 * auto contract = host->getLaunchContract("com.example.myapp");
 * if (contract.isOk()) {
 *     // Use contract.value() to launch the app
 * }
 * ```
 */

#include "nah/types.hpp"
#include "nah/contract.hpp"
#include "nah/host_profile.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nah {

// ============================================================================
// App Info
// ============================================================================

/**
 * @brief Metadata about an installed application
 */
struct AppInfo {
    std::string id;
    std::string version;
    std::string instance_id;
    std::string install_root;
    std::string record_path;
};

// ============================================================================
// Error Handling (per SPEC L1692-L1738)
// ============================================================================

/**
 * @brief Error codes for NAH operations
 */
enum class ErrorCode {
    // System / IO
    FILE_NOT_FOUND,
    PERMISSION_DENIED,
    IO_ERROR,

    // Contract composition critical errors (normative)
    MANIFEST_MISSING,
    INSTALL_RECORD_INVALID,
    PATH_TRAVERSAL,
    ENTRYPOINT_NOT_FOUND,
    NAK_LOADER_INVALID,

    // Profile load failures
    PROFILE_MISSING,
    PROFILE_PARSE_ERROR,
};

/**
 * @brief Error type with code and message
 */
class Error {
public:
    Error(ErrorCode code, std::string message)
        : code_(code), message_(std::move(message)) {}
    
    Error& withContext(const std::string& context) {
        message_ = context + ": " + message_;
        return *this;
    }

    ErrorCode code() const { return code_; }
    const std::string& message() const { return message_; }
    std::string toString() const { return message_; }

private:
    ErrorCode code_;
    std::string message_;
};

// ============================================================================
// Result Type (per SPEC L1692-L1710)
// ============================================================================

/**
 * @brief Result type for fallible operations
 * @tparam T The success value type
 * @tparam E The error type (default: Error)
 * 
 * Used throughout the NAH API for operations that can fail.
 * Check isOk() before accessing value(), or isErr() before error().
 */
template<typename T, typename E = Error>
class Result {
public:
    static Result ok(T value) { return Result(std::move(value)); }
    static Result err(E error) { return Result(std::move(error)); }

    bool isOk() const { return has_value_; }
    bool isErr() const { return !has_value_; }

    T& value() { return value_.value(); }
    const T& value() const { return value_.value(); }
    E& error() { return error_.value(); }
    const E& error() const { return error_.value(); }
    
    T valueOr(T default_value) const {
        if (has_value_) return value_.value();
        return default_value;
    }

    template<typename F>
    auto map(F func) -> Result<decltype(func(std::declval<T>())), E> {
        if (has_value_) {
            return Result<decltype(func(std::declval<T>())), E>::ok(func(value_.value()));
        }
        return Result<decltype(func(std::declval<T>())), E>::err(error_.value());
    }

    template<typename F>
    auto flatMap(F func) -> decltype(func(std::declval<T>())) {
        if (has_value_) {
            return func(value_.value());
        }
        return decltype(func(std::declval<T>()))::err(error_.value());
    }

private:
    explicit Result(T value) : has_value_(true), value_(std::move(value)) {}
    explicit Result(E error) : has_value_(false), error_(std::move(error)) {}

    bool has_value_;
    std::optional<T> value_;
    std::optional<E> error_;
};

template<typename E>
class Result<void, E> {
public:
    static Result ok() { return Result(true, std::nullopt); }
    static Result err(E error) { return Result(false, std::move(error)); }

    bool isOk() const { return has_value_; }
    bool isErr() const { return !has_value_; }

    void value() const {}
    E& error() { return error_.value(); }
    const E& error() const { return error_.value(); }

private:
    Result(bool hv, std::optional<E> err) : has_value_(hv), error_(std::move(err)) {}
    bool has_value_;
    std::optional<E> error_;
};

// ============================================================================
// NahHost Class (per SPEC L1660-L1684)
// ============================================================================

/**
 * @brief Main interface for interacting with a NAH root
 * 
 * NahHost provides methods for:
 * - Listing and finding installed applications
 * - Managing host profiles
 * - Generating launch contracts
 * 
 * @example
 * ```cpp
 * auto host = nah::NahHost::create("/nah");
 * 
 * // List all apps
 * for (const auto& app : host->listApplications()) {
 *     std::cout << app.id << "@" << app.version << "\n";
 * }
 * 
 * // Get launch contract
 * auto result = host->getLaunchContract("com.example.myapp");
 * if (result.isOk()) {
 *     const auto& contract = result.value().contract;
 *     // Launch using contract.execution.binary, etc.
 * }
 * ```
 */
class NahHost {
public:
    /**
     * @brief Create a NahHost instance for a NAH root directory
     * @param root_path Path to the NAH root (e.g., "/nah")
     * @return Unique pointer to NahHost instance
     */
    static std::unique_ptr<NahHost> create(const std::string& root_path);
    
    /// Get the NAH root path
    const std::string& root() const { return root_; }

    /// List all installed applications
    std::vector<AppInfo> listApplications() const;
    
    /**
     * @brief Find an installed application by ID
     * @param id Application identifier (e.g., "com.example.myapp")
     * @param version Optional specific version (default: latest)
     * @return AppInfo or error if not found
     */
    Result<AppInfo> findApplication(const std::string& id,
                                     const std::string& version = "") const;

    /// Get the currently active host profile
    Result<HostProfile> getActiveHostProfile() const;
    
    /// Set the active host profile by name
    Result<void> setActiveHostProfile(const std::string& name);
    
    /// List all available profile names
    std::vector<std::string> listProfiles() const;
    
    /// Load a specific profile by name
    Result<HostProfile> loadProfile(const std::string& name) const;
    
    /// Validate a host profile
    Result<void> validateProfile(const HostProfile& profile) const;

    /**
     * @brief Generate a launch contract for an application
     * @param app_id Application identifier
     * @param version Optional specific version (default: latest installed)
     * @param profile Optional profile name (default: active profile)
     * @param enable_trace Include composition trace in result
     * @return ContractEnvelope containing the launch contract
     */
    Result<ContractEnvelope> getLaunchContract(
        const std::string& app_id,
        const std::string& version = "",
        const std::string& profile = "",
        bool enable_trace = false) const;
    
    /**
     * @brief Low-level contract composition from explicit inputs
     * @param inputs Composition inputs (manifest, profile, paths, etc.)
     * @return ContractEnvelope containing the composed contract
     */
    Result<ContractEnvelope> composeContract(
        const CompositionInputs& inputs) const;

private:
    explicit NahHost(std::string root) : root_(std::move(root)) {}
    
    Result<HostProfile> resolveActiveProfile(const std::string& explicit_name = "") const;
    
    std::string root_;
};

} // namespace nah
