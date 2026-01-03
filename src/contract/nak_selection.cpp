#include "nah/nak_selection.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

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

} // namespace

NakSelectionResult select_nak_for_install(
    const Manifest& manifest,
    const HostProfile& profile,
    const std::vector<NakRegistryEntry>& registry,
    WarningCollector& warnings) {
    
    NakSelectionResult result;
    
    // Parse manifest.nak_version_req
    if (!manifest.nak_version_req.has_value()) {
        warnings.emit(Warning::invalid_manifest, {{"reason", "nak_version_req_invalid"}});
        return result;
    }
    
    const auto& req = *manifest.nak_version_req;
    
    // Filter registry records where record.nak.id == manifest.nak_id
    std::vector<const NakRegistryEntry*> candidates;
    for (const auto& entry : registry) {
        if (entry.id == manifest.nak_id) {
            candidates.push_back(&entry);
        }
    }
    
    if (candidates.empty()) {
        warnings.emit(Warning::nak_not_found, {{"nak_id", manifest.nak_id}});
        return result;
    }
    
    // Apply allow/deny rules and filter by requirement
    std::vector<const NakRegistryEntry*> valid_candidates;
    for (const auto* entry : candidates) {
        // Check allow/deny
        if (!version_allowed_by_profile(entry->version, profile)) {
            continue;
        }
        
        // Parse version and check satisfaction
        auto version = parse_version(entry->version);
        if (!version) {
            continue;  // Invalid version format
        }
        
        if (satisfies(*version, req)) {
            valid_candidates.push_back(entry);
        }
    }
    
    if (valid_candidates.empty()) {
        warnings.emit(Warning::nak_version_unsupported, 
                      {{"nak_id", manifest.nak_id}, 
                       {"nak_version_req", manifest.nak_version_req->selection_key}});
        return result;
    }
    
    // Selection based on binding mode
    const NakRegistryEntry* selected = nullptr;
    
    if (profile.nak.binding_mode == BindingMode::Mapped) {
        // Mapped mode: look up selection_key in profile.nak.map
        auto it = profile.nak.map.find(req.selection_key);
        if (it != profile.nak.map.end()) {
            // Find the entry matching the record_ref
            for (const auto* entry : valid_candidates) {
                if (entry->record_ref == it->second) {
                    selected = entry;
                    break;
                }
            }
            if (!selected) {
                // Referenced record not found or not valid
                warnings.emit(Warning::nak_version_unsupported,
                              {{"nak_id", manifest.nak_id},
                               {"reason", "mapped_record_not_found"}});
                return result;
            }
        } else {
            // No entry for selection_key
            warnings.emit(Warning::nak_version_unsupported,
                          {{"nak_id", manifest.nak_id},
                           {"selection_key", req.selection_key}});
            return result;
        }
    } else {
        // Canonical mode: choose the highest version that satisfies
        const NakRegistryEntry* highest = nullptr;
        SemVer highest_ver{0, 0, 0};
        
        for (const auto* entry : valid_candidates) {
            auto ver = parse_version(entry->version);
            if (ver && (highest == nullptr || highest_ver < *ver)) {
                highest = entry;
                highest_ver = *ver;
            }
        }
        
        selected = highest;
    }
    
    if (!selected) {
        warnings.emit(Warning::nak_version_unsupported,
                      {{"nak_id", manifest.nak_id}});
        return result;
    }
    
    result.resolved = true;
    result.pin.id = selected->id;
    result.pin.version = selected->version;
    result.pin.record_ref = selected->record_ref;
    result.selection_reason = "matched " + req.selection_key + ", allowed by profile";
    
    return result;
}

PinnedNakLoadResult load_pinned_nak(
    const NakPin& pin,
    const Manifest& manifest,
    const HostProfile& profile,
    const std::string& nah_root,
    WarningCollector& warnings) {
    
    PinnedNakLoadResult result;
    
    // Check if pin.record_ref is missing or empty
    if (pin.record_ref.empty()) {
        warnings.emit(Warning::nak_pin_invalid, {{"reason", "record_ref_empty"}});
        return result;
    }
    
    // Construct path to NAK record
    std::string record_path = nah_root + "/registry/naks/" + pin.record_ref;
    
    // Read and parse the record
    std::string toml_content = read_file(record_path);
    if (toml_content.empty()) {
        warnings.emit(Warning::nak_pin_invalid, 
                      {{"reason", "record_not_found"}, {"path", record_path}});
        return result;
    }
    
    auto parse_result = parse_nak_install_record_full(toml_content, record_path);
    if (!parse_result.ok) {
        warnings.emit(Warning::nak_pin_invalid,
                      {{"reason", "parse_error"}, {"error", parse_result.error}});
        return result;
    }
    
    const auto& nak_record = parse_result.record;
    
    // Validate schema
    if (nak_record.schema != "nah.nak.install.v1") {
        warnings.emit(Warning::nak_pin_invalid, {{"reason", "schema_mismatch"}});
        return result;
    }
    
    // Validate required fields
    std::string validation_error;
    if (!validate_nak_install_record(nak_record, validation_error)) {
        warnings.emit(Warning::nak_pin_invalid,
                      {{"reason", "validation_failed"}, {"error", validation_error}});
        return result;
    }
    
    // Validate pin.id == nak_record.nak.id == manifest.nak_id
    if (manifest.nak_id.empty()) {
        warnings.emit(Warning::invalid_manifest, {{"reason", "nak_id_missing"}});
        return result;
    }
    
    if (pin.id != nak_record.nak.id || nak_record.nak.id != manifest.nak_id) {
        warnings.emit(Warning::nak_version_unsupported,
                      {{"reason", "id_mismatch"},
                       {"pin_id", pin.id},
                       {"record_id", nak_record.nak.id},
                       {"manifest_nak_id", manifest.nak_id}});
        return result;
    }
    
    // Validate pin.version == nak_record.nak.version
    if (pin.version != nak_record.nak.version) {
        warnings.emit(Warning::nak_pin_invalid,
                      {{"reason", "version_mismatch"},
                       {"pin_version", pin.version},
                       {"record_version", nak_record.nak.version}});
        return result;
    }
    
    // Validate nak_record.nak.version is a valid core SemVer
    auto nak_ver = parse_version(nak_record.nak.version);
    if (!nak_ver) {
        warnings.emit(Warning::nak_pin_invalid,
                      {{"reason", "invalid_version"}, {"version", nak_record.nak.version}});
        return result;
    }
    
    // Parse and validate manifest.nak_version_req
    if (!manifest.nak_version_req.has_value()) {
        warnings.emit(Warning::invalid_manifest, {{"reason", "nak_version_req_invalid"}});
        return result;
    }
    
    // Check if pinned version satisfies requirement
    if (!satisfies(*nak_ver, *manifest.nak_version_req)) {
        warnings.emit(Warning::nak_version_unsupported,
                      {{"reason", "requirement_not_satisfied"},
                       {"version", nak_record.nak.version},
                       {"requirement", manifest.nak_version_req->selection_key}});
        return result;
    }
    
    // Check allow/deny rules
    if (!version_allowed_by_profile(nak_record.nak.version, profile)) {
        warnings.emit(Warning::nak_version_unsupported,
                      {{"reason", "denied_by_profile"},
                       {"version", nak_record.nak.version}});
        return result;
    }
    
    // All validations passed
    result.loaded = true;
    result.nak_record = nak_record;
    
    return result;
}

std::vector<NakRegistryEntry> scan_nak_registry(const std::string& nah_root) {
    std::vector<NakRegistryEntry> entries;
    
    std::string registry_path = nah_root + "/registry/naks";
    
    if (!fs::exists(registry_path) || !fs::is_directory(registry_path)) {
        return entries;
    }
    
    for (const auto& entry : fs::directory_iterator(registry_path)) {
        if (!entry.is_regular_file()) continue;
        
        std::string filename = entry.path().filename().string();
        if (filename.size() < 5 || filename.substr(filename.size() - 5) != ".toml") {
            continue;
        }
        
        // Parse the record to get id and version
        std::string content = read_file(entry.path().string());
        if (content.empty()) continue;
        
        auto parse_result = parse_nak_install_record_full(content, entry.path().string());
        if (!parse_result.ok) continue;
        
        NakRegistryEntry reg_entry;
        reg_entry.id = parse_result.record.nak.id;
        reg_entry.version = parse_result.record.nak.version;
        reg_entry.record_path = entry.path().string();
        reg_entry.record_ref = filename;
        
        entries.push_back(std::move(reg_entry));
    }
    
    return entries;
}

} // namespace nah
