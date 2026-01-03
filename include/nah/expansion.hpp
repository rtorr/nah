#pragma once

#include "nah/warnings.hpp"

#include <string>
#include <vector>
#include <unordered_map>

namespace nah {

// ============================================================================
// Placeholder Expansion (per SPEC L1017-L1042)
// ============================================================================

// Expansion limits per SPEC
constexpr size_t MAX_EXPANDED_SIZE = 64 * 1024;  // 64 KiB
constexpr size_t MAX_PLACEHOLDERS = 128;

struct ExpansionResult {
    bool ok = true;
    std::string value;
    std::string error_reason;  // "placeholder_limit" | "expansion_overflow"
};

struct ExpansionWithLimitsResult {
    std::string value;
    bool truncated = false;
    bool limit_exceeded = false;
};

// Expand placeholders in a string using the environment map
// Placeholders are exact tokens of the form {NAME}
// - Single-pass expansion (no recursion)
// - Missing placeholders emit warning and substitute empty string
// - Exceeding limits emits warning and substitutes empty string
ExpansionResult expand_placeholders(
    const std::string& input,
    const std::unordered_map<std::string, std::string>& environment,
    const std::string& source_path,
    WarningCollector& warnings);

// Simple overload for testing - returns expanded string and populates missing vars
std::string expand_placeholders(
    const std::string& input,
    const std::unordered_map<std::string, std::string>& environment,
    std::vector<std::string>& missing_vars);

// Expand with explicit limits (for testing)
ExpansionWithLimitsResult expand_placeholders_with_limits(
    const std::string& input,
    const std::unordered_map<std::string, std::string>& environment,
    std::vector<std::string>& missing_vars,
    size_t max_size,
    size_t max_placeholders);

// Expand all values in an environment map (in lexicographic key order)
void expand_environment_map(
    std::unordered_map<std::string, std::string>& environment,
    WarningCollector& warnings);

// Expand a vector of strings
std::vector<std::string> expand_string_vector(
    const std::vector<std::string>& input,
    const std::unordered_map<std::string, std::string>& environment,
    const std::string& source_path_prefix,
    WarningCollector& warnings);

// Simple overload for testing
std::vector<std::string> expand_vector(
    const std::vector<std::string>& input,
    const std::unordered_map<std::string, std::string>& environment,
    std::vector<std::string>& missing_vars);

} // namespace nah
