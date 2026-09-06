/*
 * NAH JSON - JSON Parsing for NAH Types
 *
 * This file parses NAH's JSON boundary formats and exposes contract serializers.
 * Requires nlohmann/json.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef NAH_JSON_H
#define NAH_JSON_H

#ifdef __cplusplus

#include "nah_core.h"
#include <initializer_list>
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

inline std::string validate_keys(const json& value,
                                 std::initializer_list<const char*> allowed,
                                 const std::string& context) {
    if (!value.is_object()) return context + " must be an object";
    for (auto item = value.begin(); item != value.end(); ++item) {
        const std::string key = item.key();
        const bool known = std::any_of(allowed.begin(), allowed.end(),
            [&key](const char* candidate) { return key == candidate; });
        if (!known) return "unknown field in " + context + ": " + key;
    }
    return {};
}

inline std::string validate_string_array(const json& value, const std::string& context) {
    if (!value.is_array()) return context + " must be an array";
    for (const auto& item : value) {
        if (!item.is_string()) return context + " must contain only strings";
    }
    return {};
}

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

inline std::string validate_env_map(const json& values) {
    if (!values.is_object()) return "environment must be an object";
    for (const auto& [key, value] : values.items()) {
        if (key.empty()) return "environment keys must not be empty";
        if (key.find('=') != std::string::npos || key.find('\0') != std::string::npos) {
            return "invalid environment key: '" + key + "'";
        }
        if (value.is_string()) continue;
        if (!value.is_object()) return "environment value for '" + key + "' must be a string or object";
        if (const auto error = detail::validate_keys(value, {"op", "value", "separator"},
                "environment operation for '" + key + "'"); !error.empty()) return error;
        if (!value.contains("op") || !value["op"].is_string()) {
            return "environment operation for '" + key + "' requires a string op";
        }
        const auto op = core::parse_env_op(value["op"].get<std::string>());
        if (!op) return "unknown environment operation for '" + key + "'";
        if (*op != core::EnvOp::Unset && (!value.contains("value") || !value["value"].is_string())) {
            return "environment operation for '" + key + "' requires a string value";
        }
        if (value.contains("separator") && !value["separator"].is_string()) {
            return "environment separator for '" + key + "' must be a string";
        }
    }
    return {};
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
// APP DECLARATION PARSING
// ============================================================================

inline ParseResult<core::AppDeclaration> parse_app_declaration(const std::string& json_str) {
    ParseResult<core::AppDeclaration> result;

    try {
        json j = json::parse(json_str);

        if (!j.is_object()) {
            result.error = "app manifest must be an object";
            return result;
        }
        if (const auto error = detail::validate_keys(j, {"$schema", "app"}, "app manifest"); !error.empty()) {
            result.error = error;
            return result;
        }
        if (j.contains("$schema") && (!j["$schema"].is_string() ||
             j["$schema"].get<std::string>() != "https://nah.rtorr.com/schemas/nap.v2.json")) {
            result.error = "unsupported app manifest schema";
            return result;
        }
        if (!j.contains("app") || !j["app"].is_object()) {
            result.error = "missing required object: app";
            return result;
        }
        j = j["app"];
        if (const auto error = detail::validate_keys(j,
                {"identity", "execution", "layout", "environment", "permissions", "exports", "metadata"},
                "app"); !error.empty()) {
            result.error = error;
            return result;
        }

        auto& app = result.value;
        if (!j.contains("identity") || !j["identity"].is_object()) {
            result.error = "missing required object: app.identity";
            return result;
        }
        const auto& identity = j["identity"];
        if (const auto error = detail::validate_keys(identity,
                {"id", "version", "nak_id", "nak_version_req"}, "app.identity"); !error.empty()) {
            result.error = error;
            return result;
        }
        app.id = detail::get_string(identity, "id");
        app.version = detail::get_string(identity, "version");
        app.nak_id = detail::get_string(identity, "nak_id");
        app.nak_version_req = detail::get_string(identity, "nak_version_req");

        if (app.id.empty()) {
            result.error = "missing required field: id";
            return result;
        }
        if (app.version.empty()) {
            result.error = "missing required field: version";
            return result;
        }

        if (!j.contains("execution") || !j["execution"].is_object()) {
            result.error = "missing required object: app.execution";
            return result;
        }
        const auto& execution = j["execution"];
        if (const auto error = detail::validate_keys(execution, {"entrypoint", "loader", "args"},
                                                       "app.execution"); !error.empty()) {
            result.error = error;
            return result;
        }
        if (execution.contains("args")) {
            if (const auto error = detail::validate_string_array(execution["args"], "app.execution.args");
                !error.empty()) { result.error = error; return result; }
        }
        app.entrypoint_path = detail::get_string(execution, "entrypoint");
        app.entrypoint_args = detail::get_string_array(execution, "args");
        app.nak_loader = detail::get_string(execution, "loader");

        if (app.entrypoint_path.empty()) {
            result.error = "missing required field: entrypoint path";
            return result;
        }

        if (j.contains("layout")) {
            const auto& layout = j["layout"];
            if (const auto error = detail::validate_keys(layout, {"lib_dirs", "asset_dirs"}, "app.layout");
                !error.empty()) { result.error = error; return result; }
            for (const char* key : {"lib_dirs", "asset_dirs"}) {
                if (layout.contains(key)) {
                    if (const auto error = detail::validate_string_array(layout[key], std::string("app.layout.") + key);
                        !error.empty()) { result.error = error; return result; }
                }
            }
            app.lib_dirs = detail::get_string_array(layout, "lib_dirs");
            app.asset_dirs = detail::get_string_array(layout, "asset_dirs");
        }

        if (j.contains("environment") && j["environment"].is_object()) {
            const auto error = validate_env_map(j["environment"]);
            if (!error.empty()) {
                result.error = error;
                return result;
            }
            app.environment = parse_env_map(j["environment"]);
        } else if (j.contains("environment")) {
            result.error = "environment must be an object";
            return result;
        }

        if (j.contains("exports")) {
            if (!j["exports"].is_array()) { result.error = "app.exports must be an array"; return result; }
            for (const auto& exp : j["exports"]) {
                if (const auto error = detail::validate_keys(exp, {"id", "path", "type"}, "app.exports item");
                    !error.empty()) { result.error = error; return result; }
                core::AssetExportDecl aed;
                aed.id = detail::get_string(exp, "id");
                aed.path = detail::get_string(exp, "path");
                aed.type = detail::get_string(exp, "type");
                if (aed.id.empty() || aed.path.empty()) {
                    result.error = "app.exports items require string id and path";
                    return result;
                }
                app.asset_exports.push_back(aed);
            }
        }

        if (j.contains("permissions")) {
            const auto& permissions = j["permissions"];
            if (const auto error = detail::validate_keys(permissions, {"filesystem", "network"}, "app.permissions");
                !error.empty()) { result.error = error; return result; }
            for (const char* key : {"filesystem", "network"}) {
                if (permissions.contains(key)) {
                    if (const auto error = detail::validate_string_array(permissions[key], std::string("app.permissions.") + key);
                        !error.empty()) { result.error = error; return result; }
                }
            }
            app.permissions_filesystem = detail::get_string_array(permissions, "filesystem");
            app.permissions_network = detail::get_string_array(permissions, "network");
        }

        if (j.contains("metadata")) {
            if (!j["metadata"].is_object()) { result.error = "app.metadata must be an object"; return result; }
            for (const char* key : {"description", "author", "license", "homepage"}) {
                if (j["metadata"].contains(key) && !j["metadata"][key].is_string()) {
                    result.error = std::string("app.metadata.") + key + " must be a string";
                    return result;
                }
            }
            app.description = detail::get_string(j["metadata"], "description");
            app.author = detail::get_string(j["metadata"], "author");
            app.license = detail::get_string(j["metadata"], "license");
            app.homepage = detail::get_string(j["metadata"], "homepage");
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
        if (j.contains("$schema") &&
            (!j["$schema"].is_string() ||
             j["$schema"].get<std::string>() != "https://nah.rtorr.com/schemas/nah.v2.json")) {
            result.error = "unsupported host schema";
            return result;
        }
        if (const auto error = detail::validate_keys(j, {"$schema", "environment", "paths"}, "host configuration");
            !error.empty()) { result.error = error; return result; }
        auto& host_env = result.value;
        const json& config = j;

        host_env.source_path = source_path;

        // Environment section
        if (config.contains("environment") && config["environment"].is_object()) {
            const auto error = validate_env_map(config["environment"]);
            if (!error.empty()) {
                result.error = error;
                return result;
            }
            host_env.vars = parse_env_map(config["environment"]);
        } else if (config.contains("environment")) {
            result.error = "environment must be an object";
            return result;
        }

        // Paths section
        if (config.contains("paths")) {
            if (const auto error = detail::validate_keys(config["paths"], {"library_prepend", "library_append"},
                                                           "host paths"); !error.empty()) {
                result.error = error; return result;
            }
            for (const char* key : {"library_prepend", "library_append"}) {
                if (config["paths"].contains(key)) {
                    if (const auto error = detail::validate_string_array(config["paths"][key],
                            std::string("host paths.") + key); !error.empty()) {
                        result.error = error; return result;
                    }
                }
            }
            host_env.paths.library_prepend = detail::get_string_array(config["paths"], "library_prepend");
            host_env.paths.library_append = detail::get_string_array(config["paths"], "library_append");
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

        if (j.contains("$schema") &&
            (!j["$schema"].is_string() ||
             j["$schema"].get<std::string>() != "https://nah.rtorr.com/schemas/app-record.v2.json")) {
            result.error = "unsupported app record schema";
            return result;
        }
        if (const auto error = detail::validate_keys(j,
                {"$schema", "install", "app", "nak", "paths", "provenance", "trust", "overrides"},
                "app install record"); !error.empty()) { result.error = error; return result; }
        for (const char* section : {"install", "app", "paths"}) {
            if (!j.contains(section) || !j[section].is_object()) {
                result.error = std::string("missing required object: ") + section;
                return result;
            }
        }

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
        if (ir.app.id.empty() || ir.app.version.empty()) {
            result.error = "app record requires app.id and app.version";
            return result;
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
                const auto error = validate_env_map(ovr["environment"]);
                if (!error.empty()) {
                    result.error = error;
                    return result;
                }
                ir.overrides.environment = parse_env_map(ovr["environment"]);
            } else if (ovr.contains("environment")) {
                result.error = "environment must be an object";
                return result;
            }

            if (ovr.contains("arguments") && ovr["arguments"].is_object()) {
                ir.overrides.arguments.prepend = detail::get_string_array(ovr["arguments"], "prepend");
                ir.overrides.arguments.append = detail::get_string_array(ovr["arguments"], "append");
            }

            if (ovr.contains("paths") && ovr["paths"].is_object()) {
                ir.overrides.paths.library_prepend = detail::get_string_array(ovr["paths"], "library_prepend");
                ir.overrides.paths.library_append = detail::get_string_array(ovr["paths"], "library_append");
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

        if (j.contains("$schema") &&
            (!j["$schema"].is_string() ||
             j["$schema"].get<std::string>() != "https://nah.rtorr.com/schemas/nak-record.v1.json")) {
            result.error = "unsupported NAK record schema";
            return result;
        }
        if (const auto error = detail::validate_keys(j,
                {"$schema", "nak", "paths", "environment", "loaders", "execution", "provenance", "trust"},
                "NAK install record"); !error.empty()) { result.error = error; return result; }
        for (const char* section : {"nak", "paths"}) {
            if (!j.contains(section) || !j[section].is_object()) {
                result.error = std::string("missing required object: ") + section;
                return result;
            }
        }

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

        // Environment section
        if (j.contains("environment") && j["environment"].is_object()) {
            const auto error = validate_env_map(j["environment"]);
            if (!error.empty()) {
                result.error = error;
                return result;
            }
            rd.environment = parse_env_map(j["environment"]);
        } else if (j.contains("environment")) {
            result.error = "environment must be an object";
            return result;
        }

        // Loaders section
        if (j.contains("loaders") && j["loaders"].is_object()) {
            for (auto& [name, config] : j["loaders"].items()) {
                if (const auto error = detail::validate_keys(config, {"exec_path", "args_template"},
                        "NAK loader " + name); !error.empty()) { result.error = error; return result; }
                if (!config.contains("exec_path") || !config["exec_path"].is_string() ||
                    config["exec_path"].get<std::string>().empty()) {
                    result.error = "NAK loader requires a non-empty exec_path: " + name;
                    return result;
                }
                if (config.contains("args_template")) {
                    if (const auto error = detail::validate_string_array(config["args_template"],
                            "NAK loader args_template"); !error.empty()) { result.error = error; return result; }
                }
                rd.loaders[name] = parse_loader_config(config);
            }
        } else if (j.contains("loaders")) {
            result.error = "loaders must be an object";
            return result;
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

        if (j.contains("trust") && j["trust"].is_object()) {
            rd.trust = parse_trust_info(j["trust"]);
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

        if (const auto error = detail::validate_keys(j,
                {"schema", "app", "nak", "execution", "environment", "permissions", "trust"},
                "launch contract"); !error.empty()) { result.error = error; return result; }
        for (const char* section : {"app", "nak", "execution", "environment", "permissions", "trust"}) {
            if (!j.contains(section) || !j[section].is_object()) {
                result.error = std::string("missing required object: ") + section;
                return result;
            }
        }
        if (!j.contains("schema") || !j["schema"].is_string() ||
            j["schema"].get<std::string>() != core::NAH_CONTRACT_SCHEMA) {
            result.error = "unsupported launch contract schema";
            return result;
        }

        if (const auto error = detail::validate_keys(j["app"],
                {"id", "version", "root", "entrypoint", "package_hash"}, "launch app");
            !error.empty()) { result.error = error; return result; }
        for (const char* key : {"id", "version", "root", "entrypoint", "package_hash"}) {
            if (!j["app"].contains(key) || !j["app"][key].is_string()) {
                result.error = std::string("launch app requires string field: ") + key;
                return result;
            }
        }
        if (const auto error = detail::validate_keys(j["nak"],
                {"id", "version", "root", "resource_root", "record_ref", "package_hash"}, "launch NAK");
            !error.empty()) { result.error = error; return result; }
        for (const char* key : {"id", "version", "root", "resource_root", "record_ref", "package_hash"}) {
            if (!j["nak"].contains(key) || !j["nak"][key].is_string()) {
                result.error = std::string("launch NAK requires string field: ") + key;
                return result;
            }
        }
        if (const auto error = detail::validate_keys(j["execution"],
                {"binary", "arguments", "cwd", "library_path_env_key", "library_paths"}, "launch execution");
            !error.empty()) { result.error = error; return result; }
        for (const char* key : {"binary", "cwd", "library_path_env_key"}) {
            if (!j["execution"].contains(key) || !j["execution"][key].is_string()) {
                result.error = std::string("launch execution requires string field: ") + key;
                return result;
            }
        }
        for (const char* key : {"arguments", "library_paths"}) {
            if (!j["execution"].contains(key)) {
                result.error = std::string("launch execution requires field: ") + key;
                return result;
            }
            if (const auto error = detail::validate_string_array(j["execution"][key],
                    std::string("launch execution.") + key); !error.empty()) {
                result.error = error; return result;
            }
        }
        if (const auto error = detail::validate_keys(j["permissions"], {"filesystem", "network"},
                                                       "launch permissions"); !error.empty()) {
            result.error = error; return result;
        }
        for (const char* key : {"filesystem", "network"}) {
            if (!j["permissions"].contains(key)) {
                result.error = std::string("launch permissions requires field: ") + key;
                return result;
            }
            if (const auto error = detail::validate_string_array(j["permissions"][key],
                    std::string("launch permissions.") + key); !error.empty()) {
                result.error = error; return result;
            }
        }
        for (const auto& [key, value] : j["environment"].items()) {
            if (!value.is_string()) { result.error = "launch environment value must be a string: " + key; return result; }
        }
        if (const auto error = detail::validate_keys(j["trust"],
                {"state", "source", "evaluated_at", "expires_at"}, "launch trust");
            !error.empty()) { result.error = error; return result; }
        for (const char* key : {"state", "source", "evaluated_at", "expires_at"}) {
            if (!j["trust"].contains(key) || !j["trust"][key].is_string()) {
                result.error = std::string("launch trust requires string field: ") + key;
                return result;
            }
        }
        if (!core::parse_trust_state(j["trust"]["state"].get<std::string>())) {
            result.error = "launch trust state is invalid";
            return result;
        }

        // App section
        if (j.contains("app") && j["app"].is_object()) {
            c.app.id = detail::get_string(j["app"], "id");
            c.app.version = detail::get_string(j["app"], "version");
            c.app.root = detail::get_string(j["app"], "root");
            c.app.entrypoint = detail::get_string(j["app"], "entrypoint");
            c.app.package_hash = detail::get_string(j["app"], "package_hash");
        }

        // NAK section
        if (j.contains("nak") && j["nak"].is_object()) {
            c.nak.id = detail::get_string(j["nak"], "id");
            c.nak.version = detail::get_string(j["nak"], "version");
            c.nak.root = detail::get_string(j["nak"], "root");
            c.nak.resource_root = detail::get_string(j["nak"], "resource_root");
            c.nak.record_ref = detail::get_string(j["nak"], "record_ref");
            c.nak.package_hash = detail::get_string(j["nak"], "package_hash");
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

        // Permission requests
        if (j.contains("permissions") && j["permissions"].is_object()) {
            c.permissions.filesystem = detail::get_string_array(j["permissions"], "filesystem");
            c.permissions.network = detail::get_string_array(j["permissions"], "network");
        }

        // Trust section
        if (j.contains("trust") && j["trust"].is_object()) {
            c.trust = parse_trust_info(j["trust"]);
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
