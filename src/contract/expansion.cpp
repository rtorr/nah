#include "nah/expansion.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace nah {

// Helper to get system environment variable
static std::string get_system_env(const std::string& name) {
    const char* val = std::getenv(name.c_str());
    return val ? std::string(val) : std::string();
}

ExpansionResult expand_placeholders(
    const std::string& input,
    const std::unordered_map<std::string, std::string>& environment,
    const std::string& source_path,
    WarningCollector& warnings) {
    
    ExpansionResult result;
    result.ok = true;
    
    std::string output;
    output.reserve(input.size());
    
    size_t placeholder_count = 0;
    size_t i = 0;
    
    // Helper lambda to look up and substitute a variable
    auto substitute_var = [&](const std::string& name) -> bool {
        placeholder_count++;
        
        // Check placeholder limit
        if (placeholder_count > MAX_PLACEHOLDERS) {
            warnings.emit(Warning::invalid_configuration,
                          nah::warnings::invalid_configuration("placeholder_limit", source_path));
            result.ok = false;
            result.value = "";
            result.error_reason = "placeholder_limit";
            return false;
        }
        
        // Look up the variable in NAH environment first
        auto it = environment.find(name);
        if (it != environment.end()) {
            output += it->second;
        } else {
            // Fall back to system environment
            std::string sys_val = get_system_env(name);
            if (!sys_val.empty()) {
                output += sys_val;
            } else {
                // Missing in both NAH and system - emit warning and substitute empty
                warnings.emit(Warning::missing_env_var,
                              nah::warnings::missing_env_var(name, source_path));
                // Substitute empty string
            }
        }
        return true;
    };
    
    while (i < input.size()) {
        // Check for ${NAME} or $NAME (shell-style) or {NAME} (NAH-style)
        if (input[i] == '$') {
            if (i + 1 < input.size() && input[i + 1] == '{') {
                // ${NAME} syntax
                size_t close = input.find('}', i + 2);
                if (close != std::string::npos) {
                    std::string name = input.substr(i + 2, close - i - 2);
                    if (!name.empty() && name.find('{') == std::string::npos) {
                        if (!substitute_var(name)) return result;
                        i = close + 1;
                        continue;
                    }
                }
                // Invalid syntax, copy literally
                output += input[i];
                ++i;
            } else if (i + 1 < input.size() && (std::isalpha(input[i + 1]) || input[i + 1] == '_')) {
                // $NAME syntax - read identifier (alphanumeric + underscore)
                size_t start = i + 1;
                size_t end = start;
                while (end < input.size() && (std::isalnum(input[end]) || input[end] == '_')) {
                    ++end;
                }
                std::string name = input.substr(start, end - start);
                if (!substitute_var(name)) return result;
                i = end;
            } else {
                // Lone $ or $followed by non-identifier, copy literally
                output += input[i];
                ++i;
            }
        } else if (input[i] == '{') {
            // {NAME} syntax (original NAH-style)
            size_t close = input.find('}', i + 1);
            if (close != std::string::npos) {
                std::string name = input.substr(i + 1, close - i - 1);
                
                // Check for nested braces (not allowed)
                if (name.find('{') != std::string::npos) {
                    // Not a valid placeholder, copy literally
                    output += input[i];
                    ++i;
                    continue;
                }
                
                if (!substitute_var(name)) return result;
                i = close + 1;
            } else {
                // No closing brace, copy literally
                output += input[i];
                ++i;
            }
        } else {
            output += input[i];
            ++i;
        }
        
        // Check size limit
        if (output.size() > MAX_EXPANDED_SIZE) {
            warnings.emit(Warning::invalid_configuration,
                          nah::warnings::invalid_configuration("expansion_overflow", source_path));
            result.ok = false;
            result.value = "";
            result.error_reason = "expansion_overflow";
            return result;
        }
    }
    
    result.value = std::move(output);
    return result;
}

void expand_environment_map(
    std::unordered_map<std::string, std::string>& environment,
    WarningCollector& warnings) {
    
    // Get keys in lexicographic order (per SPEC deterministic ordering)
    std::vector<std::string> keys;
    keys.reserve(environment.size());
    for (const auto& [key, _] : environment) {
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    
    // Take a snapshot of the environment for expansion (per SPEC single-pass)
    auto snapshot = environment;
    
    // Expand each value in order
    for (const auto& key : keys) {
        std::string source_path = "environment." + key;
        auto result = expand_placeholders(environment[key], snapshot, source_path, warnings);
        if (result.ok) {
            environment[key] = result.value;
        } else {
            environment[key] = "";
        }
    }
}

std::vector<std::string> expand_string_vector(
    const std::vector<std::string>& input,
    const std::unordered_map<std::string, std::string>& environment,
    const std::string& source_path_prefix,
    WarningCollector& warnings) {
    
    std::vector<std::string> result;
    result.reserve(input.size());
    
    for (size_t i = 0; i < input.size(); ++i) {
        std::string source_path = source_path_prefix + "[" + std::to_string(i) + "]";
        auto expanded = expand_placeholders(input[i], environment, source_path, warnings);
        if (expanded.ok) {
            result.push_back(expanded.value);
        } else {
            result.push_back("");
        }
    }
    
    return result;
}

// ============================================================================
// Simple overloads for testing
// ============================================================================

std::string expand_placeholders(
    const std::string& input,
    const std::unordered_map<std::string, std::string>& environment,
    std::vector<std::string>& missing_vars) {
    
    std::string output;
    output.reserve(input.size());
    
    // Helper lambda to look up and substitute a variable
    auto substitute_var = [&](const std::string& name, const std::string& original) {
        auto it = environment.find(name);
        if (it != environment.end()) {
            output += it->second;
        } else {
            // Fall back to system environment
            std::string sys_val = get_system_env(name);
            if (!sys_val.empty()) {
                output += sys_val;
            } else {
                missing_vars.push_back(name);
                output += original;  // Keep original placeholder
            }
        }
    };
    
    size_t i = 0;
    
    while (i < input.size()) {
        // Check for ${NAME} or $NAME (shell-style) or {NAME} (NAH-style)
        if (input[i] == '$') {
            if (i + 1 < input.size() && input[i + 1] == '{') {
                // ${NAME} syntax
                size_t close = input.find('}', i + 2);
                if (close != std::string::npos) {
                    std::string name = input.substr(i + 2, close - i - 2);
                    if (!name.empty() && name.find('{') == std::string::npos) {
                        substitute_var(name, input.substr(i, close - i + 1));
                        i = close + 1;
                        continue;
                    }
                }
                // Invalid syntax, copy literally
                output += input[i];
                ++i;
            } else if (i + 1 < input.size() && (std::isalpha(input[i + 1]) || input[i + 1] == '_')) {
                // $NAME syntax - read identifier (alphanumeric + underscore)
                size_t start = i + 1;
                size_t end = start;
                while (end < input.size() && (std::isalnum(input[end]) || input[end] == '_')) {
                    ++end;
                }
                std::string name = input.substr(start, end - start);
                substitute_var(name, input.substr(i, end - i));
                i = end;
            } else {
                // Lone $ or $followed by non-identifier, copy literally
                output += input[i];
                ++i;
            }
        } else if (input[i] == '{') {
            // {NAME} syntax (original NAH-style)
            size_t close = input.find('}', i + 1);
            if (close != std::string::npos) {
                std::string name = input.substr(i + 1, close - i - 1);
                
                if (name.find('{') != std::string::npos || name.empty()) {
                    output += input[i];
                    ++i;
                    continue;
                }
                
                substitute_var(name, input.substr(i, close - i + 1));
                i = close + 1;
            } else {
                output += input[i];
                ++i;
            }
        } else {
            output += input[i];
            ++i;
        }
    }
    
    return output;
}

ExpansionWithLimitsResult expand_placeholders_with_limits(
    const std::string& input,
    const std::unordered_map<std::string, std::string>& environment,
    std::vector<std::string>& missing_vars,
    size_t max_size,
    size_t max_placeholders) {
    
    ExpansionWithLimitsResult result;
    std::string output;
    output.reserve(input.size());
    
    size_t placeholder_count = 0;
    size_t i = 0;
    
    while (i < input.size()) {
        if (input[i] == '{') {
            size_t close = input.find('}', i + 1);
            if (close != std::string::npos) {
                std::string name = input.substr(i + 1, close - i - 1);
                
                if (name.find('{') != std::string::npos || name.empty()) {
                    output += input[i];
                    ++i;
                    continue;
                }
                
                placeholder_count++;
                
                if (placeholder_count > max_placeholders) {
                    result.limit_exceeded = true;
                    result.value = output;
                    return result;
                }
                
                auto it = environment.find(name);
                if (it != environment.end()) {
                    output += it->second;
                } else {
                    missing_vars.push_back(name);
                    output += input.substr(i, close - i + 1);
                }
                
                i = close + 1;
            } else {
                output += input[i];
                ++i;
            }
        } else {
            output += input[i];
            ++i;
        }
        
        if (output.size() > max_size) {
            result.truncated = true;
            result.value = output.substr(0, max_size);
            return result;
        }
    }
    
    result.value = output;
    return result;
}

std::vector<std::string> expand_vector(
    const std::vector<std::string>& input,
    const std::unordered_map<std::string, std::string>& environment,
    std::vector<std::string>& missing_vars) {
    
    std::vector<std::string> result;
    result.reserve(input.size());
    
    for (const auto& s : input) {
        result.push_back(expand_placeholders(s, environment, missing_vars));
    }
    
    return result;
}

} // namespace nah
