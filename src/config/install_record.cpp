#include "nah/install_record.hpp"

#include <nlohmann/json.hpp>
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

AppInstallRecordParseResult parse_app_install_record_full(const std::string& json_str,
                                                           const std::string& source_path) {
    AppInstallRecordParseResult result;
    result.record.source_path = source_path;
    
    try {
        auto j = nlohmann::json::parse(json_str);
        
        if (!j.is_object()) {
            result.error = "JSON must be an object";
            result.is_critical_error = true;
            return result;
        }
        
        // $schema (REQUIRED)
        if (auto schema = get_string(j, "$schema")) {
            result.record.schema = trim(*schema);
        } else {
            result.error = "$schema missing";
            result.is_critical_error = true;
            return result;
        }
        
        if (result.record.schema != "nah.app.install.v2") {
            result.error = "$schema mismatch: expected nah.app.install.v2";
            result.is_critical_error = true;
            return result;
        }
        
        // "install" section
        if (j.contains("install") && j["install"].is_object()) {
            const auto& install = j["install"];
            if (auto id = get_string(install, "instance_id")) {
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
        
        // "app" section (audit snapshots only)
        if (j.contains("app") && j["app"].is_object()) {
            const auto& app = j["app"];
            if (auto id = get_string(app, "id")) {
                result.record.app.id = *id;
            }
            if (auto ver = get_string(app, "version")) {
                result.record.app.version = *ver;
            }
            if (auto nak_id = get_string(app, "nak_id")) {
                result.record.app.nak_id = *nak_id;
            }
            if (auto nak_ver = get_string(app, "nak_version_req")) {
                result.record.app.nak_version_req = *nak_ver;
            }
        }
        
        // "nak" section (pinned NAK)
        if (j.contains("nak") && j["nak"].is_object()) {
            const auto& nak = j["nak"];
            if (auto id = get_string(nak, "id")) {
                result.record.nak.id = *id;
            }
            if (auto ver = get_string(nak, "version")) {
                result.record.nak.version = *ver;
            }
            if (auto ref = get_string(nak, "record_ref")) {
                result.record.nak.record_ref = *ref;
            }
            if (auto reason = get_string(nak, "selection_reason")) {
                result.record.nak.selection_reason = *reason;
            }
        }
        
        // "paths" section (REQUIRED)
        if (j.contains("paths") && j["paths"].is_object()) {
            const auto& paths = j["paths"];
            if (auto root = get_string(paths, "install_root")) {
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
        
        // "trust" section
        if (j.contains("trust") && j["trust"].is_object()) {
            const auto& trust = j["trust"];
            if (auto state = get_string(trust, "state")) {
                auto parsed = parse_trust_state(*state);
                if (parsed) {
                    result.record.trust.state = *parsed;
                } else {
                    result.warnings.push_back("invalid_trust_state");
                    result.record.trust.state = TrustState::Unknown;
                }
            }
            if (auto src = get_string(trust, "source")) {
                result.record.trust.source = *src;
            }
            if (auto dt = get_string(trust, "evaluated_at")) {
                result.record.trust.evaluated_at = *dt;
            }
            if (auto dt = get_string(trust, "expires_at")) {
                result.record.trust.expires_at = *dt;
            }
            if (auto h = get_string(trust, "inputs_hash")) {
                result.record.trust.inputs_hash = *h;
            }
            
            // "details" - opaque metadata
            if (trust.contains("details") && trust["details"].is_object()) {
                for (auto& [key, val] : trust["details"].items()) {
                    if (val.is_string()) {
                        result.record.trust.details[key] = val.get<std::string>();
                    } else if (val.is_boolean()) {
                        result.record.trust.details[key] = val.get<bool>() ? "true" : "false";
                    }
                }
            }
        }
        
        // "verification" section
        if (j.contains("verification") && j["verification"].is_object()) {
            const auto& ver = j["verification"];
            if (auto dt = get_string(ver, "last_verified_at")) {
                result.record.verification.last_verified_at = *dt;
            }
            if (auto v = get_string(ver, "last_verifier_version")) {
                result.record.verification.last_verifier_version = *v;
            }
        }
        
        // "overrides" section
        if (j.contains("overrides") && j["overrides"].is_object()) {
            const auto& ovr = j["overrides"];
            
            // "environment"
            if (ovr.contains("environment") && ovr["environment"].is_object()) {
                for (auto& [key, val] : ovr["environment"].items()) {
                    if (val.is_string()) {
                        result.record.overrides.environment[key] = val.get<std::string>();
                    }
                }
            }
            
            // "arguments"
            if (ovr.contains("arguments") && ovr["arguments"].is_object()) {
                const auto& args = ovr["arguments"];
                result.record.overrides.arguments.prepend = get_string_array(args, "prepend");
                result.record.overrides.arguments.append = get_string_array(args, "append");
            }
            
            // "paths"
            if (ovr.contains("paths") && ovr["paths"].is_object()) {
                const auto& paths = ovr["paths"];
                result.record.overrides.paths.library_prepend = get_string_array(paths, "library_prepend");
            }
        }
        
        result.ok = true;
        return result;
        
    } catch (const nlohmann::json::parse_error& e) {
        result.error = std::string("parse error: ") + e.what();
        result.is_critical_error = true;
        return result;
    } catch (const nlohmann::json::exception& e) {
        result.error = std::string("JSON error: ") + e.what();
        result.is_critical_error = true;
        return result;
    }
}

bool validate_app_install_record(const AppInstallRecord& record, std::string& error) {
    if (record.schema != "nah.app.install.v2") {
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
InstallRecordValidation parse_app_install_record(const std::string& json_str, AppInstallRecord& out) {
    auto result = parse_app_install_record_full(json_str);
    if (!result.ok) {
        return {false, result.error};
    }
    out = result.record;
    return {true, {}};
}

} // namespace nah
