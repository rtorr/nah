#include "nah/nak_record.hpp"

#include <toml++/toml.h>
#include <cctype>

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

std::string format_datetime(const toml::date_time& dt) {
    char buf[64];
    if (dt.offset.has_value()) {
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                 dt.date.year, dt.date.month, dt.date.day,
                 dt.time.hour, dt.time.minute, dt.time.second);
    } else {
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
                 dt.date.year, dt.date.month, dt.date.day,
                 dt.time.hour, dt.time.minute, dt.time.second);
    }
    return buf;
}

} // namespace

NakInstallRecordParseResult parse_nak_install_record_full(const std::string& toml_str,
                                                           const std::string& source_path) {
    NakInstallRecordParseResult result;
    result.record.source_path = source_path;
    
    try {
        auto tbl = toml::parse(toml_str);
        
        // schema (REQUIRED)
        if (auto schema = tbl["schema"].value<std::string>()) {
            result.record.schema = trim(*schema);
        } else {
            result.error = "schema missing";
            return result;
        }
        
        if (result.record.schema != "nah.nak.install.v1") {
            result.error = "schema mismatch: expected nah.nak.install.v1";
            return result;
        }
        
        // [nak] section (REQUIRED)
        if (auto nak_tbl = tbl["nak"].as_table()) {
            if (auto id = (*nak_tbl)["id"].value<std::string>()) {
                if (is_empty_after_trim(*id)) {
                    result.error = "nak.id empty";
                    return result;
                }
                result.record.nak.id = *id;
            } else {
                result.error = "nak.id missing";
                return result;
            }
            
            if (auto ver = (*nak_tbl)["version"].value<std::string>()) {
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
        
        // [paths] section (REQUIRED)
        if (auto paths_tbl = tbl["paths"].as_table()) {
            if (auto root = (*paths_tbl)["root"].value<std::string>()) {
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
            if (auto res = (*paths_tbl)["resource_root"].value<std::string>()) {
                result.record.paths.resource_root = *res;
            } else {
                result.record.paths.resource_root = result.record.paths.root;
            }
            
            // lib_dirs
            if (auto arr = (*paths_tbl)["lib_dirs"].as_array()) {
                for (const auto& elem : *arr) {
                    if (auto s = elem.value<std::string>()) {
                        result.record.paths.lib_dirs.push_back(*s);
                    }
                }
            }
        } else {
            result.error = "paths section missing";
            return result;
        }
        
        // [environment] section
        if (auto env_tbl = tbl["environment"].as_table()) {
            for (const auto& [key, val] : *env_tbl) {
                if (auto s = val.value<std::string>()) {
                    result.record.environment[std::string(key.str())] = *s;
                }
            }
        }
        
        // [loader] section (OPTIONAL per SPEC L395)
        if (auto loader_tbl = tbl["loader"].as_table()) {
            result.record.loader.present = true;
            
            if (auto exec = (*loader_tbl)["exec_path"].value<std::string>()) {
                result.record.loader.exec_path = *exec;
            }
            
            if (auto arr = (*loader_tbl)["args_template"].as_array()) {
                for (const auto& elem : *arr) {
                    if (auto s = elem.value<std::string>()) {
                        result.record.loader.args_template.push_back(*s);
                    }
                }
            }
        }
        
        // [execution] section (OPTIONAL per SPEC L402)
        if (auto exec_tbl = tbl["execution"].as_table()) {
            result.record.execution.present = true;
            
            if (auto cwd = (*exec_tbl)["cwd"].value<std::string>()) {
                result.record.execution.cwd = *cwd;
            }
        }
        
        // [provenance] section
        if (auto prov_tbl = tbl["provenance"].as_table()) {
            if (auto h = (*prov_tbl)["package_hash"].value<std::string>()) {
                result.record.provenance.package_hash = *h;
            }
            if (auto dt = (*prov_tbl)["installed_at"].as<toml::date_time>()) {
                result.record.provenance.installed_at = format_datetime(dt->get());
            } else if (auto s = (*prov_tbl)["installed_at"].value<std::string>()) {
                result.record.provenance.installed_at = *s;
            }
            if (auto by = (*prov_tbl)["installed_by"].value<std::string>()) {
                result.record.provenance.installed_by = *by;
            }
            if (auto src = (*prov_tbl)["source"].value<std::string>()) {
                result.record.provenance.source = *src;
            }
        }
        
        result.ok = true;
        return result;
        
    } catch (const toml::parse_error& e) {
        result.error = std::string("parse error: ") + e.description().data();
        return result;
    }
}

bool validate_nak_install_record(const NakInstallRecord& record, std::string& error) {
    if (record.schema != "nah.nak.install.v1") {
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

NakPackManifestParseResult parse_nak_pack_manifest(const std::string& toml_str) {
    NakPackManifestParseResult result;
    
    try {
        auto tbl = toml::parse(toml_str);
        
        // schema (REQUIRED)
        if (auto schema = tbl["schema"].value<std::string>()) {
            result.manifest.schema = trim(*schema);
        } else {
            result.error = "schema missing";
            return result;
        }
        
        if (result.manifest.schema != "nah.nak.pack.v1") {
            result.error = "schema mismatch: expected nah.nak.pack.v1";
            return result;
        }
        
        // [nak] section (REQUIRED)
        if (auto nak_tbl = tbl["nak"].as_table()) {
            if (auto id = (*nak_tbl)["id"].value<std::string>()) {
                if (is_empty_after_trim(*id)) {
                    result.error = "nak.id empty";
                    return result;
                }
                result.manifest.nak.id = *id;
            } else {
                result.error = "nak.id missing";
                return result;
            }
            
            if (auto ver = (*nak_tbl)["version"].value<std::string>()) {
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
        
        // [paths] section
        if (auto paths_tbl = tbl["paths"].as_table()) {
            if (auto res = (*paths_tbl)["resource_root"].value<std::string>()) {
                result.manifest.paths.resource_root = *res;
            }
            
            if (auto arr = (*paths_tbl)["lib_dirs"].as_array()) {
                for (const auto& elem : *arr) {
                    if (auto s = elem.value<std::string>()) {
                        result.manifest.paths.lib_dirs.push_back(*s);
                    }
                }
            }
        }
        
        // [environment] section
        if (auto env_tbl = tbl["environment"].as_table()) {
            for (const auto& [key, val] : *env_tbl) {
                if (auto s = val.value<std::string>()) {
                    result.manifest.environment[std::string(key.str())] = *s;
                }
            }
        }
        
        // [loader] section
        if (auto loader_tbl = tbl["loader"].as_table()) {
            result.manifest.loader.present = true;
            
            if (auto exec = (*loader_tbl)["exec_path"].value<std::string>()) {
                result.manifest.loader.exec_path = *exec;
            }
            
            if (auto arr = (*loader_tbl)["args_template"].as_array()) {
                for (const auto& elem : *arr) {
                    if (auto s = elem.value<std::string>()) {
                        result.manifest.loader.args_template.push_back(*s);
                    }
                }
            }
        }
        
        // [execution] section
        if (auto exec_tbl = tbl["execution"].as_table()) {
            result.manifest.execution.present = true;
            
            if (auto cwd = (*exec_tbl)["cwd"].value<std::string>()) {
                result.manifest.execution.cwd = *cwd;
            }
        }
        
        result.ok = true;
        return result;
        
    } catch (const toml::parse_error& e) {
        result.error = std::string("parse error: ") + e.description().data();
        return result;
    }
}

// Legacy API implementation
NakInstallValidation parse_nak_install_record(const std::string& toml_str, NakInstallRecord& out) {
    auto result = parse_nak_install_record_full(toml_str);
    if (!result.ok) {
        return {false, result.error};
    }
    out = result.record;
    return {true, {}};
}

} // namespace nah
