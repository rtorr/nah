#include "nah/contract.hpp"
#include "nah/expansion.hpp"
#include "nah/capabilities.hpp"
#include "nah/nak_selection.hpp"
#include "nah/path_utils.hpp"
#include "nah/platform.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

#include <nlohmann/json.hpp>

namespace nah {

namespace fs = std::filesystem;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool file_exists(const std::string& path) {
    return fs::exists(path) && fs::is_regular_file(path);
}

// Normalize RFC3339 timestamp to comparable form
// Handles both 'Z' suffix and '+00:00'/'-00:00' offsets
std::string normalize_rfc3339(const std::string& ts) {
    if (ts.empty()) return ts;
    
    std::string result = ts;
    
    // Replace +00:00 or -00:00 with Z for UTC
    if (result.size() >= 6) {
        std::string suffix = result.substr(result.size() - 6);
        if (suffix == "+00:00" || suffix == "-00:00") {
            result = result.substr(0, result.size() - 6) + "Z";
        }
    }
    
    return result;
}

// Compare RFC3339 timestamps
// Returns true if timestamp 'a' is before timestamp 'b'
bool timestamp_before(const std::string& a, const std::string& b) {
    // Normalize to handle +00:00 vs Z variations
    std::string norm_a = normalize_rfc3339(a);
    std::string norm_b = normalize_rfc3339(b);
    
    // Lexicographic comparison works for normalized RFC3339 UTC timestamps
    // Format: YYYY-MM-DDTHH:MM:SSZ (fixed width, sortable)
    return norm_a < norm_b;
}

// Apply an environment operation to the current environment
// Returns the new value for the key (or nullopt if unset)
std::optional<std::string> apply_env_op(
    const std::string& key,
    const EnvValue& env_val,
    const std::unordered_map<std::string, std::string>& current_env) {
    
    switch (env_val.op) {
        case EnvOp::Set:
            return env_val.value;
            
        case EnvOp::Prepend: {
            auto it = current_env.find(key);
            if (it != current_env.end() && !it->second.empty()) {
                return env_val.value + env_val.separator + it->second;
            }
            return env_val.value;
        }
        
        case EnvOp::Append: {
            auto it = current_env.find(key);
            if (it != current_env.end() && !it->second.empty()) {
                return it->second + env_val.separator + env_val.value;
            }
            return env_val.value;
        }
        
        case EnvOp::Unset:
            return std::nullopt;
    }
    
    return env_val.value;
}

} // namespace

std::string get_library_path_env_key() {
#if defined(__APPLE__)
    return "DYLD_LIBRARY_PATH";
#elif defined(_WIN32)
    return "PATH";
#else
    return "LD_LIBRARY_PATH";
#endif
}

char get_path_separator() {
#ifdef _WIN32
    return ';';
#else
    return ':';
#endif
}

OverridesParseResult parse_overrides_file(const std::string& content, const std::string& /*path*/) {
    OverridesParseResult result;
    
    // Parse JSON overrides file
    try {
        auto json = nlohmann::json::parse(content);
        
        if (!json.is_object()) {
            result.error = "invalid_shape";
            return result;
        }
        
        for (auto& [key, val] : json.items()) {
            if (key == "environment") {
                if (!val.is_object()) {
                    result.error = "invalid_shape";
                    return result;
                }
                for (auto& [k, v] : val.items()) {
                    if (!v.is_string()) {
                        result.error = "invalid_shape";
                        return result;
                    }
                    result.overrides.environment[k] = v.get<std::string>();
                }
            } else if (key == "warnings") {
                if (!val.is_object()) {
                    result.error = "invalid_shape";
                    return result;
                }
                for (auto& [k, v] : val.items()) {
                    if (!v.is_string()) {
                        result.error = "invalid_shape";
                        return result;
                    }
                    result.overrides.warnings[k] = v.get<std::string>();
                }
            } else {
                result.error = "invalid_shape";
                return result;
            }
        }
        
        result.ok = true;
        return result;
    } catch (const nlohmann::json::exception&) {
        result.error = "parse_failure";
        return result;
    }
}

CompositionResult compose_contract(const CompositionInputs& inputs) {
    CompositionResult result;
    WarningCollector warnings(&inputs.profile);
    
    const auto& manifest = inputs.manifest;
    const auto& install_record = inputs.install_record;
    const auto& profile = inputs.profile;
    
    // =========================================================================
    // Step 1: Validate inputs (per SPEC L879-L930)
    // =========================================================================
    
    // App Install Record is already validated in inputs
    // Check for app field mismatches
    if (!install_record.app.id.empty() && install_record.app.id != manifest.id) {
        warnings.emit(Warning::invalid_configuration,
                      nah::warnings::invalid_configuration("app_field_mismatch", 
                                                           "install_record.app", "id"));
    }
    if (!install_record.app.version.empty() && install_record.app.version != manifest.version) {
        warnings.emit(Warning::invalid_configuration,
                      nah::warnings::invalid_configuration("app_field_mismatch",
                                                           "install_record.app", "version"));
    }
    
    // Load pinned NAK (skip for standalone apps with no nak_id)
    bool nak_resolved = false;
    NakInstallRecord nak_record;
    NakPin pin;  // Declared outside for use in contract.nak.record_ref
    
    if (!manifest.nak_id.empty()) {
        pin.id = install_record.nak.id;
        pin.version = install_record.nak.version;
        pin.record_ref = install_record.nak.record_ref;
        
        if (pin.record_ref.empty() || pin.id.empty() || pin.version.empty()) {
            warnings.emit(Warning::nak_pin_invalid, {{"reason", "pin_fields_missing"}});
        } else {
            auto load_result = load_pinned_nak(pin, manifest, profile, inputs.nah_root, warnings);
            if (load_result.loaded) {
                nak_resolved = true;
                nak_record = load_result.nak_record;
            }
        }
    }
    // Standalone apps (empty nak_id) skip NAK resolution entirely
    
    // =========================================================================
    // Step 2: Derive app fields (per SPEC L932-L948)
    // =========================================================================
    
    LaunchContract& contract = result.envelope.contract;
    
    contract.app.id = manifest.id;
    contract.app.version = manifest.version;
    contract.app.root = install_record.paths.install_root;
    
    // Validate entrypoint
    if (manifest.entrypoint_path.empty()) {
        result.critical_error = CriticalError::ENTRYPOINT_NOT_FOUND;
        warnings.emit(Warning::invalid_manifest, {{"reason", "entrypoint_missing"}});
        result.envelope.warnings = warnings.get_warnings();
        return result;
    }
    
    if (is_absolute_path(manifest.entrypoint_path)) {
        result.critical_error = CriticalError::ENTRYPOINT_NOT_FOUND;
        warnings.emit(Warning::invalid_manifest, {{"reason", "entrypoint_absolute"}});
        result.envelope.warnings = warnings.get_warnings();
        return result;
    }
    
    // Resolve entrypoint under app root
    auto entry_result = normalize_under_root(contract.app.root, manifest.entrypoint_path, false);
    if (!entry_result.ok) {
        result.critical_error = CriticalError::PATH_TRAVERSAL;
        result.critical_error_context = "entrypoint '" + manifest.entrypoint_path + 
            "' escapes app root '" + contract.app.root + "'";
        result.envelope.warnings = warnings.get_warnings();
        return result;
    }
    
    contract.app.entrypoint = entry_result.path;
    
    // Check entrypoint exists
    if (!file_exists(contract.app.entrypoint)) {
        result.critical_error = CriticalError::ENTRYPOINT_NOT_FOUND;
        result.envelope.warnings = warnings.get_warnings();
        return result;
    }
    
    // =========================================================================
    // Step 3: Validate NAK requirement (per SPEC L950-L956)
    // =========================================================================
    
    // Only validate nak_version_req if nak_id is specified (non-standalone apps)
    if (!manifest.nak_id.empty() && !manifest.nak_version_req.has_value()) {
        warnings.emit(Warning::invalid_manifest, {{"reason", "nak_version_req_invalid"}});
    }
    
    // =========================================================================
    // Step 4: Derive NAK fields (per SPEC L958-L976)
    // =========================================================================
    
    if (nak_resolved) {
        contract.nak.id = nak_record.nak.id;
        contract.nak.version = nak_record.nak.version;
        contract.nak.root = nak_record.paths.root;
        contract.nak.resource_root = nak_record.paths.resource_root;
        contract.nak.record_ref = pin.record_ref;
        
        // Validate NAK paths - lib_dirs must be absolute and under paths.root
        // Paths are already normalized to forward slashes by config parsers
        for (const auto& lib_dir : nak_record.paths.lib_dirs) {
            if (!is_absolute_path(lib_dir)) {
                result.critical_error = CriticalError::PATH_TRAVERSAL;
                result.critical_error_context = "NAK lib_dir '" + lib_dir + 
                    "' is relative; SPEC requires absolute paths in NAK install records";
                result.envelope.warnings = warnings.get_warnings();
                return result;
            }
            if (!is_path_under_root(nak_record.paths.root, lib_dir)) {
                result.critical_error = CriticalError::PATH_TRAVERSAL;
                result.critical_error_context = "NAK lib_dir '" + lib_dir + 
                    "' escapes NAK root '" + nak_record.paths.root + "'";
                result.envelope.warnings = warnings.get_warnings();
                return result;
            }
        }
        
        // Validate all loader exec_paths are under NAK root
        for (const auto& [loader_name, loader_config] : nak_record.loaders) {
            if (!loader_config.exec_path.empty()) {
                if (!is_absolute_path(loader_config.exec_path)) {
                    result.critical_error = CriticalError::PATH_TRAVERSAL;
                    result.critical_error_context = "NAK loader '" + loader_name + 
                        "' exec_path '" + loader_config.exec_path + 
                        "' is relative; SPEC requires absolute paths";
                    result.envelope.warnings = warnings.get_warnings();
                    return result;
                }
                if (!is_path_under_root(nak_record.paths.root, loader_config.exec_path)) {
                    result.critical_error = CriticalError::PATH_TRAVERSAL;
                    result.critical_error_context = "NAK loader '" + loader_name + 
                        "' exec_path '" + loader_config.exec_path + 
                        "' escapes NAK root '" + nak_record.paths.root + "'";
                    result.envelope.warnings = warnings.get_warnings();
                    return result;
                }
            }
        }
    }
    
    // =========================================================================
    // Step 5 & 6: Build effective_environment (per SPEC L978-L1016)
    // Environment algebra: set, prepend, append, unset
    // =========================================================================
    
    std::unordered_map<std::string, std::string> effective_env;
    
    // Track history for trace output
    std::unordered_map<std::string, std::vector<TraceContribution>> env_history;
    
    // Helper to record a contribution
    auto record_contribution = [&](const std::string& key, const std::string& value,
                                   const std::string& source_kind, const std::string& source_path,
                                   int rank, EnvOp op, bool accepted) {
        if (inputs.enable_trace) {
            TraceContribution contrib;
            contrib.value = value;
            contrib.source_kind = source_kind;
            contrib.source_path = source_path;
            contrib.precedence_rank = rank;
            contrib.operation = op;
            contrib.accepted = accepted;
            env_history[key].push_back(contrib);
        }
    };
    
    // Layer 1: Host Profile (apply operations)
    for (const auto& [key, env_val] : profile.environment) {
        auto op_result = apply_env_op(key, env_val, effective_env);
        if (op_result.has_value()) {
            effective_env[key] = op_result.value();
            record_contribution(key, op_result.value(), "profile", "host_profile", 1, env_val.op, true);
        } else {
            effective_env.erase(key);
            record_contribution(key, "", "profile", "host_profile", 1, env_val.op, true);
        }
    }
    
    // Layer 2: NAK Install Record (apply operations)
    if (nak_resolved) {
        for (const auto& [key, env_val] : nak_record.environment) {
            auto op_result = apply_env_op(key, env_val, effective_env);
            if (op_result.has_value()) {
                effective_env[key] = op_result.value();
                record_contribution(key, op_result.value(), "nak", install_record.nak.record_ref, 2, env_val.op, true);
            } else {
                effective_env.erase(key);
                record_contribution(key, "", "nak", install_record.nak.record_ref, 2, env_val.op, true);
            }
        }
    }
    
    // Layer 3: App Manifest defaults (string values, fill-only)
    for (const auto& env_var : manifest.env_vars) {
        auto eq = env_var.find('=');
        if (eq != std::string::npos) {
            std::string key = env_var.substr(0, eq);
            std::string val = env_var.substr(eq + 1);
            bool accepted = (effective_env.find(key) == effective_env.end());
            if (accepted) {
                effective_env[key] = val;
            }
            record_contribution(key, val, "manifest", "app_manifest", 3, EnvOp::Set, accepted);
        }
    }
    
    // Layer 4: App Install Record overrides (apply operations)
    for (const auto& [key, env_val] : install_record.overrides.environment) {
        auto op_result = apply_env_op(key, env_val, effective_env);
        if (op_result.has_value()) {
            effective_env[key] = op_result.value();
            record_contribution(key, op_result.value(), "install_override", install_record.source_path, 4, env_val.op, true);
        } else {
            effective_env.erase(key);
            record_contribution(key, "", "install_override", install_record.source_path, 4, env_val.op, true);
        }
    }
    
    // Layer 5: NAH standard variables (overwrite)
    effective_env["NAH_APP_ID"] = contract.app.id;
    record_contribution("NAH_APP_ID", contract.app.id, "nah_standard", "nah", 5, EnvOp::Set, true);
    effective_env["NAH_APP_VERSION"] = contract.app.version;
    record_contribution("NAH_APP_VERSION", contract.app.version, "nah_standard", "nah", 5, EnvOp::Set, true);
    effective_env["NAH_APP_ROOT"] = contract.app.root;
    record_contribution("NAH_APP_ROOT", contract.app.root, "nah_standard", "nah", 5, EnvOp::Set, true);
    effective_env["NAH_APP_ENTRY"] = contract.app.entrypoint;
    record_contribution("NAH_APP_ENTRY", contract.app.entrypoint, "nah_standard", "nah", 5, EnvOp::Set, true);
    
    if (nak_resolved) {
        effective_env["NAH_NAK_ID"] = contract.nak.id;
        record_contribution("NAH_NAK_ID", contract.nak.id, "nah_standard", "nah", 5, EnvOp::Set, true);
        effective_env["NAH_NAK_VERSION"] = contract.nak.version;
        record_contribution("NAH_NAK_VERSION", contract.nak.version, "nah_standard", "nah", 5, EnvOp::Set, true);
        effective_env["NAH_NAK_ROOT"] = contract.nak.root;
        record_contribution("NAH_NAK_ROOT", contract.nak.root, "nah_standard", "nah", 5, EnvOp::Set, true);
    }
    
    // Layer 6 & 7: Process env and file overrides
    // First, collect all NAH_OVERRIDE_* from process env
    std::vector<std::pair<std::string, std::string>> env_overrides;
    for (const auto& [key, val] : inputs.process_env) {
        if (key.rfind("NAH_OVERRIDE_", 0) == 0) {
            env_overrides.push_back({key, val});
        }
    }
    // Sort lexicographically
    std::sort(env_overrides.begin(), env_overrides.end());
    
    // Apply env overrides
    for (const auto& [key, val] : env_overrides) {
        std::string target = key.substr(13);  // Strip "NAH_OVERRIDE_"
        
        if (target == "ENVIRONMENT") {
            if (!is_override_permitted(target, profile)) {
                warnings.emit(Warning::override_denied,
                              nah::warnings::override_denied(key, "process_env", key));
                continue;
            }
            
            // Parse JSON
            try {
                auto json = nlohmann::json::parse(val);
                if (!json.is_object()) {
                    warnings.emit(Warning::override_invalid,
                                  nah::warnings::override_invalid(key, "parse_failure", "process_env", key));
                    continue;
                }
                for (auto& [k, v] : json.items()) {
                    if (v.is_string()) {
                        effective_env[k] = v.get<std::string>();
                    }
                }
            } catch (const nlohmann::json::exception&) {
                warnings.emit(Warning::override_invalid,
                              nah::warnings::override_invalid(key, "parse_failure", "process_env", key));
            }
        } else if (target.rfind("WARNINGS_", 0) == 0) {
            std::string warning_key = target.substr(9);  // Strip "WARNINGS_"
            
            if (!is_override_permitted(target, profile)) {
                warnings.emit(Warning::override_denied,
                              nah::warnings::override_denied(key, "process_env", key));
                continue;
            }
            
            auto action = parse_warning_action(val);
            if (!action) {
                warnings.emit(Warning::override_invalid,
                              nah::warnings::override_invalid(key, "invalid_value", "process_env", key));
                continue;
            }
            
            auto warn = parse_warning_key(warning_key);
            if (!warn) {
                warnings.emit(Warning::override_invalid,
                              nah::warnings::override_invalid(key, "unknown_warning_key", "process_env", key));
                continue;
            }
            
            warnings.apply_override(warning_key, *action);
        } else {
            // Unknown override target
            warnings.emit(Warning::override_denied,
                          nah::warnings::override_denied(key, "process_env", key));
        }
    }
    
    // Apply file overrides
    if (inputs.overrides_file_path.has_value()) {
        std::string content = read_file(*inputs.overrides_file_path);
        if (content.empty()) {
            warnings.emit(Warning::override_invalid,
                          nah::warnings::override_invalid("OVERRIDES_FILE", "parse_failure",
                                                          "overrides_file", *inputs.overrides_file_path));
        } else {
            auto parse_result = parse_overrides_file(content, *inputs.overrides_file_path);
            if (!parse_result.ok) {
                warnings.emit(Warning::override_invalid,
                              nah::warnings::override_invalid("OVERRIDES_FILE", parse_result.error,
                                                              "overrides_file", *inputs.overrides_file_path));
            } else {
                // Apply environment overrides
                if (!parse_result.overrides.environment.empty()) {
                    std::string target = "ENVIRONMENT";
                    if (!is_override_permitted(target, profile)) {
                        warnings.emit(Warning::override_denied,
                                      nah::warnings::override_denied("NAH_OVERRIDE_ENVIRONMENT",
                                                                     "overrides_file",
                                                                     *inputs.overrides_file_path + ":environment"));
                    } else {
                        for (const auto& [k, v] : parse_result.overrides.environment) {
                            effective_env[k] = v;
                        }
                    }
                }
                
                // Apply warnings overrides (in lexicographic order)
                std::vector<std::string> warn_keys;
                for (const auto& [k, _] : parse_result.overrides.warnings) {
                    warn_keys.push_back(k);
                }
                std::sort(warn_keys.begin(), warn_keys.end());
                
                for (const auto& warning_key : warn_keys) {
                    std::string target = "WARNINGS_" + warning_key;
                    std::string source_ref = *inputs.overrides_file_path + ":warnings." + warning_key;
                    
                    if (!is_override_permitted(target, profile)) {
                        warnings.emit(Warning::override_denied,
                                      nah::warnings::override_denied("NAH_OVERRIDE_WARNINGS_" + warning_key,
                                                                     "overrides_file", source_ref));
                        continue;
                    }
                    
                    auto action = parse_warning_action(parse_result.overrides.warnings[warning_key]);
                    if (!action) {
                        warnings.emit(Warning::override_invalid,
                                      nah::warnings::override_invalid("NAH_OVERRIDE_WARNINGS_" + warning_key,
                                                                      "invalid_value", "overrides_file", source_ref));
                        continue;
                    }
                    
                    auto warn = parse_warning_key(warning_key);
                    if (!warn) {
                        warnings.emit(Warning::override_invalid,
                                      nah::warnings::override_invalid("NAH_OVERRIDE_WARNINGS_" + warning_key,
                                                                      "unknown_warning_key", "overrides_file", source_ref));
                        continue;
                    }
                    
                    warnings.apply_override(warning_key, *action);
                }
            }
        }
    }
    
    // =========================================================================
    // Step 8: Placeholder expansion (per SPEC L1017-L1042)
    // =========================================================================
    
    expand_environment_map(effective_env, warnings);
    
    // Expand NAK loader templates if resolved and loader selected
    std::vector<std::string> expanded_args_template;
    std::string expanded_cwd;
    const LoaderConfig* selected_loader = nullptr;
    
    if (nak_resolved && nak_record.has_loaders()) {
        // Determine which loader to use: explicit pin, "default", or single-loader auto-select
        std::string loader_name = install_record.nak.loader;
        if (loader_name.empty()) {
            if (nak_record.loaders.count("default")) {
                loader_name = "default";
            } else if (nak_record.loaders.size() == 1) {
                loader_name = nak_record.loaders.begin()->first;
            }
        }
        
        if (!loader_name.empty() && nak_record.loaders.count(loader_name)) {
            selected_loader = &nak_record.loaders.at(loader_name);
            expanded_args_template = expand_string_vector(
                selected_loader->args_template,
                effective_env,
                "nak_record.loaders." + loader_name + ".args_template",
                warnings);
        }
        
        if (nak_record.execution.present && !nak_record.execution.cwd.empty()) {
            auto cwd_result = expand_placeholders(
                nak_record.execution.cwd,
                effective_env,
                "nak_record.execution.cwd",
                warnings);
            expanded_cwd = cwd_result.value;
        }
    }
    
    // Expand profile paths
    auto expanded_lib_prepend = expand_string_vector(
        profile.paths.library_prepend,
        effective_env,
        "profile.paths.library_prepend",
        warnings);
    
    auto expanded_lib_append = expand_string_vector(
        profile.paths.library_append,
        effective_env,
        "profile.paths.library_append",
        warnings);
    
    // Expand install record override paths
    auto expanded_ovr_lib_prepend = expand_string_vector(
        install_record.overrides.paths.library_prepend,
        effective_env,
        "install_record.overrides.paths.library_prepend",
        warnings);
    
    // Expand install record override arguments
    auto expanded_ovr_args_prepend = expand_string_vector(
        install_record.overrides.arguments.prepend,
        effective_env,
        "install_record.overrides.arguments.prepend",
        warnings);
    
    auto expanded_ovr_args_append = expand_string_vector(
        install_record.overrides.arguments.append,
        effective_env,
        "install_record.overrides.arguments.append",
        warnings);
    
    // Expand manifest entrypoint args
    auto expanded_entry_args = expand_string_vector(
        manifest.entrypoint_args,
        effective_env,
        "manifest.entrypoint_args",
        warnings);
    
    // =========================================================================
    // Step 9: Derive capabilities and enforcement (per SPEC L1044-L1066)
    // =========================================================================
    
    auto enforcement_result = derive_enforcement(
        manifest.permissions_filesystem,
        manifest.permissions_network,
        profile,
        warnings);
    
    contract.enforcement.filesystem = enforcement_result.filesystem;
    contract.enforcement.network = enforcement_result.network;
    contract.capability_usage = enforcement_result.capability_usage;
    
    // =========================================================================
    // Step 10: Determine execution binary and arguments (per SPEC L1068-L1078)
    // =========================================================================
    
    // Read pinned loader from App Install Record (host-resolved at install time)
    const std::string& pinned_loader = install_record.nak.loader;
    
    if (nak_resolved && nak_record.has_loaders()) {
        std::string effective_loader = pinned_loader;
        
        // Auto-select loader if not explicitly pinned
        if (effective_loader.empty()) {
            // Try "default" loader first, then single-loader NAK auto-select
            if (nak_record.loaders.count("default")) {
                effective_loader = "default";
            } else if (nak_record.loaders.size() == 1) {
                effective_loader = nak_record.loaders.begin()->first;
            } else {
                // Multiple loaders but none specified - warn and use app entrypoint
                warnings.emit(Warning::nak_loader_required, {
                    {"reason", "NAK has multiple loaders but app didn't specify which one to use"}
                });
                contract.execution.binary = contract.app.entrypoint;
            }
        }
        
        if (!effective_loader.empty()) {
            auto it = nak_record.loaders.find(effective_loader);
            if (it == nak_record.loaders.end()) {
                // Loader not found in NAK
                warnings.emit(Warning::nak_loader_missing, {
                    {"requested", effective_loader},
                    {"reason", "loader not found in NAK"}
                });
                result.critical_error = CriticalError::NAK_LOADER_INVALID;
                result.envelope.warnings = warnings.get_warnings();
                return result;
            }
            // Found the loader - use it
            contract.execution.binary = it->second.exec_path;
            contract.execution.arguments = expanded_args_template;
        }
    } else {
        // libs-only NAK or not resolved - use app entrypoint
        contract.execution.binary = contract.app.entrypoint;
    }
    
    // Prepend override arguments
    contract.execution.arguments.insert(
        contract.execution.arguments.begin(),
        expanded_ovr_args_prepend.begin(),
        expanded_ovr_args_prepend.end());
    
    // Append manifest entrypoint args
    contract.execution.arguments.insert(
        contract.execution.arguments.end(),
        expanded_entry_args.begin(),
        expanded_entry_args.end());
    
    // Append override arguments
    contract.execution.arguments.insert(
        contract.execution.arguments.end(),
        expanded_ovr_args_append.begin(),
        expanded_ovr_args_append.end());
    
    // =========================================================================
    // Step 11: Determine execution cwd (per SPEC L1080-L1086)
    // =========================================================================
    
    if (nak_resolved && nak_record.execution.present && !expanded_cwd.empty()) {
        if (is_absolute_path(expanded_cwd)) {
            contract.execution.cwd = expanded_cwd;
        } else {
            // Resolve relative to NAK root
            auto cwd_result = normalize_under_root(nak_record.paths.root, expanded_cwd, false);
            if (cwd_result.ok) {
                contract.execution.cwd = cwd_result.path;
            } else {
                result.critical_error = CriticalError::PATH_TRAVERSAL;
                result.critical_error_context = "execution.cwd '" + expanded_cwd + 
                    "' escapes NAK root '" + nak_record.paths.root + "'";
                result.envelope.warnings = warnings.get_warnings();
                return result;
            }
        }
    } else {
        contract.execution.cwd = contract.app.root;
    }
    
    // =========================================================================
    // Step 12: Set library path environment key (per SPEC L1088)
    // =========================================================================
    
    contract.execution.library_path_env_key = get_library_path_env_key();
    
    // =========================================================================
    // Step 13: Build library path list (per SPEC L1090-L1108)
    // =========================================================================
    
    // Profile library_prepend
    for (const auto& path : expanded_lib_prepend) {
        if (!is_absolute_path(path)) {
            warnings.emit(Warning::invalid_library_path,
                          nah::warnings::invalid_library_path(path, "profile.paths.library_prepend"));
            continue;
        }
        contract.execution.library_paths.push_back(path);
    }
    
    // Install record override library_prepend
    for (const auto& path : expanded_ovr_lib_prepend) {
        if (!is_absolute_path(path)) {
            warnings.emit(Warning::invalid_library_path,
                          nah::warnings::invalid_library_path(path, "install_record.overrides.paths.library_prepend"));
            continue;
        }
        contract.execution.library_paths.push_back(path);
    }
    
    // NAK lib_dirs (if resolved)
    if (nak_resolved) {
        for (const auto& lib_dir : nak_record.paths.lib_dirs) {
            contract.execution.library_paths.push_back(lib_dir);
        }
    }
    
    // Manifest LIB_DIR entries
    for (const auto& lib_dir : manifest.lib_dirs) {
        if (is_absolute_path(lib_dir)) {
            warnings.emit(Warning::invalid_manifest, {{"reason", "lib_dir_absolute"}});
            continue;
        }
        
        auto lib_result = normalize_under_root(contract.app.root, lib_dir, false);
        if (!lib_result.ok) {
            result.critical_error = CriticalError::PATH_TRAVERSAL;
            result.critical_error_context = "manifest lib_dir '" + lib_dir + 
                "' escapes app root '" + contract.app.root + "'";
            result.envelope.warnings = warnings.get_warnings();
            return result;
        }
        contract.execution.library_paths.push_back(lib_result.path);
    }
    
    // Profile library_append
    for (const auto& path : expanded_lib_append) {
        if (!is_absolute_path(path)) {
            warnings.emit(Warning::invalid_library_path,
                          nah::warnings::invalid_library_path(path, "profile.paths.library_append"));
            continue;
        }
        contract.execution.library_paths.push_back(path);
    }
    
    // =========================================================================
    // Step 14: Resolve asset exports (per SPEC L1110-L1120)
    // =========================================================================
    
    for (const auto& exp : manifest.asset_exports) {
        if (is_absolute_path(exp.path)) {
            warnings.emit(Warning::invalid_manifest, {{"reason", "asset_export_absolute"}});
            continue;
        }
        
        auto export_result = normalize_under_root(contract.app.root, exp.path, false);
        if (!export_result.ok) {
            result.critical_error = CriticalError::PATH_TRAVERSAL;
            result.critical_error_context = "asset export '" + exp.id + "' path '" + exp.path + 
                "' escapes app root '" + contract.app.root + "'";
            result.envelope.warnings = warnings.get_warnings();
            return result;
        }
        
        AssetExport asset;
        asset.id = exp.id;
        asset.path = export_result.path;
        asset.type = exp.type;
        contract.exports[exp.id] = asset;  // Last wins
    }
    
    // =========================================================================
    // Step 15: Finalize environment map (per SPEC L1122)
    // =========================================================================
    
    contract.environment = effective_env;
    
    // =========================================================================
    // Trust state handling (per SPEC L470-L483)
    // =========================================================================
    
    contract.trust.state = install_record.trust.state;
    contract.trust.source = install_record.trust.source;
    contract.trust.evaluated_at = install_record.trust.evaluated_at;
    contract.trust.expires_at = install_record.trust.expires_at;
    contract.trust.details = install_record.trust.details;
    
    // Emit trust warnings
    if (install_record.trust.source.empty() && install_record.trust.evaluated_at.empty()) {
        // Trust section absent - treat as unknown
        contract.trust.state = TrustState::Unknown;
        warnings.emit(Warning::trust_state_unknown, {});
    } else {
        switch (install_record.trust.state) {
            case TrustState::Verified:
                // No warning for verified
                break;
            case TrustState::Unverified:
                warnings.emit(Warning::trust_state_unverified, {});
                break;
            case TrustState::Failed:
                warnings.emit(Warning::trust_state_failed, {});
                break;
            case TrustState::Unknown:
                warnings.emit(Warning::trust_state_unknown, {});
                break;
        }
    }
    
    // Check staleness
    if (!install_record.trust.expires_at.empty() && !inputs.now.empty()) {
        if (timestamp_before(install_record.trust.expires_at, inputs.now)) {
            warnings.emit(Warning::trust_state_stale, {});
        }
    }
    
    // =========================================================================
    // Finalize result
    // =========================================================================
    
    result.ok = true;
    result.envelope.warnings = warnings.get_warnings();
    
    // Build trace if enabled
    if (inputs.enable_trace) {
        std::unordered_map<std::string, std::unordered_map<std::string, TraceEntry>> trace;
        
        // Environment trace entries
        std::unordered_map<std::string, TraceEntry> env_entries;
        for (const auto& [key, history] : env_history) {
            TraceEntry entry;
            // Find the final value
            auto env_it = contract.environment.find(key);
            if (env_it != contract.environment.end()) {
                entry.value = env_it->second;
            }
            // Find the winning contribution (last accepted one)
            for (auto it = history.rbegin(); it != history.rend(); ++it) {
                if (it->accepted) {
                    entry.source_kind = it->source_kind;
                    entry.source_path = it->source_path;
                    entry.precedence_rank = it->precedence_rank;
                    break;
                }
            }
            entry.history = history;
            env_entries[key] = entry;
        }
        trace["environment"] = env_entries;
        
        result.envelope.trace = trace;
    }
    
    return result;
}

std::string serialize_contract_json(const ContractEnvelope& envelope,
                                     bool include_trace,
                                     std::optional<CriticalError> critical_error) {
    nlohmann::ordered_json j;
    
    j["schema"] = "nah.launch.contract.v1";
    
    if (!critical_error.has_value()) {
        const auto& c = envelope.contract;
        
        // app
        j["app"]["id"] = c.app.id;
        j["app"]["version"] = c.app.version;
        j["app"]["root"] = c.app.root;
        j["app"]["entrypoint"] = c.app.entrypoint;
        
        // nak
        j["nak"]["id"] = c.nak.id;
        j["nak"]["version"] = c.nak.version;
        j["nak"]["root"] = c.nak.root;
        j["nak"]["resource_root"] = c.nak.resource_root;
        j["nak"]["record_ref"] = c.nak.record_ref;
        
        // execution
        j["execution"]["binary"] = c.execution.binary;
        j["execution"]["arguments"] = c.execution.arguments;
        j["execution"]["cwd"] = c.execution.cwd;
        j["execution"]["library_path_env_key"] = c.execution.library_path_env_key;
        j["execution"]["library_paths"] = c.execution.library_paths;
        
        // environment (sorted keys)
        std::vector<std::string> env_keys;
        for (const auto& [k, _] : c.environment) {
            env_keys.push_back(k);
        }
        std::sort(env_keys.begin(), env_keys.end());
        
        nlohmann::ordered_json env_obj;
        for (const auto& k : env_keys) {
            env_obj[k] = c.environment.at(k);
        }
        j["environment"] = env_obj;
        
        // enforcement
        j["enforcement"]["filesystem"] = c.enforcement.filesystem;
        j["enforcement"]["network"] = c.enforcement.network;
        
        // trust
        j["trust"]["state"] = trust_state_to_string(c.trust.state);
        j["trust"]["source"] = c.trust.source;
        j["trust"]["evaluated_at"] = c.trust.evaluated_at;
        j["trust"]["expires_at"] = c.trust.expires_at;
        
        // trust.details (sorted keys)
        std::vector<std::string> detail_keys;
        for (const auto& [k, _] : c.trust.details) {
            detail_keys.push_back(k);
        }
        std::sort(detail_keys.begin(), detail_keys.end());
        
        nlohmann::ordered_json details_obj;
        for (const auto& k : detail_keys) {
            details_obj[k] = c.trust.details.at(k);
        }
        j["trust"]["details"] = details_obj;
        
        // exports (sorted keys)
        std::vector<std::string> export_keys;
        for (const auto& [k, _] : c.exports) {
            export_keys.push_back(k);
        }
        std::sort(export_keys.begin(), export_keys.end());
        
        nlohmann::ordered_json exports_obj;
        for (const auto& k : export_keys) {
            const auto& exp = c.exports.at(k);
            exports_obj[k]["id"] = exp.id;
            exports_obj[k]["path"] = exp.path;
            exports_obj[k]["type"] = exp.type;
        }
        j["exports"] = exports_obj;
        
        // capability_usage
        j["capability_usage"]["present"] = c.capability_usage.present;
        j["capability_usage"]["required_capabilities"] = c.capability_usage.required_capabilities;
        j["capability_usage"]["optional_capabilities"] = c.capability_usage.optional_capabilities;
        j["capability_usage"]["critical_capabilities"] = c.capability_usage.critical_capabilities;
    }
    
    // warnings (sorted by key within each object, but array preserves emission order)
    nlohmann::json warnings_arr = nlohmann::json::array();
    for (const auto& w : envelope.warnings) {
        nlohmann::json warn_obj = nlohmann::json::object();
        warn_obj["action"] = w.action;
        
        // fields (sorted keys)
        std::vector<std::string> field_keys;
        for (const auto& [k, _] : w.fields) {
            field_keys.push_back(k);
        }
        std::sort(field_keys.begin(), field_keys.end());
        
        nlohmann::json fields_obj = nlohmann::json::object();
        for (const auto& k : field_keys) {
            fields_obj[k] = w.fields.at(k);
        }
        warn_obj["fields"] = fields_obj;
        warn_obj["key"] = w.key;
        
        warnings_arr.push_back(warn_obj);
    }
    j["warnings"] = warnings_arr;
    
    // critical_error
    if (critical_error.has_value()) {
        j["critical_error"] = critical_error_to_string(*critical_error);
    } else {
        j["critical_error"] = nullptr;
    }
    
    // trace (optional)
    if (include_trace && envelope.trace.has_value()) {
        nlohmann::json trace_obj = nlohmann::json::object();
        
        // Get sorted keys for deterministic output
        std::vector<std::string> section_keys;
        for (const auto& [section, entries] : *envelope.trace) {
            section_keys.push_back(section);
        }
        std::sort(section_keys.begin(), section_keys.end());
        
        for (const auto& section : section_keys) {
            const auto& entries = envelope.trace->at(section);
            nlohmann::json section_obj = nlohmann::json::object();
            
            // Get sorted entry keys for deterministic output
            std::vector<std::string> entry_keys;
            for (const auto& [key, entry] : entries) {
                entry_keys.push_back(key);
            }
            std::sort(entry_keys.begin(), entry_keys.end());
            
            for (const auto& key : entry_keys) {
                const auto& entry = entries.at(key);
                nlohmann::json entry_obj;
                entry_obj["value"] = entry.value;
                entry_obj["source_kind"] = entry.source_kind;
                entry_obj["source_path"] = entry.source_path;
                entry_obj["precedence_rank"] = entry.precedence_rank;
                
                // Include history if present
                if (!entry.history.empty()) {
                    nlohmann::json history_arr = nlohmann::json::array();
                    for (const auto& contrib : entry.history) {
                        nlohmann::json contrib_obj;
                        contrib_obj["value"] = contrib.value;
                        contrib_obj["source_kind"] = contrib.source_kind;
                        contrib_obj["source_path"] = contrib.source_path;
                        contrib_obj["precedence_rank"] = contrib.precedence_rank;
                        contrib_obj["operation"] = env_op_to_string(contrib.operation);
                        contrib_obj["accepted"] = contrib.accepted;
                        history_arr.push_back(contrib_obj);
                    }
                    entry_obj["history"] = history_arr;
                }
                
                section_obj[key] = entry_obj;
            }
            trace_obj[section] = section_obj;
        }
        j["trace"] = trace_obj;
    }
    
    return j.dump(2);
}

} // namespace nah
