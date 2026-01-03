#include "nah/install_record.hpp"

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
    // Format as RFC3339
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

AppInstallRecordParseResult parse_app_install_record_full(const std::string& toml_str,
                                                           const std::string& source_path) {
    AppInstallRecordParseResult result;
    result.record.source_path = source_path;
    
    try {
        auto tbl = toml::parse(toml_str);
        
        // schema (REQUIRED)
        if (auto schema = tbl["schema"].value<std::string>()) {
            result.record.schema = trim(*schema);
        } else {
            result.error = "schema missing";
            result.is_critical_error = true;
            return result;
        }
        
        if (result.record.schema != "nah.app.install.v1") {
            result.error = "schema mismatch: expected nah.app.install.v1";
            result.is_critical_error = true;
            return result;
        }
        
        // [install] section
        if (auto install_tbl = tbl["install"].as_table()) {
            if (auto id = (*install_tbl)["instance_id"].value<std::string>()) {
                if (is_empty_after_trim(*id)) {
                    result.error = "install.instance_id empty";
                    result.is_critical_error = true;
                    return result;
                }
                result.record.install.instance_id = *id;
            } else {
                result.error = "install.instance_id missing";
                result.is_critical_error = true;
                return result;
            }
        } else {
            result.error = "install section missing";
            result.is_critical_error = true;
            return result;
        }
        
        // [app] section (audit snapshots only)
        if (auto app_tbl = tbl["app"].as_table()) {
            if (auto id = (*app_tbl)["id"].value<std::string>()) {
                result.record.app.id = *id;
            }
            if (auto ver = (*app_tbl)["version"].value<std::string>()) {
                result.record.app.version = *ver;
            }
            if (auto nak_id = (*app_tbl)["nak_id"].value<std::string>()) {
                result.record.app.nak_id = *nak_id;
            }
            if (auto nak_ver = (*app_tbl)["nak_version_req"].value<std::string>()) {
                result.record.app.nak_version_req = *nak_ver;
            }
        }
        
        // [nak] section (pinned NAK)
        if (auto nak_tbl = tbl["nak"].as_table()) {
            if (auto id = (*nak_tbl)["id"].value<std::string>()) {
                result.record.nak.id = *id;
            }
            if (auto ver = (*nak_tbl)["version"].value<std::string>()) {
                result.record.nak.version = *ver;
            }
            if (auto ref = (*nak_tbl)["record_ref"].value<std::string>()) {
                result.record.nak.record_ref = *ref;
            }
            if (auto reason = (*nak_tbl)["selection_reason"].value<std::string>()) {
                result.record.nak.selection_reason = *reason;
            }
        }
        
        // [paths] section (REQUIRED)
        if (auto paths_tbl = tbl["paths"].as_table()) {
            if (auto root = (*paths_tbl)["install_root"].value<std::string>()) {
                if (is_empty_after_trim(*root)) {
                    result.error = "paths.install_root empty";
                    result.is_critical_error = true;
                    return result;
                }
                result.record.paths.install_root = *root;
            } else {
                result.error = "paths.install_root missing";
                result.is_critical_error = true;
                return result;
            }
        } else {
            result.error = "paths section missing";
            result.is_critical_error = true;
            return result;
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
        
        // [trust] section
        if (auto trust_tbl = tbl["trust"].as_table()) {
            if (auto state = (*trust_tbl)["state"].value<std::string>()) {
                auto parsed = parse_trust_state(*state);
                if (parsed) {
                    result.record.trust.state = *parsed;
                } else {
                    result.warnings.push_back("invalid_trust_state");
                    result.record.trust.state = TrustState::Unknown;
                }
            }
            if (auto src = (*trust_tbl)["source"].value<std::string>()) {
                result.record.trust.source = *src;
            }
            if (auto dt = (*trust_tbl)["evaluated_at"].as<toml::date_time>()) {
                result.record.trust.evaluated_at = format_datetime(dt->get());
            } else if (auto s = (*trust_tbl)["evaluated_at"].value<std::string>()) {
                result.record.trust.evaluated_at = *s;
            }
            if (auto dt = (*trust_tbl)["expires_at"].as<toml::date_time>()) {
                result.record.trust.expires_at = format_datetime(dt->get());
            } else if (auto s = (*trust_tbl)["expires_at"].value<std::string>()) {
                result.record.trust.expires_at = *s;
            }
            if (auto h = (*trust_tbl)["inputs_hash"].value<std::string>()) {
                result.record.trust.inputs_hash = *h;
            }
            
            // [trust.details] - opaque metadata
            if (auto details_tbl = (*trust_tbl)["details"].as_table()) {
                for (const auto& [key, val] : *details_tbl) {
                    if (auto s = val.value<std::string>()) {
                        result.record.trust.details[std::string(key.str())] = *s;
                    } else if (val.is_boolean()) {
                        result.record.trust.details[std::string(key.str())] = 
                            val.as_boolean()->get() ? "true" : "false";
                    }
                }
            }
        }
        
        // [verification] section
        if (auto ver_tbl = tbl["verification"].as_table()) {
            if (auto dt = (*ver_tbl)["last_verified_at"].as<toml::date_time>()) {
                result.record.verification.last_verified_at = format_datetime(dt->get());
            } else if (auto s = (*ver_tbl)["last_verified_at"].value<std::string>()) {
                result.record.verification.last_verified_at = *s;
            }
            if (auto v = (*ver_tbl)["last_verifier_version"].value<std::string>()) {
                result.record.verification.last_verifier_version = *v;
            }
        }
        
        // [overrides] section
        if (auto ovr_tbl = tbl["overrides"].as_table()) {
            // [overrides.environment]
            if (auto env_tbl = (*ovr_tbl)["environment"].as_table()) {
                for (const auto& [key, val] : *env_tbl) {
                    if (auto s = val.value<std::string>()) {
                        result.record.overrides.environment[std::string(key.str())] = *s;
                    }
                }
            }
            
            // [overrides.arguments]
            if (auto args_tbl = (*ovr_tbl)["arguments"].as_table()) {
                if (auto arr = (*args_tbl)["prepend"].as_array()) {
                    for (const auto& elem : *arr) {
                        if (auto s = elem.value<std::string>()) {
                            result.record.overrides.arguments.prepend.push_back(*s);
                        }
                    }
                }
                if (auto arr = (*args_tbl)["append"].as_array()) {
                    for (const auto& elem : *arr) {
                        if (auto s = elem.value<std::string>()) {
                            result.record.overrides.arguments.append.push_back(*s);
                        }
                    }
                }
            }
            
            // [overrides.paths]
            if (auto paths_tbl = (*ovr_tbl)["paths"].as_table()) {
                if (auto arr = (*paths_tbl)["library_prepend"].as_array()) {
                    for (const auto& elem : *arr) {
                        if (auto s = elem.value<std::string>()) {
                            result.record.overrides.paths.library_prepend.push_back(*s);
                        }
                    }
                }
            }
        }
        
        result.ok = true;
        return result;
        
    } catch (const toml::parse_error& e) {
        result.error = std::string("parse error: ") + e.description().data();
        result.is_critical_error = true;
        return result;
    }
}

bool validate_app_install_record(const AppInstallRecord& record, std::string& error) {
    if (record.schema != "nah.app.install.v1") {
        error = "schema mismatch";
        return false;
    }
    if (is_empty_after_trim(record.install.instance_id)) {
        error = "install.instance_id empty or missing";
        return false;
    }
    if (is_empty_after_trim(record.paths.install_root)) {
        error = "paths.install_root empty or missing";
        return false;
    }
    return true;
}

// Legacy API implementation
InstallRecordValidation parse_app_install_record(const std::string& toml_str, AppInstallRecord& out) {
    auto result = parse_app_install_record_full(toml_str);
    if (!result.ok) {
        return {false, result.error};
    }
    out = result.record;
    return {true, {}};
}

} // namespace nah
