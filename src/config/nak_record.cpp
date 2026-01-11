#include "nah/nak_record.hpp"

#include <cctype>
#include <optional>

#include <nlohmann/json.hpp>

namespace nah {

namespace {

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

bool is_empty_after_trim(const std::string& s) {
    return trim(s).empty();
}

// Helper to safely get a string from JSON
std::optional<std::string> get_string(const nlohmann::json& j, const std::string& key) {
    if (j.contains(key) && j[key].is_string()) {
        return j[key].get<std::string>();
    }
    return std::nullopt;
}

// Helper to safely get a string array from JSON
std::vector<std::string> get_string_array(const nlohmann::json& j, const std::string& key) {
    std::vector<std::string> result;
    if (j.contains(key) && j[key].is_array()) {
        for (const auto& elem : j[key]) {
            if (elem.is_string()) {
                result.push_back(elem.get<std::string>());
            }
        }
    }
    return result;
}

} // namespace

NakInstallRecordParseResult parse_nak_install_record_full(const std::string& json_str,
                                                           const std::string& source_path) {
    NakInstallRecordParseResult result;
    result.record.source_path = source_path;
    
    try {
        auto j = nlohmann::json::parse(json_str);
        
        if (!j.is_object()) {
            result.error = "JSON must be an object";
            return result;
        }
        
        // $schema (REQUIRED)
        if (auto schema = get_string(j, "$schema")) {
            result.record.schema = trim(*schema);
        } else {
            result.error = "$schema missing";
            return result;
        }
        
        if (result.record.schema != "nah.nak.install.v2") {
            result.error = "$schema mismatch: expected nah.nak.install.v2";
            return result;
        }
        
        // "nak" section (REQUIRED)
        if (j.contains("nak") && j["nak"].is_object()) {
            const auto& nak = j["nak"];
            
            if (auto id = get_string(nak, "id")) {
                if (is_empty_after_trim(*id)) {
                    result.error = "nak.id empty";
                    return result;
                }
                result.record.nak.id = *id;
            } else {
                result.error = "nak.id missing";
                return result;
            }
            
            if (auto ver = get_string(nak, "version")) {
                if (is_empty_after_trim(*ver)) {
                    result.error = "nak.version empty";
                    return result;
                }
                result.record.nak.version = *ver;
            } else {
                result.error = "nak.version missing";
                return result;
            }
        } else {
            result.error = "nak section missing";
            return result;
        }
        
        // "paths" section (REQUIRED)
        if (j.contains("paths") && j["paths"].is_object()) {
            const auto& paths = j["paths"];
            
            if (auto root = get_string(paths, "root")) {
                if (is_empty_after_trim(*root)) {
                    result.error = "paths.root empty";
                    return result;
                }
                result.record.paths.root = *root;
            } else {
                result.error = "paths.root missing";
                return result;
            }
            
            // resource_root (defaults to root if omitted)
            if (auto res = get_string(paths, "resource_root")) {
                result.record.paths.resource_root = *res;
            } else {
                result.record.paths.resource_root = result.record.paths.root;
            }
            
            // lib_dirs
            result.record.paths.lib_dirs = get_string_array(paths, "lib_dirs");
        } else {
            result.error = "paths section missing";
            return result;
        }
        
        // "environment" section
        if (j.contains("environment") && j["environment"].is_object()) {
            for (auto& [key, val] : j["environment"].items()) {
                if (val.is_string()) {
                    result.record.environment[key] = val.get<std::string>();
                }
            }
        }
        
        // "loaders" section (OPTIONAL - libs-only NAKs omit this)
        if (j.contains("loaders") && j["loaders"].is_object()) {
            for (auto& [name, loader_json] : j["loaders"].items()) {
                if (loader_json.is_object()) {
                    LoaderConfig config;
                    if (auto exec = get_string(loader_json, "exec_path")) {
                        if (is_empty_after_trim(*exec)) {
                            result.error = "loaders." + name + ".exec_path empty";
                            return result;
                        }
                        config.exec_path = *exec;
                    } else {
                        result.error = "loaders." + name + ".exec_path missing";
                        return result;
                    }
                    config.args_template = get_string_array(loader_json, "args_template");
                    result.record.loaders[name] = config;
                }
            }
        }
        
        // "execution" section (OPTIONAL per SPEC L402)
        if (j.contains("execution") && j["execution"].is_object()) {
            result.record.execution.present = true;
            const auto& exec = j["execution"];
            
            if (auto cwd = get_string(exec, "cwd")) {
                result.record.execution.cwd = *cwd;
            }
        }
        
        // "provenance" section
        if (j.contains("provenance") && j["provenance"].is_object()) {
            const auto& prov = j["provenance"];
            
            if (auto h = get_string(prov, "package_hash")) {
                result.record.provenance.package_hash = *h;
            }
            if (auto dt = get_string(prov, "installed_at")) {
                result.record.provenance.installed_at = *dt;
            }
            if (auto by = get_string(prov, "installed_by")) {
                result.record.provenance.installed_by = *by;
            }
            if (auto src = get_string(prov, "source")) {
                result.record.provenance.source = *src;
            }
        }
        
        result.ok = true;
        return result;
        
    } catch (const nlohmann::json::parse_error& e) {
        result.error = std::string("parse error: ") + e.what();
        return result;
    } catch (const nlohmann::json::exception& e) {
        result.error = std::string("JSON error: ") + e.what();
        return result;
    }
}

bool validate_nak_install_record(const NakInstallRecord& record, std::string& error) {
    if (record.schema != "nah.nak.install.v2") {
        error = "schema mismatch";
        return false;
    }
    if (is_empty_after_trim(record.nak.id)) {
        error = "nak.id empty or missing";
        return false;
    }
    if (is_empty_after_trim(record.nak.version)) {
        error = "nak.version empty or missing";
        return false;
    }
    if (is_empty_after_trim(record.paths.root)) {
        error = "paths.root empty or missing";
        return false;
    }
    return true;
}

NakPackManifestParseResult parse_nak_pack_manifest(const std::string& json_str) {
    NakPackManifestParseResult result;
    
    try {
        auto j = nlohmann::json::parse(json_str);
        
        if (!j.is_object()) {
            result.error = "JSON must be an object";
            return result;
        }
        
        // $schema (REQUIRED)
        if (auto schema = get_string(j, "$schema")) {
            result.manifest.schema = trim(*schema);
        } else {
            result.error = "$schema missing";
            return result;
        }
        
        if (result.manifest.schema != "nah.nak.pack.v2") {
            result.error = "$schema mismatch: expected nah.nak.pack.v2";
            return result;
        }
        
        // "nak" section (REQUIRED)
        if (j.contains("nak") && j["nak"].is_object()) {
            const auto& nak = j["nak"];
            
            if (auto id = get_string(nak, "id")) {
                if (is_empty_after_trim(*id)) {
                    result.error = "nak.id empty";
                    return result;
                }
                result.manifest.nak.id = *id;
            } else {
                result.error = "nak.id missing";
                return result;
            }
            
            if (auto ver = get_string(nak, "version")) {
                if (is_empty_after_trim(*ver)) {
                    result.error = "nak.version empty";
                    return result;
                }
                result.manifest.nak.version = *ver;
            } else {
                result.error = "nak.version missing";
                return result;
            }
        } else {
            result.error = "nak section missing";
            return result;
        }
        
        // "paths" section
        if (j.contains("paths") && j["paths"].is_object()) {
            const auto& paths = j["paths"];
            
            if (auto res = get_string(paths, "resource_root")) {
                result.manifest.paths.resource_root = *res;
            }
            
            result.manifest.paths.lib_dirs = get_string_array(paths, "lib_dirs");
        }
        
        // "environment" section
        if (j.contains("environment") && j["environment"].is_object()) {
            for (auto& [key, val] : j["environment"].items()) {
                if (val.is_string()) {
                    result.manifest.environment[key] = val.get<std::string>();
                }
            }
        }
        
        // "loaders" section (OPTIONAL - libs-only NAKs omit this)
        if (j.contains("loaders") && j["loaders"].is_object()) {
            for (auto& [name, loader_json] : j["loaders"].items()) {
                if (loader_json.is_object()) {
                    LoaderConfig config;
                    if (auto exec = get_string(loader_json, "exec_path")) {
                        if (is_empty_after_trim(*exec)) {
                            result.error = "loaders." + name + ".exec_path empty";
                            return result;
                        }
                        config.exec_path = *exec;
                    } else {
                        result.error = "loaders." + name + ".exec_path missing";
                        return result;
                    }
                    config.args_template = get_string_array(loader_json, "args_template");
                    result.manifest.loaders[name] = config;
                }
            }
        }
        
        // "execution" section
        if (j.contains("execution") && j["execution"].is_object()) {
            result.manifest.execution.present = true;
            const auto& exec = j["execution"];
            
            if (auto cwd = get_string(exec, "cwd")) {
                result.manifest.execution.cwd = *cwd;
            }
        }
        
        result.ok = true;
        return result;
        
    } catch (const nlohmann::json::parse_error& e) {
        result.error = std::string("parse error: ") + e.what();
        return result;
    } catch (const nlohmann::json::exception& e) {
        result.error = std::string("JSON error: ") + e.what();
        return result;
    }
}

// Legacy API implementation
NakInstallValidation parse_nak_install_record(const std::string& json_str, NakInstallRecord& out) {
    auto result = parse_nak_install_record_full(json_str);
    if (!result.ok) {
        return {false, result.error};
    }
    out = result.record;
    return {true, {}};
}

} // namespace nah
