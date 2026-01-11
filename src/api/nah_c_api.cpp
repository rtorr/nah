/**
 * @file nah_c_api.cpp
 * @brief NAH C API Implementation
 *
 * This file implements the stable C ABI for NAH. All C++ exceptions are
 * caught at the boundary and converted to error codes.
 */

#include "nah/nah.h"
#include "nah/nahhost.hpp"
#include "nah/contract.hpp"

#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

// Version is generated at build time from VERSION file
#ifndef NAH_VERSION_STRING
#define NAH_VERSION_STRING "unknown"
#endif

// ============================================================================
// Thread-Local Error State
// ============================================================================

namespace {

thread_local std::string g_last_error;
thread_local NahStatus g_last_error_code = NAH_OK;

void set_error(NahStatus code, const std::string& message) {
    g_last_error_code = code;
    g_last_error = message;
}

void set_error(NahStatus code, const char* message) {
    g_last_error_code = code;
    g_last_error = message ? message : "";
}

void clear_error() {
    g_last_error_code = NAH_OK;
    g_last_error.clear();
}

NahStatus map_error_code(nah::ErrorCode code) {
    switch (code) {
        case nah::ErrorCode::FILE_NOT_FOUND:
            return NAH_ERROR_NOT_FOUND;
        case nah::ErrorCode::PERMISSION_DENIED:
        case nah::ErrorCode::IO_ERROR:
            return NAH_ERROR_IO;
        case nah::ErrorCode::MANIFEST_MISSING:
            return NAH_ERROR_MANIFEST_MISSING;
        case nah::ErrorCode::INSTALL_RECORD_INVALID:
            return NAH_ERROR_INSTALL_RECORD_INVALID;
        case nah::ErrorCode::PATH_TRAVERSAL:
            return NAH_ERROR_PATH_TRAVERSAL;
        case nah::ErrorCode::ENTRYPOINT_NOT_FOUND:
            return NAH_ERROR_ENTRYPOINT_NOT_FOUND;
        case nah::ErrorCode::NAK_LOADER_INVALID:
            return NAH_ERROR_NAK_LOADER_INVALID;
        case nah::ErrorCode::PROFILE_MISSING:
        case nah::ErrorCode::PROFILE_PARSE_ERROR:
            return NAH_ERROR_PARSE;
        default:
            return NAH_ERROR_INTERNAL;
    }
}

// Duplicate a string for returning to C caller (caller must free)
char* duplicate_string(const std::string& s) {
    char* result = static_cast<char*>(malloc(s.size() + 1));
    if (result) {
        memcpy(result, s.c_str(), s.size() + 1);
    }
    return result;
}

} // namespace

// ============================================================================
// Opaque Handle Implementations
// ============================================================================

struct NahHost {
    std::unique_ptr<nah::NahHost> impl;
    std::string root;  // Keep copy for stable pointer
    
    explicit NahHost(std::unique_ptr<nah::NahHost> h)
        : impl(std::move(h)), root(impl->root()) {}
};

struct NahContract {
    nah::ContractEnvelope envelope;
    
    // Cached strings for stable pointers
    std::string binary;
    std::string cwd;
    std::string library_path_env_key;
    std::string app_id;
    std::string app_version;
    std::string app_root;
    std::string nak_id;
    std::string nak_version;
    std::string nak_root;
    std::vector<std::string> arguments;
    std::vector<std::string> library_paths;
    std::vector<std::string> warning_keys;
    
    explicit NahContract(nah::ContractEnvelope env) : envelope(std::move(env)) {
        // Cache all strings for stable pointers
        const auto& c = envelope.contract;
        binary = c.execution.binary;
        cwd = c.execution.cwd;
        library_path_env_key = c.execution.library_path_env_key;
        app_id = c.app.id;
        app_version = c.app.version;
        app_root = c.app.root;
        nak_id = c.nak.id;
        nak_version = c.nak.version;
        nak_root = c.nak.root;
        arguments = c.execution.arguments;
        library_paths = c.execution.library_paths;
        
        for (const auto& w : envelope.warnings) {
            warning_keys.push_back(w.key);
        }
    }
};

struct NahAppList {
    std::vector<nah::AppInfo> apps;
};

struct NahStringList {
    std::vector<std::string> strings;
};

// ============================================================================
// API Version
// ============================================================================

extern "C" {

NAH_CAPI int32_t nah_abi_version(void) {
    return NAH_ABI_VERSION;
}

NAH_CAPI const char* nah_version_string(void) {
    return NAH_VERSION_STRING;
}

// ============================================================================
// Error Handling
// ============================================================================

NAH_CAPI const char* nah_get_last_error(void) {
    return g_last_error.c_str();
}

NAH_CAPI NahStatus nah_get_last_error_code(void) {
    return g_last_error_code;
}

NAH_CAPI void nah_clear_error(void) {
    clear_error();
}

// ============================================================================
// Memory Management
// ============================================================================

NAH_CAPI void nah_free_string(char* str) {
    free(str);
}

// ============================================================================
// Host Lifecycle
// ============================================================================

NAH_CAPI NahHost* nah_host_create(const char* root_path) {
    clear_error();
    
    if (!root_path) {
        set_error(NAH_ERROR_INVALID_ARGUMENT, "root_path is NULL");
        return nullptr;
    }
    
    try {
        auto impl = nah::NahHost::create(root_path);
        return new NahHost(std::move(impl));
    } catch (const std::exception& e) {
        set_error(NAH_ERROR_INTERNAL, e.what());
        return nullptr;
    } catch (...) {
        set_error(NAH_ERROR_INTERNAL, "unknown error");
        return nullptr;
    }
}

NAH_CAPI void nah_host_destroy(NahHost* host) {
    delete host;
}

NAH_CAPI const char* nah_host_root(const NahHost* host) {
    if (!host) return "";
    return host->root.c_str();
}

// ============================================================================
// Application Listing
// ============================================================================

NAH_CAPI NahAppList* nah_host_list_apps(NahHost* host) {
    clear_error();
    
    if (!host) {
        set_error(NAH_ERROR_INVALID_ARGUMENT, "host is NULL");
        return nullptr;
    }
    
    try {
        auto list = new NahAppList();
        list->apps = host->impl->listApplications();
        return list;
    } catch (const std::exception& e) {
        set_error(NAH_ERROR_INTERNAL, e.what());
        return nullptr;
    } catch (...) {
        set_error(NAH_ERROR_INTERNAL, "unknown error");
        return nullptr;
    }
}

NAH_CAPI int32_t nah_app_list_count(const NahAppList* list) {
    if (!list) return 0;
    return static_cast<int32_t>(list->apps.size());
}

NAH_CAPI const char* nah_app_list_id(const NahAppList* list, int32_t index) {
    if (!list || index < 0 || static_cast<size_t>(index) >= list->apps.size()) {
        return nullptr;
    }
    return list->apps[static_cast<size_t>(index)].id.c_str();
}

NAH_CAPI const char* nah_app_list_version(const NahAppList* list, int32_t index) {
    if (!list || index < 0 || static_cast<size_t>(index) >= list->apps.size()) {
        return nullptr;
    }
    return list->apps[static_cast<size_t>(index)].version.c_str();
}

NAH_CAPI void nah_app_list_destroy(NahAppList* list) {
    delete list;
}

// ============================================================================
// Profile Management
// ============================================================================

NAH_CAPI NahStringList* nah_host_list_profiles(NahHost* host) {
    clear_error();
    
    if (!host) {
        set_error(NAH_ERROR_INVALID_ARGUMENT, "host is NULL");
        return nullptr;
    }
    
    try {
        auto list = new NahStringList();
        list->strings = host->impl->listProfiles();
        return list;
    } catch (const std::exception& e) {
        set_error(NAH_ERROR_INTERNAL, e.what());
        return nullptr;
    } catch (...) {
        set_error(NAH_ERROR_INTERNAL, "unknown error");
        return nullptr;
    }
}

NAH_CAPI char* nah_host_active_profile(NahHost* host) {
    clear_error();
    
    if (!host) {
        set_error(NAH_ERROR_INVALID_ARGUMENT, "host is NULL");
        return nullptr;
    }
    
    try {
        auto result = host->impl->getActiveHostProfile();
        if (result.isErr()) {
            // No active profile is not an error, just return NULL
            return nullptr;
        }
        // Profile doesn't have a name field, return "active" as placeholder
        // TODO: Track profile name in HostProfile or resolve from symlink
        return duplicate_string("default");
    } catch (const std::exception& e) {
        set_error(NAH_ERROR_INTERNAL, e.what());
        return nullptr;
    } catch (...) {
        set_error(NAH_ERROR_INTERNAL, "unknown error");
        return nullptr;
    }
}

NAH_CAPI NahStatus nah_host_set_profile(NahHost* host, const char* name) {
    clear_error();
    
    if (!host) {
        set_error(NAH_ERROR_INVALID_ARGUMENT, "host is NULL");
        return NAH_ERROR_INVALID_ARGUMENT;
    }
    if (!name) {
        set_error(NAH_ERROR_INVALID_ARGUMENT, "name is NULL");
        return NAH_ERROR_INVALID_ARGUMENT;
    }
    
    try {
        auto result = host->impl->setActiveHostProfile(name);
        if (result.isErr()) {
            auto code = map_error_code(result.error().code());
            set_error(code, result.error().message());
            return code;
        }
        return NAH_OK;
    } catch (const std::exception& e) {
        set_error(NAH_ERROR_INTERNAL, e.what());
        return NAH_ERROR_INTERNAL;
    } catch (...) {
        set_error(NAH_ERROR_INTERNAL, "unknown error");
        return NAH_ERROR_INTERNAL;
    }
}

// ============================================================================
// String List
// ============================================================================

NAH_CAPI int32_t nah_string_list_count(const NahStringList* list) {
    if (!list) return 0;
    return static_cast<int32_t>(list->strings.size());
}

NAH_CAPI const char* nah_string_list_get(const NahStringList* list, int32_t index) {
    if (!list || index < 0 || static_cast<size_t>(index) >= list->strings.size()) {
        return nullptr;
    }
    return list->strings[static_cast<size_t>(index)].c_str();
}

NAH_CAPI void nah_string_list_destroy(NahStringList* list) {
    delete list;
}

// ============================================================================
// Contract Composition
// ============================================================================

NAH_CAPI NahContract* nah_host_get_contract(
    NahHost* host,
    const char* app_id,
    const char* version,
    const char* profile
) {
    clear_error();
    
    if (!host) {
        set_error(NAH_ERROR_INVALID_ARGUMENT, "host is NULL");
        return nullptr;
    }
    if (!app_id) {
        set_error(NAH_ERROR_INVALID_ARGUMENT, "app_id is NULL");
        return nullptr;
    }
    
    try {
        auto result = host->impl->getLaunchContract(
            app_id,
            version ? version : "",
            profile ? profile : "",
            false  // enable_trace
        );
        
        if (result.isErr()) {
            auto code = map_error_code(result.error().code());
            set_error(code, result.error().message());
            return nullptr;
        }
        
        return new NahContract(std::move(result.value()));
    } catch (const std::exception& e) {
        set_error(NAH_ERROR_INTERNAL, e.what());
        return nullptr;
    } catch (...) {
        set_error(NAH_ERROR_INTERNAL, "unknown error");
        return nullptr;
    }
}

NAH_CAPI void nah_contract_destroy(NahContract* contract) {
    delete contract;
}

// ============================================================================
// Contract Accessors - Execution
// ============================================================================

NAH_CAPI const char* nah_contract_binary(const NahContract* contract) {
    if (!contract) return "";
    return contract->binary.c_str();
}

NAH_CAPI const char* nah_contract_cwd(const NahContract* contract) {
    if (!contract) return "";
    return contract->cwd.c_str();
}

NAH_CAPI int32_t nah_contract_argc(const NahContract* contract) {
    if (!contract) return 0;
    return static_cast<int32_t>(contract->arguments.size());
}

NAH_CAPI const char* nah_contract_argv(const NahContract* contract, int32_t index) {
    if (!contract || index < 0 || static_cast<size_t>(index) >= contract->arguments.size()) {
        return nullptr;
    }
    return contract->arguments[static_cast<size_t>(index)].c_str();
}

// ============================================================================
// Contract Accessors - Library Paths
// ============================================================================

NAH_CAPI const char* nah_contract_library_path_env_key(const NahContract* contract) {
    if (!contract) return "";
    return contract->library_path_env_key.c_str();
}

NAH_CAPI int32_t nah_contract_library_path_count(const NahContract* contract) {
    if (!contract) return 0;
    return static_cast<int32_t>(contract->library_paths.size());
}

NAH_CAPI const char* nah_contract_library_path(const NahContract* contract, int32_t index) {
    if (!contract || index < 0 || static_cast<size_t>(index) >= contract->library_paths.size()) {
        return nullptr;
    }
    return contract->library_paths[static_cast<size_t>(index)].c_str();
}

NAH_CAPI char* nah_contract_library_paths_joined(const NahContract* contract) {
    if (!contract) return duplicate_string("");
    
    std::string result;
    for (size_t i = 0; i < contract->library_paths.size(); ++i) {
        if (i > 0) {
#ifdef _WIN32
            result += ';';
#else
            result += ':';
#endif
        }
        result += contract->library_paths[i];
    }
    return duplicate_string(result);
}

// ============================================================================
// Contract Accessors - Environment
// ============================================================================

NAH_CAPI char* nah_contract_environment_json(const NahContract* contract) {
    if (!contract) return duplicate_string("{}");
    
    try {
        // Build JSON manually to avoid dependency on nlohmann_json in header
        std::string json = "{";
        bool first = true;
        for (const auto& [key, value] : contract->envelope.contract.environment) {
            if (!first) json += ",";
            first = false;
            
            // Escape key and value
            json += "\"";
            for (char c : key) {
                if (c == '"') json += "\\\"";
                else if (c == '\\') json += "\\\\";
                else if (c == '\n') json += "\\n";
                else if (c == '\r') json += "\\r";
                else if (c == '\t') json += "\\t";
                else json += c;
            }
            json += "\":\"";
            for (char c : value) {
                if (c == '"') json += "\\\"";
                else if (c == '\\') json += "\\\\";
                else if (c == '\n') json += "\\n";
                else if (c == '\r') json += "\\r";
                else if (c == '\t') json += "\\t";
                else json += c;
            }
            json += "\"";
        }
        json += "}";
        return duplicate_string(json);
    } catch (...) {
        return duplicate_string("{}");
    }
}

NAH_CAPI const char* nah_contract_environment_get(const NahContract* contract, const char* name) {
    if (!contract || !name) return nullptr;
    
    auto it = contract->envelope.contract.environment.find(name);
    if (it == contract->envelope.contract.environment.end()) {
        return nullptr;
    }
    return it->second.c_str();
}

// ============================================================================
// Contract Accessors - App/NAK Info
// ============================================================================

NAH_CAPI const char* nah_contract_app_id(const NahContract* contract) {
    if (!contract) return "";
    return contract->app_id.c_str();
}

NAH_CAPI const char* nah_contract_app_version(const NahContract* contract) {
    if (!contract) return "";
    return contract->app_version.c_str();
}

NAH_CAPI const char* nah_contract_app_root(const NahContract* contract) {
    if (!contract) return "";
    return contract->app_root.c_str();
}

NAH_CAPI const char* nah_contract_nak_id(const NahContract* contract) {
    if (!contract) return "";
    return contract->nak_id.c_str();
}

NAH_CAPI const char* nah_contract_nak_version(const NahContract* contract) {
    if (!contract) return "";
    return contract->nak_version.c_str();
}

NAH_CAPI const char* nah_contract_nak_root(const NahContract* contract) {
    if (!contract) return "";
    return contract->nak_root.c_str();
}

// ============================================================================
// Contract Accessors - Warnings
// ============================================================================

NAH_CAPI int32_t nah_contract_warning_count(const NahContract* contract) {
    if (!contract) return 0;
    return static_cast<int32_t>(contract->warning_keys.size());
}

NAH_CAPI const char* nah_contract_warning_key(const NahContract* contract, int32_t index) {
    if (!contract || index < 0 || static_cast<size_t>(index) >= contract->warning_keys.size()) {
        return nullptr;
    }
    return contract->warning_keys[static_cast<size_t>(index)].c_str();
}

NAH_CAPI char* nah_contract_warnings_json(const NahContract* contract) {
    if (!contract) return duplicate_string("[]");
    
    try {
        std::string json = "[";
        bool first = true;
        for (const auto& w : contract->envelope.warnings) {
            if (!first) json += ",";
            first = false;
            
            json += "{\"key\":\"" + w.key + "\",\"action\":\"" + w.action + "\"";
            for (const auto& [k, v] : w.fields) {
                json += ",\"" + k + "\":\"";
                for (char c : v) {
                    if (c == '"') json += "\\\"";
                    else if (c == '\\') json += "\\\\";
                    else json += c;
                }
                json += "\"";
            }
            json += "}";
        }
        json += "]";
        return duplicate_string(json);
    } catch (...) {
        return duplicate_string("[]");
    }
}

// ============================================================================
// Contract Serialization
// ============================================================================

NAH_CAPI char* nah_contract_to_json(const NahContract* contract) {
    if (!contract) return duplicate_string("{}");
    
    try {
        std::string json = nah::serialize_contract_json(contract->envelope, false);
        return duplicate_string(json);
    } catch (...) {
        return duplicate_string("{}");
    }
}

} // extern "C"
