/*
 * NAH JSON - JSON Parsing for NAH Types
 * 
 * This file provides JSON serialization and deserialization for all NAH types.
 * Requires nlohmann/json.
 * 
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NAH_JSON_H
#define NAH_JSON_H

#ifdef __cplusplus

#include "nah_core.h"
#include <nlohmann/json.hpp>

namespace nah {
namespace json {

using json = nlohmann::json;

// ============================================================================
// PARSE RESULTS
// ============================================================================

template<typename T>
struct ParseResult {
    bool ok = false;
    std::string error;
    T value;
    std::vector<std::string> warnings;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

namespace detail {

inline std::string get_string(const json& j, const std::string& key, const std::string& default_val = "") {
    if (j.contains(key) && j[key].is_string()) {
        return j[key].get<std::string>();
    }
    return default_val;
}

inline std::vector<std::string> get_string_array(const json& j, const std::string& key) {
    std::vector<std::string> result;
    if (j.contains(key) && j[key].is_array()) {
        for (const auto& item : j[key]) {
            if (item.is_string()) {
                result.push_back(item.get<std::string>());
            }
        }
    }
    return result;
}

inline bool get_bool(const json& j, const std::string& key, bool default_val = false) {
    if (j.contains(key) && j[key].is_boolean()) {
        return j[key].get<bool>();
    }
    return default_val;
}

} // namespace detail

// ============================================================================
// ENV VALUE PARSING
// ============================================================================

inline core::EnvValue parse_env_value(const json& j) {
    core::EnvValue ev;
    
    if (j.is_string()) {
        ev.op = core::EnvOp::Set;
        ev.value = j.get<std::string>();
        return ev;
    }
    
    if (j.is_object()) {
        std::string op_str = detail::get_string(j, "op", "set");
        auto op = core::parse_env_op(op_str);
        ev.op = op.value_or(core::EnvOp::Set);
        ev.value = detail::get_string(j, "value");
        ev.separator = detail::get_string(j, "separator", ":");
    }
    
    return ev;
}

inline core::EnvMap parse_env_map(const json& j) {
    core::EnvMap result;
    if (j.is_object()) {
        for (auto& [key, val] : j.items()) {
            result[key] = parse_env_value(val);
        }
    }
    return result;
}

// ============================================================================
// TRUST INFO PARSING
// ============================================================================

inline core::TrustInfo parse_trust_info(const json& j) {
    core::TrustInfo ti;
    
    std::string state_str = detail::get_string(j, "state", "unknown");
    auto state = core::parse_trust_state(state_str);
    ti.state = state.value_or(core::TrustState::Unknown);
    
    ti.source = detail::get_string(j, "source");
    ti.evaluated_at = detail::get_string(j, "evaluated_at");
    ti.expires_at = detail::get_string(j, "expires_at");
    ti.inputs_hash = detail::get_string(j, "inputs_hash");
    
    if (j.contains("details") && j["details"].is_object()) {
        for (auto& [key, val] : j["details"].items()) {
            if (val.is_string()) {
                ti.details[key] = val.get<std::string>();
            }
        }
    }
    
    return ti;
}

// ============================================================================
// LOADER CONFIG PARSING
// ============================================================================

inline core::LoaderConfig parse_loader_config(const json& j) {
    core::LoaderConfig lc;
    lc.exec_path = detail::get_string(j, "exec_path");
    lc.args_template = detail::get_string_array(j, "args_template");
    return lc;
}

// ============================================================================
// COMPONENT PARSING
// ============================================================================

inline core::ComponentDecl parse_component(const json& j) {
    core::ComponentDecl comp;
    
    comp.id = detail::get_string(j, "id");
    comp.name = detail::get_string(j, "name");
    comp.description = detail::get_string(j, "description");
    comp.icon = detail::get_string(j, "icon");
    comp.entrypoint = detail::get_string(j, "entrypoint");
    comp.uri_pattern = detail::get_string(j, "uri_pattern");
    comp.loader = detail::get_string(j, "loader");
    comp.standalone = detail::get_bool(j, "standalone", true);
    comp.hidden = detail::get_bool(j, "hidden", false);
    
    // Component-specific environment
    if (j.contains("environment") && j["environment"].is_object()) {
        comp.environment = parse_env_map(j["environment"]);
    }
    
    // Component-specific permissions
    if (j.contains("permissions") && j["permissions"].is_object()) {
        comp.permissions_filesystem = detail::get_string_array(j["permissions"], "filesystem");
        comp.permissions_network = detail::get_string_array(j["permissions"], "network");
    }
    
    // Metadata
    if (j.contains("metadata") && j["metadata"].is_object()) {
        for (auto& [key, value] : j["metadata"].items()) {
            if (value.is_string()) {
                comp.metadata[key] = value.get<std::string>();
            }
        }
    }
    
    return comp;
}

// ============================================================================
// APP DECLARATION PARSING
// ============================================================================

inline ParseResult<core::AppDeclaration> parse_app_declaration(const std::string& json_str) {
    ParseResult<core::AppDeclaration> result;

    try {
        json j = json::parse(json_str);

        // Handle nested "app" structure if present
        if (j.contains("app") && j["app"].is_object()) {
            j = j["app"];
        }

        auto& app = result.value;

        // Identity (nested in v1.1.0 format, flat in older format)
        if (j.contains("identity") && j["identity"].is_object()) {
            // New format: app.identity
            auto& identity = j["identity"];
            app.id = detail::get_string(identity, "id");
            app.version = detail::get_string(identity, "version");
            app.nak_id = detail::get_string(identity, "nak_id");
            app.nak_version_req = detail::get_string(identity, "nak_version_req");
        } else {
            // Legacy flat format
            app.id = detail::get_string(j, "id");
            app.version = detail::get_string(j, "version");
            
            // NAK requirements - check multiple possible formats
            if (j.contains("nak") && j["nak"].is_object()) {
                // Legacy: nak.id and nak.version_req
                app.nak_id = detail::get_string(j["nak"], "id");
                app.nak_version_req = detail::get_string(j["nak"], "version_req");
            } else {
                // Flat format: nak_id and nak_version_req
                app.nak_id = detail::get_string(j, "nak_id");
                app.nak_version_req = detail::get_string(j, "nak_version_req");
            }
        }
        
        if (app.id.empty()) {
            result.error = "missing required field: id";
            return result;
        }
        if (app.version.empty()) {
            result.error = "missing required field: version";
            return result;
        }
        
        // Execution (nested in v1.1.0 format)
        if (j.contains("execution") && j["execution"].is_object()) {
            // New format: app.execution
            auto& execution = j["execution"];
            app.entrypoint_path = detail::get_string(execution, "entrypoint");
            app.entrypoint_args = detail::get_string_array(execution, "args");
            app.nak_loader = detail::get_string(execution, "loader");  // Optional loader preference
        } else if (j.contains("entrypoint")) {
            // Legacy format
            if (j["entrypoint"].is_object()) {
                app.entrypoint_path = detail::get_string(j["entrypoint"], "path");
                app.entrypoint_args = detail::get_string_array(j["entrypoint"], "args");
            } else if (j["entrypoint"].is_string()) {
                app.entrypoint_path = j["entrypoint"].get<std::string>();
            }
        } else {
            app.entrypoint_path = detail::get_string(j, "entrypoint_path");
            app.entrypoint_args = detail::get_string_array(j, "entrypoint_args");
        }
        
        if (app.entrypoint_path.empty()) {
            result.error = "missing required field: entrypoint path";
            return result;
        }
        
        // Layout (nested in v1.1.0 format)
        if (j.contains("layout") && j["layout"].is_object()) {
            // New format: app.layout
            auto& layout = j["layout"];
            app.lib_dirs = detail::get_string_array(layout, "lib_dirs");
            app.asset_dirs = detail::get_string_array(layout, "asset_dirs");
        } else {
            // Legacy flat format
            app.lib_dirs = detail::get_string_array(j, "lib_dirs");
            app.asset_dirs = detail::get_string_array(j, "asset_dirs");
        }
        
        // Environment
        app.env_vars = detail::get_string_array(j, "env_vars");
        if (j.contains("environment") && j["environment"].is_object()) {
            // Also support environment object (v1.1.0 format)
            for (auto& [key, value] : j["environment"].items()) {
                if (value.is_string()) {
                    app.env_vars.push_back(key + "=" + value.get<std::string>());
                }
            }
        }
        
        // Asset exports
        if (j.contains("exports") && j["exports"].is_array()) {
            for (const auto& exp : j["exports"]) {
                core::AssetExportDecl aed;
                aed.id = detail::get_string(exp, "id");
                aed.path = detail::get_string(exp, "path");
                aed.type = detail::get_string(exp, "type");
                app.asset_exports.push_back(aed);
            }
        } else if (j.contains("asset_exports") && j["asset_exports"].is_array()) {
            for (const auto& exp : j["asset_exports"]) {
                core::AssetExportDecl aed;
                aed.id = detail::get_string(exp, "id");
                aed.path = detail::get_string(exp, "path");
                aed.type = detail::get_string(exp, "type");
                app.asset_exports.push_back(aed);
            }
        }
        
        // Permissions
        if (j.contains("permissions") && j["permissions"].is_object()) {
            app.permissions_filesystem = detail::get_string_array(j["permissions"], "filesystem");
            app.permissions_network = detail::get_string_array(j["permissions"], "network");
        }
        
        // Metadata (can be flat or in metadata object)
        if (j.contains("metadata") && j["metadata"].is_object()) {
            app.description = detail::get_string(j["metadata"], "description");
            app.author = detail::get_string(j["metadata"], "author");
            app.license = detail::get_string(j["metadata"], "license");
            app.homepage = detail::get_string(j["metadata"], "homepage");
        } else {
            app.description = detail::get_string(j, "description");
            app.author = detail::get_string(j, "author");
            app.license = detail::get_string(j, "license");
            app.homepage = detail::get_string(j, "homepage");
        }
        
        // Components (new in component architecture)
        if (j.contains("components") && j["components"].is_object()) {
            const auto& comps = j["components"];
            if (comps.contains("provides") && comps["provides"].is_array()) {
                for (const auto& comp_json : comps["provides"]) {
                    app.components.push_back(parse_component(comp_json));
                }
            }
        }
        
        result.ok = true;
        
    } catch (const json::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }
    
    return result;
}

// ============================================================================
// HOST ENVIRONMENT PARSING
// ============================================================================

inline ParseResult<core::HostEnvironment> parse_host_environment(const json& j,
                                                                  const std::string& source_path = "") {
    ParseResult<core::HostEnvironment> result;
    
    try {
        auto& host_env = result.value;
        
        host_env.source_path = source_path;
        
        // Environment section
        if (j.contains("environment") && j["environment"].is_object()) {
            host_env.vars = parse_env_map(j["environment"]);
        }
        
        // Paths section
        if (j.contains("paths") && j["paths"].is_object()) {
            host_env.paths.library_prepend = detail::get_string_array(j["paths"], "library_prepend");
            host_env.paths.library_append = detail::get_string_array(j["paths"], "library_append");
        }
        
        // Overrides section
        if (j.contains("overrides") && j["overrides"].is_object()) {
            const auto& ovr = j["overrides"];
            host_env.overrides.allow_env_overrides = detail::get_bool(ovr, "allow_env_overrides", true);
            host_env.overrides.allowed_env_keys = detail::get_string_array(ovr, "allowed_env_keys");
        }
        
        result.ok = true;
        
    } catch (const json::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }
    
    return result;
}

inline ParseResult<core::HostEnvironment> parse_host_environment(const std::string& json_str,
                                                                  const std::string& source_path = "") {
    ParseResult<core::HostEnvironment> result;
    
    try {
        json j = json::parse(json_str);
        return parse_host_environment(j, source_path);
    } catch (const json::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }
    
    return result;
}

// ============================================================================
// INSTALL RECORD PARSING
// ============================================================================

inline ParseResult<core::InstallRecord> parse_install_record(const std::string& json_str,
                                                              const std::string& source_path = "") {
    ParseResult<core::InstallRecord> result;
    
    try {
        json j = json::parse(json_str);
        auto& ir = result.value;
        
        ir.source_path = source_path;
        
        // Install section
        if (j.contains("install") && j["install"].is_object()) {
            ir.install.instance_id = detail::get_string(j["install"], "instance_id");
        }
        
        if (ir.install.instance_id.empty()) {
            result.error = "missing required field: install.instance_id";
            return result;
        }
        
        // App section (audit only)
        if (j.contains("app") && j["app"].is_object()) {
            ir.app.id = detail::get_string(j["app"], "id");
            ir.app.version = detail::get_string(j["app"], "version");
            ir.app.nak_id = detail::get_string(j["app"], "nak_id");
            ir.app.nak_version_req = detail::get_string(j["app"], "nak_version_req");
        }
        
        // NAK section
        if (j.contains("nak") && j["nak"].is_object()) {
            ir.nak.id = detail::get_string(j["nak"], "id");
            ir.nak.version = detail::get_string(j["nak"], "version");
            ir.nak.record_ref = detail::get_string(j["nak"], "record_ref");
            ir.nak.loader = detail::get_string(j["nak"], "loader");
            ir.nak.selection_reason = detail::get_string(j["nak"], "selection_reason");
        }
        
        // Paths section
        if (j.contains("paths") && j["paths"].is_object()) {
            ir.paths.install_root = detail::get_string(j["paths"], "install_root");
        }
        
        if (ir.paths.install_root.empty()) {
            result.error = "missing required field: paths.install_root";
            return result;
        }
        
        // Provenance section
        if (j.contains("provenance") && j["provenance"].is_object()) {
            ir.provenance.package_hash = detail::get_string(j["provenance"], "package_hash");
            ir.provenance.installed_at = detail::get_string(j["provenance"], "installed_at");
            ir.provenance.installed_by = detail::get_string(j["provenance"], "installed_by");
            ir.provenance.source = detail::get_string(j["provenance"], "source");
        }
        
        // Trust section
        if (j.contains("trust") && j["trust"].is_object()) {
            ir.trust = parse_trust_info(j["trust"]);
        }
        
        // Overrides section
        if (j.contains("overrides") && j["overrides"].is_object()) {
            const auto& ovr = j["overrides"];
            
            if (ovr.contains("environment") && ovr["environment"].is_object()) {
                ir.overrides.environment = parse_env_map(ovr["environment"]);
            }
            
            if (ovr.contains("arguments") && ovr["arguments"].is_object()) {
                ir.overrides.arguments.prepend = detail::get_string_array(ovr["arguments"], "prepend");
                ir.overrides.arguments.append = detail::get_string_array(ovr["arguments"], "append");
            }
            
            if (ovr.contains("paths") && ovr["paths"].is_object()) {
                ir.overrides.paths.library_prepend = detail::get_string_array(ovr["paths"], "library_prepend");
            }
        }
        
        result.ok = true;
        
    } catch (const json::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }
    
    return result;
}

// ============================================================================
// RUNTIME DESCRIPTOR PARSING
// ============================================================================

inline ParseResult<core::RuntimeDescriptor> parse_runtime_descriptor(const std::string& json_str,
                                                                      const std::string& source_path = "") {
    ParseResult<core::RuntimeDescriptor> result;
    
    try {
        json j = json::parse(json_str);
        auto& rd = result.value;
        
        rd.source_path = source_path;
        
        // NAK section
        if (j.contains("nak") && j["nak"].is_object()) {
            rd.nak.id = detail::get_string(j["nak"], "id");
            rd.nak.version = detail::get_string(j["nak"], "version");
        }
        
        if (rd.nak.id.empty()) {
            result.error = "missing required field: nak.id";
            return result;
        }
        if (rd.nak.version.empty()) {
            result.error = "missing required field: nak.version";
            return result;
        }
        
        // Paths section
        if (j.contains("paths") && j["paths"].is_object()) {
            rd.paths.root = detail::get_string(j["paths"], "root");
            rd.paths.resource_root = detail::get_string(j["paths"], "resource_root");
            rd.paths.lib_dirs = detail::get_string_array(j["paths"], "lib_dirs");
        }
        
        if (rd.paths.root.empty()) {
            result.error = "missing required field: paths.root";
            return result;
        }
        
        if (rd.paths.resource_root.empty()) {
            rd.paths.resource_root = rd.paths.root;
        }
        
        // Environment section
        if (j.contains("environment") && j["environment"].is_object()) {
            rd.environment = parse_env_map(j["environment"]);
        }
        
        // Loaders section
        if (j.contains("loaders") && j["loaders"].is_object()) {
            for (auto& [name, config] : j["loaders"].items()) {
                rd.loaders[name] = parse_loader_config(config);
            }
        }
        
        // Execution section
        if (j.contains("execution") && j["execution"].is_object()) {
            rd.execution.present = true;
            rd.execution.cwd = detail::get_string(j["execution"], "cwd");
        }
        
        // Provenance section
        if (j.contains("provenance") && j["provenance"].is_object()) {
            rd.provenance.package_hash = detail::get_string(j["provenance"], "package_hash");
            rd.provenance.installed_at = detail::get_string(j["provenance"], "installed_at");
            rd.provenance.installed_by = detail::get_string(j["provenance"], "installed_by");
            rd.provenance.source = detail::get_string(j["provenance"], "source");
        }
        
        result.ok = true;
        
    } catch (const json::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }
    
    return result;
}

// ============================================================================
// LAUNCH CONTRACT SERIALIZATION (already in nah_core.h, re-export here)
// ============================================================================

using core::serialize_contract;
using core::serialize_result;

// ============================================================================
// LAUNCH CONTRACT PARSING (for cached contracts)
// ============================================================================

inline ParseResult<core::LaunchContract> parse_launch_contract(const std::string& json_str) {
    ParseResult<core::LaunchContract> result;
    
    try {
        json j = json::parse(json_str);
        auto& c = result.value;
        
        // App section
        if (j.contains("app") && j["app"].is_object()) {
            c.app.id = detail::get_string(j["app"], "id");
            c.app.version = detail::get_string(j["app"], "version");
            c.app.root = detail::get_string(j["app"], "root");
            c.app.entrypoint = detail::get_string(j["app"], "entrypoint");
        }
        
        // NAK section
        if (j.contains("nak") && j["nak"].is_object()) {
            c.nak.id = detail::get_string(j["nak"], "id");
            c.nak.version = detail::get_string(j["nak"], "version");
            c.nak.root = detail::get_string(j["nak"], "root");
            c.nak.resource_root = detail::get_string(j["nak"], "resource_root");
            c.nak.record_ref = detail::get_string(j["nak"], "record_ref");
        }
        
        // Execution section
        if (j.contains("execution") && j["execution"].is_object()) {
            c.execution.binary = detail::get_string(j["execution"], "binary");
            c.execution.arguments = detail::get_string_array(j["execution"], "arguments");
            c.execution.cwd = detail::get_string(j["execution"], "cwd");
            c.execution.library_path_env_key = detail::get_string(j["execution"], "library_path_env_key");
            c.execution.library_paths = detail::get_string_array(j["execution"], "library_paths");
        }
        
        // Environment section
        if (j.contains("environment") && j["environment"].is_object()) {
            for (auto& [key, val] : j["environment"].items()) {
                if (val.is_string()) {
                    c.environment[key] = val.get<std::string>();
                }
            }
        }
        
        // Enforcement section
        if (j.contains("enforcement") && j["enforcement"].is_object()) {
            c.enforcement.filesystem = detail::get_string_array(j["enforcement"], "filesystem");
            c.enforcement.network = detail::get_string_array(j["enforcement"], "network");
        }
        
        // Trust section
        if (j.contains("trust") && j["trust"].is_object()) {
            c.trust = parse_trust_info(j["trust"]);
        }
        
        // Capability usage section
        if (j.contains("capability_usage") && j["capability_usage"].is_object()) {
            c.capability_usage.present = detail::get_bool(j["capability_usage"], "present");
            c.capability_usage.required_capabilities = 
                detail::get_string_array(j["capability_usage"], "required_capabilities");
            c.capability_usage.optional_capabilities = 
                detail::get_string_array(j["capability_usage"], "optional_capabilities");
            c.capability_usage.critical_capabilities = 
                detail::get_string_array(j["capability_usage"], "critical_capabilities");
        }
        
        result.ok = true;
        
    } catch (const json::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }
    
    return result;
}

} // namespace json
} // namespace nah

#endif // __cplusplus

#endif // NAH_JSON_H
