#include "nah/manifest_generate.hpp"
#include "nah/manifest_builder.hpp"

#include <toml++/toml.h>

#include <algorithm>

namespace nah {

ManifestInputParseResult parse_manifest_input(const std::string& toml_content) {
    ManifestInputParseResult result;
    
    try {
        auto tbl = toml::parse(toml_content);
        
        // Check schema
        auto schema = tbl["schema"].value<std::string>();
        if (!schema || *schema != "nah.manifest.input.v1") {
            result.error = "missing or invalid schema (expected nah.manifest.input.v1)";
            return result;
        }
        
        // Parse [app] section
        auto app = tbl["app"].as_table();
        if (!app) {
            result.error = "missing [app] section";
            return result;
        }
        
        // Required fields
        auto id = (*app)["id"].value<std::string>();
        if (!id || id->empty()) {
            result.error = "missing required field: app.id";
            return result;
        }
        result.input.id = *id;
        
        auto version = (*app)["version"].value<std::string>();
        if (!version || version->empty()) {
            result.error = "missing required field: app.version";
            return result;
        }
        result.input.version = *version;
        
        auto nak_id = (*app)["nak_id"].value<std::string>();
        if (!nak_id || nak_id->empty()) {
            result.error = "missing required field: app.nak_id";
            return result;
        }
        result.input.nak_id = *nak_id;
        
        auto nak_version_req = (*app)["nak_version_req"].value<std::string>();
        if (!nak_version_req || nak_version_req->empty()) {
            result.error = "missing required field: app.nak_version_req";
            return result;
        }
        result.input.nak_version_req = *nak_version_req;
        
        auto entrypoint = (*app)["entrypoint"].value<std::string>();
        if (!entrypoint || entrypoint->empty()) {
            result.error = "missing required field: app.entrypoint";
            return result;
        }
        // Validate entrypoint is relative
        if (!entrypoint->empty() && (*entrypoint)[0] == '/') {
            result.error = "app.entrypoint must be a relative path";
            return result;
        }
        if (entrypoint->find("..") != std::string::npos) {
            result.error = "app.entrypoint must not contain '..'";
            return result;
        }
        result.input.entrypoint = *entrypoint;
        
        // Optional fields
        if (auto desc = (*app)["description"].value<std::string>()) {
            result.input.description = *desc;
        }
        if (auto auth = (*app)["author"].value<std::string>()) {
            result.input.author = *auth;
        }
        if (auto lic = (*app)["license"].value<std::string>()) {
            result.input.license = *lic;
        }
        if (auto hp = (*app)["homepage"].value<std::string>()) {
            result.input.homepage = *hp;
        }
        
        // Entrypoint args
        if (auto args = (*app)["entrypoint_args"].as_array()) {
            for (const auto& arg : *args) {
                if (auto s = arg.value<std::string>()) {
                    result.input.entrypoint_args.push_back(*s);
                }
            }
        }
        
        // lib_dirs
        if (auto libs = (*app)["lib_dirs"].as_array()) {
            for (const auto& lib : *libs) {
                if (auto s = lib.value<std::string>()) {
                    if (!s->empty() && (*s)[0] == '/') {
                        result.error = "lib_dirs entries must be relative paths";
                        return result;
                    }
                    if (s->find("..") != std::string::npos) {
                        result.error = "lib_dirs entries must not contain '..'";
                        return result;
                    }
                    result.input.lib_dirs.push_back(*s);
                }
            }
        }
        
        // asset_dirs
        if (auto assets = (*app)["asset_dirs"].as_array()) {
            for (const auto& asset : *assets) {
                if (auto s = asset.value<std::string>()) {
                    if (!s->empty() && (*s)[0] == '/') {
                        result.error = "asset_dirs entries must be relative paths";
                        return result;
                    }
                    if (s->find("..") != std::string::npos) {
                        result.error = "asset_dirs entries must not contain '..'";
                        return result;
                    }
                    result.input.asset_dirs.push_back(*s);
                }
            }
        }
        
        // exports (array of tables)
        if (auto exports = (*app)["exports"].as_array()) {
            for (const auto& exp : *exports) {
                if (auto exp_tbl = exp.as_table()) {
                    ManifestInput::AssetExport asset_exp;
                    
                    auto exp_id = (*exp_tbl)["id"].value<std::string>();
                    auto exp_path = (*exp_tbl)["path"].value<std::string>();
                    
                    if (!exp_id || exp_id->empty()) {
                        result.error = "exports entry missing 'id'";
                        return result;
                    }
                    if (!exp_path || exp_path->empty()) {
                        result.error = "exports entry missing 'path'";
                        return result;
                    }
                    if ((*exp_path)[0] == '/') {
                        result.error = "exports path must be relative";
                        return result;
                    }
                    if (exp_path->find("..") != std::string::npos) {
                        result.error = "exports path must not contain '..'";
                        return result;
                    }
                    
                    asset_exp.id = *exp_id;
                    asset_exp.path = *exp_path;
                    
                    if (auto exp_type = (*exp_tbl)["type"].value<std::string>()) {
                        asset_exp.type = *exp_type;
                    }
                    
                    result.input.exports.push_back(asset_exp);
                }
            }
        }
        
        // environment
        if (auto env = (*app)["environment"].as_table()) {
            for (const auto& [key, val] : *env) {
                if (auto s = val.value<std::string>()) {
                    result.input.environment[std::string(key.str())] = *s;
                }
            }
        }
        
        // permissions
        if (auto perms = (*app)["permissions"].as_table()) {
            if (auto fs = (*perms)["filesystem"].as_array()) {
                for (const auto& p : *fs) {
                    if (auto s = p.value<std::string>()) {
                        // Validate format: operation:selector
                        auto colon = s->find(':');
                        if (colon == std::string::npos) {
                            result.error = "invalid filesystem permission format (expected operation:selector)";
                            return result;
                        }
                        std::string op = s->substr(0, colon);
                        if (op != "read" && op != "write" && op != "execute") {
                            result.error = "invalid filesystem permission operation (expected read, write, or execute)";
                            return result;
                        }
                        result.input.permissions_filesystem.push_back(*s);
                    }
                }
            }
            if (auto net = (*perms)["network"].as_array()) {
                for (const auto& p : *net) {
                    if (auto s = p.value<std::string>()) {
                        // Validate format: operation:selector
                        auto colon = s->find(':');
                        if (colon == std::string::npos) {
                            result.error = "invalid network permission format (expected operation:selector)";
                            return result;
                        }
                        std::string op = s->substr(0, colon);
                        if (op != "connect" && op != "listen" && op != "bind") {
                            result.error = "invalid network permission operation (expected connect, listen, or bind)";
                            return result;
                        }
                        result.input.permissions_network.push_back(*s);
                    }
                }
            }
        }
        
        result.ok = true;
        return result;
        
    } catch (const toml::parse_error& e) {
        result.error = std::string("TOML parse error: ") + e.what();
        return result;
    }
}

std::vector<uint8_t> build_manifest_from_input(const ManifestInput& input) {
    ManifestBuilder builder;
    
    builder.id(input.id)
           .version(input.version)
           .nak_id(input.nak_id)
           .nak_version_req(input.nak_version_req)
           .entrypoint(input.entrypoint);
    
    for (const auto& arg : input.entrypoint_args) {
        builder.entrypoint_arg(arg);
    }
    
    if (!input.description.empty()) {
        builder.description(input.description);
    }
    if (!input.author.empty()) {
        builder.author(input.author);
    }
    if (!input.license.empty()) {
        builder.license(input.license);
    }
    if (!input.homepage.empty()) {
        builder.homepage(input.homepage);
    }
    
    for (const auto& lib : input.lib_dirs) {
        builder.lib_dir(lib);
    }
    for (const auto& asset : input.asset_dirs) {
        builder.asset_dir(asset);
    }
    for (const auto& exp : input.exports) {
        builder.asset_export(exp.id, exp.path, exp.type);
    }
    
    // Sort environment keys for deterministic output
    std::vector<std::string> env_keys;
    for (const auto& [key, _] : input.environment) {
        env_keys.push_back(key);
    }
    std::sort(env_keys.begin(), env_keys.end());
    for (const auto& key : env_keys) {
        builder.env(key, input.environment.at(key));
    }
    
    for (const auto& perm : input.permissions_filesystem) {
        builder.filesystem_permission(perm);
    }
    for (const auto& perm : input.permissions_network) {
        builder.network_permission(perm);
    }
    
    return builder.build();
}

ManifestGenerateResult generate_manifest(const std::string& toml_content) {
    ManifestGenerateResult result;
    
    auto parse_result = parse_manifest_input(toml_content);
    if (!parse_result.ok) {
        result.error = parse_result.error;
        result.warnings = parse_result.warnings;
        return result;
    }
    
    result.manifest_bytes = build_manifest_from_input(parse_result.input);
    result.warnings = parse_result.warnings;
    result.ok = true;
    return result;
}

} // namespace nah
