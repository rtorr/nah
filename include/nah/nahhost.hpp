#pragma once

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

    // Profile load failures
    PROFILE_MISSING,
    PROFILE_PARSE_ERROR,
};

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

class NahHost {
public:
    // Creation
    static std::unique_ptr<NahHost> create(const std::string& root_path);
    
    // Get the NAH root path
    const std::string& root() const { return root_; }

    // Application discovery
    std::vector<AppInfo> listApplications() const;
    Result<AppInfo> findApplication(const std::string& id,
                                     const std::string& version = "") const;

    // Profile management
    Result<HostProfile> getActiveHostProfile() const;
    Result<void> setActiveHostProfile(const std::string& name);
    std::vector<std::string> listProfiles() const;
    Result<HostProfile> loadProfile(const std::string& name) const;
    Result<void> validateProfile(const HostProfile& profile) const;

    // Launch contract generation
    Result<ContractEnvelope> getLaunchContract(
        const std::string& app_id,
        const std::string& version = "",
        const std::string& profile = "",
        bool enable_trace = false) const;
    
    // Low-level contract composition
    Result<ContractEnvelope> composeContract(
        const CompositionInputs& inputs) const;

private:
    explicit NahHost(std::string root) : root_(std::move(root)) {}
    
    // Resolve active profile following SPEC resolution rules
    Result<HostProfile> resolveActiveProfile(const std::string& explicit_name = "") const;
    
    std::string root_;
};

} // namespace nah
