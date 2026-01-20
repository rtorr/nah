/*
 * NAH Overrides - Override Parsing and Application
 * 
 * This file provides helpers for parsing and applying NAH_OVERRIDE_* 
 * environment variables. It uses nlohmann/json for JSON parsing.
 * 
 * Include this header if you want automatic override handling.
 * Otherwise, implement override parsing yourself and modify the
 * CompositionResult.contract.environment after calling nah_compose().
 * 
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NAH_OVERRIDES_H
#define NAH_OVERRIDES_H

#ifdef __cplusplus

#include "nah_core.h"
#include <nlohmann/json.hpp>
#include <cstdlib>

namespace nah {
namespace overrides {

using json = nlohmann::json;
using namespace nah::core;

// ============================================================================
// OVERRIDE PARSING
// ============================================================================

/**
 * Result of parsing NAH_OVERRIDE_ENVIRONMENT.
 */
struct EnvOverrideParseResult {
    bool present = false;       ///< Was NAH_OVERRIDE_ENVIRONMENT set?
    bool ok = false;            ///< Did parsing succeed?
    std::string error;          ///< Error message if parsing failed
    std::unordered_map<std::string, std::string> values;  ///< Parsed values
};

/**
 * Parse NAH_OVERRIDE_ENVIRONMENT from process environment.
 * 
 * @return Parse result with values or error
 */
inline EnvOverrideParseResult parse_env_override() {
    EnvOverrideParseResult result;
    
    const char* env_val = std::getenv("NAH_OVERRIDE_ENVIRONMENT");
    if (!env_val) {
        return result;  // Not present
    }
    
    result.present = true;
    
    // Validate JSON first (no exceptions)
    if (!json::accept(env_val)) {
        result.error = "invalid JSON";
        return result;
    }
    
    // Parse with noexcept overload
    auto j = json::parse(env_val, nullptr, false);
    
    if (j.is_discarded()) {
        result.error = "JSON parse failed";
        return result;
    }
    
    if (!j.is_object()) {
        result.error = "expected object";
        return result;
    }
    
    for (auto& [key, val] : j.items()) {
        if (!val.is_string()) {
            result.error = "value for '" + key + "' must be string";
            return result;
        }
        result.values[key] = val.get<std::string>();
    }
    
    result.ok = true;
    return result;
}

/**
 * Parse NAH_OVERRIDE_ENVIRONMENT from a provided map (e.g., from N-API).
 * 
 * @param process_env Map containing environment variables
 * @return Parse result with values or error
 */
inline EnvOverrideParseResult parse_env_override(
    const std::unordered_map<std::string, std::string>& process_env)
{
    EnvOverrideParseResult result;
    
    auto it = process_env.find("NAH_OVERRIDE_ENVIRONMENT");
    if (it == process_env.end()) {
        return result;  // Not present
    }
    
    result.present = true;
    
    const std::string& env_val = it->second;
    
    // Validate JSON first (no exceptions)
    if (!json::accept(env_val)) {
        result.error = "invalid JSON";
        return result;
    }
    
    // Parse with noexcept overload
    auto j = json::parse(env_val, nullptr, false);
    
    if (j.is_discarded()) {
        result.error = "JSON parse failed";
        return result;
    }
    
    if (!j.is_object()) {
        result.error = "expected object";
        return result;
    }
    
    for (auto& [key, val] : j.items()) {
        if (!val.is_string()) {
            result.error = "value for '" + key + "' must be string";
            return result;
        }
        result.values[key] = val.get<std::string>();
    }
    
    result.ok = true;
    return result;
}

// ============================================================================
// OVERRIDE POLICY HELPERS
// ============================================================================

/**
 * Check if a key is allowed by the override policy.
 */
inline bool is_key_allowed(const std::string& key, const HostEnvironment& host_env) {
    if (!host_env.overrides.allow_env_overrides) {
        return false;
    }
    if (host_env.overrides.allowed_env_keys.empty()) {
        return true;  // No allowlist = all keys allowed
    }
    for (const auto& allowed : host_env.overrides.allowed_env_keys) {
        if (allowed == key) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// OVERRIDE APPLICATION
// ============================================================================

/**
 * Apply environment overrides to a composition result.
 * 
 * This function:
 * 1. Parses NAH_OVERRIDE_ENVIRONMENT from process_env
 * 2. Checks if permitted by host environment policy
 * 3. Merges into contract.environment if permitted
 * 4. Emits warnings on parse errors or denied overrides
 * 
 * Call this after nah_compose() to apply overrides.
 * 
 * @param result Composition result to modify
 * @param host_env Host environment with override policy
 * @param process_env Process environment map
 */
inline void apply_overrides(
    CompositionResult& result,
    const HostEnvironment& host_env,
    const std::unordered_map<std::string, std::string>& process_env)
{
    if (!result.ok) {
        return;  // Don't apply overrides to failed compositions
    }
    
    auto parsed = parse_env_override(process_env);
    
    if (!parsed.present) {
        return;  // No override to apply
    }
    
    if (!parsed.ok) {
        // Parse failure
        result.warnings.push_back({
            warning_to_string(Warning::override_invalid),
            "warn",
            {
                {"target", "NAH_OVERRIDE_ENVIRONMENT"},
                {"reason", "parse_failure"},
                {"source_kind", trace_source::PROCESS_ENV},
                {"source_ref", "NAH_OVERRIDE_ENVIRONMENT"}
            }
        });
        return;
    }
    
    // Check global override permission
    if (!host_env.overrides.allow_env_overrides) {
        result.warnings.push_back({
            warning_to_string(Warning::override_denied),
            "warn",
            {
                {"target", "NAH_OVERRIDE_ENVIRONMENT"},
                {"reason", "overrides_disabled"},
                {"source_kind", trace_source::PROCESS_ENV},
                {"source_ref", "NAH_OVERRIDE_ENVIRONMENT"}
            }
        });
        return;
    }
    
    // Apply overrides, checking per-key permissions
    for (const auto& [key, value] : parsed.values) {
        if (is_key_allowed(key, host_env)) {
            result.contract.environment[key] = value;
        } else {
            result.warnings.push_back({
                warning_to_string(Warning::override_denied),
                "warn",
                {
                    {"target", key},
                    {"reason", "key_not_allowed"},
                    {"source_kind", trace_source::PROCESS_ENV},
                    {"source_ref", "NAH_OVERRIDE_ENVIRONMENT"}
                }
            });
        }
    }
}

/**
 * Apply environment overrides using actual process environment.
 * 
 * Convenience overload that reads from std::getenv().
 * 
 * @param result Composition result to modify
 * @param host_env Host environment with override policy
 */
inline void apply_overrides(CompositionResult& result, const HostEnvironment& host_env)
{
    if (!result.ok) {
        return;
    }
    
    auto parsed = parse_env_override();
    
    if (!parsed.present) {
        return;
    }
    
    if (!parsed.ok) {
        result.warnings.push_back({
            warning_to_string(Warning::override_invalid),
            "warn",
            {
                {"target", "NAH_OVERRIDE_ENVIRONMENT"},
                {"reason", "parse_failure"},
                {"source_kind", trace_source::PROCESS_ENV},
                {"source_ref", "NAH_OVERRIDE_ENVIRONMENT"}
            }
        });
        return;
    }
    
    if (!host_env.overrides.allow_env_overrides) {
        result.warnings.push_back({
            warning_to_string(Warning::override_denied),
            "warn",
            {
                {"target", "NAH_OVERRIDE_ENVIRONMENT"},
                {"reason", "overrides_disabled"},
                {"source_kind", trace_source::PROCESS_ENV},
                {"source_ref", "NAH_OVERRIDE_ENVIRONMENT"}
            }
        });
        return;
    }
    
    for (const auto& [key, value] : parsed.values) {
        if (is_key_allowed(key, host_env)) {
            result.contract.environment[key] = value;
        } else {
            result.warnings.push_back({
                warning_to_string(Warning::override_denied),
                "warn",
                {
                    {"target", key},
                    {"reason", "key_not_allowed"},
                    {"source_kind", trace_source::PROCESS_ENV},
                    {"source_ref", "NAH_OVERRIDE_ENVIRONMENT"}
                }
            });
        }
    }
}

} // namespace overrides
} // namespace nah

#endif // __cplusplus

#endif // NAH_OVERRIDES_H
