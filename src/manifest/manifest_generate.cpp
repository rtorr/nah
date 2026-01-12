#include "nah/manifest_generate.hpp"
#include "nah/manifest_builder.hpp"

#include <algorithm>
#include <optional>
#include <set>

#include <nlohmann/json.hpp>

namespace nah {

namespace {

std::optional<std::string> get_string(const nlohmann::json& j, const std::string& key) {
    if (j.contains(key) && j[key].is_string()) {
        return j[key].get<std::string>();
    }
    return std::nullopt;
}

std::vector<std::string> get_string_array(const nlohmann::json& j, const std::string& key) {
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

} // anonymous namespace

ManifestInputParseResult parse_manifest_input(const std::string& json_content) {
    ManifestInputParseResult result;
    
    try {
        auto j = nlohmann::json::parse(json_content);
        
        // Check for unknown top-level fields
        static const std::set<std::string> known_top_level = {"$schema", "app"};
        for (const auto& [key, _] : j.items()) {
            if (known_top_level.find(key) == known_top_level.end()) {
                result.warnings.push_back("unknown field '" + key + "' (ignored)");
            }
        }
        
        // Parse "app" section
        if (!j.contains("app") || !j["app"].is_object()) {
            result.error = "missing \"app\" section";
            return result;
        }
        const auto& app = j["app"];
        
        // Check for unknown fields in "app" section
        static const std::set<std::string> known_app_fields = {
            "id", "version", "nak_id", "nak_version_req", "nak_loader",
            "entrypoint", "entrypoint_args", "description", "author",
            "license", "homepage", "lib_dirs", "asset_dirs", "exports",
            "environment", "permissions"
        };
        for (const auto& [key, _] : app.items()) {
            if (known_app_fields.find(key) == known_app_fields.end()) {
                std::string warning = "unknown field 'app." + key + "' (ignored)";
                // Provide hints for common mistakes
                if (key == "dependencies" || key == "deps" || key == "naks" || key == "requirements") {
                    warning += "\n  hint: To declare a NAK dependency, use 'nak_id' and 'nak_version_req' directly:\n"
                               "    \"nak_id\": \"com.example.sdk\",\n"
                               "    \"nak_version_req\": \">=1.0.0\"";
                } else if (key == "name") {
                    warning += "\n  hint: Use 'id' for the app identifier (e.g., \"com.example.myapp\")";
                } else if (key == "entry" || key == "main" || key == "bin") {
                    warning += "\n  hint: Use 'entrypoint' for the executable path (e.g., \"bin/myapp\")";
                }
                result.warnings.push_back(warning);
            }
        }
        
        // Required fields
        auto id = get_string(app, "id");
        if (!id || id->empty()) {
            result.error = "missing required field: app.id";
            return result;
        }
        result.input.id = *id;
        
        auto version = get_string(app, "version");
        if (!version || version->empty()) {
            result.error = "missing required field: app.version";
            return result;
        }
        result.input.version = *version;
        
        // nak_id and nak_version_req are optional for standalone apps
        auto nak_id = get_string(app, "nak_id");
        if (nak_id && !nak_id->empty()) {
            result.input.nak_id = *nak_id;
            
            auto nak_version_req = get_string(app, "nak_version_req");
            if (nak_version_req && !nak_version_req->empty()) {
                result.input.nak_version_req = *nak_version_req;
            } else {
                // Default to any version if nak_id specified but no version req
                result.input.nak_version_req = "*";
            }
        }
        // If nak_id is empty/missing, app is standalone (no NAK dependency)
        
        // Optional: nak_loader (which loader to use from the NAK)
        if (auto nak_loader = get_string(app, "nak_loader")) {
            result.input.nak_loader = *nak_loader;
        }
        
        auto entrypoint = get_string(app, "entrypoint");
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
        if (auto desc = get_string(app, "description")) {
            result.input.description = *desc;
        }
        if (auto auth = get_string(app, "author")) {
            result.input.author = *auth;
        }
        if (auto lic = get_string(app, "license")) {
            result.input.license = *lic;
        }
        if (auto hp = get_string(app, "homepage")) {
            result.input.homepage = *hp;
        }
        
        // Entrypoint args
        result.input.entrypoint_args = get_string_array(app, "entrypoint_args");
        
        // lib_dirs
        auto lib_dirs = get_string_array(app, "lib_dirs");
        for (const auto& s : lib_dirs) {
            if (!s.empty() && s[0] == '/') {
                result.error = "lib_dirs entries must be relative paths";
                return result;
            }
            if (s.find("..") != std::string::npos) {
                result.error = "lib_dirs entries must not contain '..'";
                return result;
            }
            result.input.lib_dirs.push_back(s);
        }
        
        // asset_dirs
        auto asset_dirs = get_string_array(app, "asset_dirs");
        for (const auto& s : asset_dirs) {
            if (!s.empty() && s[0] == '/') {
                result.error = "asset_dirs entries must be relative paths";
                return result;
            }
            if (s.find("..") != std::string::npos) {
                result.error = "asset_dirs entries must not contain '..'";
                return result;
            }
            result.input.asset_dirs.push_back(s);
        }
        
        // exports (array of objects)
        if (app.contains("exports") && app["exports"].is_array()) {
            for (const auto& exp : app["exports"]) {
                if (!exp.is_object()) continue;
                
                ManifestInput::AssetExport asset_exp;
                
                auto exp_id = get_string(exp, "id");
                auto exp_path = get_string(exp, "path");
                
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
                
                if (auto exp_type = get_string(exp, "type")) {
                    asset_exp.type = *exp_type;
                }
                
                result.input.exports.push_back(asset_exp);
            }
        }
        
        // environment
        if (app.contains("environment") && app["environment"].is_object()) {
            for (const auto& [key, val] : app["environment"].items()) {
                if (val.is_string()) {
                    result.input.environment[key] = val.get<std::string>();
                }
            }
        }
        
        // permissions
        if (app.contains("permissions") && app["permissions"].is_object()) {
            const auto& perms = app["permissions"];
            
            auto fs_perms = get_string_array(perms, "filesystem");
            for (const auto& s : fs_perms) {
                // Validate format: operation:selector
                auto colon = s.find(':');
                if (colon == std::string::npos) {
                    result.error = "invalid filesystem permission format (expected operation:selector)";
                    return result;
                }
                std::string op = s.substr(0, colon);
                if (op != "read" && op != "write" && op != "execute") {
                    result.error = "invalid filesystem permission operation (expected read, write, or execute)";
                    return result;
                }
                result.input.permissions_filesystem.push_back(s);
            }
            
            auto net_perms = get_string_array(perms, "network");
            for (const auto& s : net_perms) {
                // Validate format: operation:selector
                auto colon = s.find(':');
                if (colon == std::string::npos) {
                    result.error = "invalid network permission format (expected operation:selector)";
                    return result;
                }
                std::string op = s.substr(0, colon);
                if (op != "connect" && op != "listen" && op != "bind") {
                    result.error = "invalid network permission operation (expected connect, listen, or bind)";
                    return result;
                }
                result.input.permissions_network.push_back(s);
            }
        }
        
        result.ok = true;
        return result;
        
    } catch (const nlohmann::json::parse_error& e) {
        result.error = std::string("JSON parse error: ") + e.what();
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
    
    if (!input.nak_loader.empty()) {
        builder.nak_loader(input.nak_loader);
    }
    
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

ManifestGenerateResult generate_manifest(const std::string& json_content) {
    ManifestGenerateResult result;
    
    auto parse_result = parse_manifest_input(json_content);
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
