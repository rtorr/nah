#include "nah/nahhost.hpp"
#include "nah/platform.hpp"
#include "nah/manifest.hpp"

#include <algorithm>
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

std::unique_ptr<NahHost> NahHost::create(const std::string& root_path) {
    return std::unique_ptr<NahHost>(new NahHost(root_path));
}

std::vector<AppInfo> NahHost::listApplications() const {
    std::vector<AppInfo> apps;
    
    fs::path registry_path = fs::path(root_) / "registry" / "installs";
    
    if (!fs::exists(registry_path) || !fs::is_directory(registry_path)) {
        return apps;
    }
    
    for (const auto& entry : fs::directory_iterator(registry_path)) {
        if (!entry.is_regular_file()) continue;
        
        std::string filename = entry.path().filename().string();
        if (filename.size() < 5 || filename.substr(filename.size() - 5) != ".json") {
            continue;
        }
        
        std::string content = read_file(entry.path().string());
        if (content.empty()) continue;
        
        auto parse_result = parse_app_install_record_full(content, entry.path().string());
        if (!parse_result.ok) continue;
        
        AppInfo info;
        info.id = parse_result.record.app.id;
        info.version = parse_result.record.app.version;
        info.instance_id = parse_result.record.install.instance_id;
        info.install_root = parse_result.record.paths.install_root;
        info.record_path = entry.path().string();
        
        apps.push_back(std::move(info));
    }
    
    return apps;
}

Result<AppInfo> NahHost::findApplication(const std::string& id,
                                          const std::string& version) const {
    auto apps = listApplications();
    
    std::vector<AppInfo> matches;
    for (const auto& app : apps) {
        if (app.id == id) {
            if (version.empty() || app.version == version) {
                matches.push_back(app);
            }
        }
    }
    
    if (matches.empty()) {
        return Result<AppInfo>::err(Error(ErrorCode::FILE_NOT_FOUND, 
                                          "application not found: " + id));
    }
    
    if (matches.size() > 1 && version.empty()) {
        std::string versions;
        for (const auto& m : matches) {
            if (!versions.empty()) versions += ", ";
            versions += m.version;
        }
        return Result<AppInfo>::err(Error(ErrorCode::FILE_NOT_FOUND,
                                          "multiple versions installed: " + versions));
    }
    
    return Result<AppInfo>::ok(matches[0]);
}

Result<HostProfile> NahHost::getActiveHostProfile() const {
    return resolveActiveProfile();
}

Result<void> NahHost::setActiveHostProfile(const std::string& name) {
    fs::path link_path = fs::path(root_) / "host" / "profile.current";
    std::string target = "profiles/" + name + ".json";
    fs::path profile_path = fs::path(root_) / "host" / "profiles" / (name + ".json");
    
    // Verify profile exists
    if (!fs::exists(profile_path)) {
        return Result<void>::err(Error(ErrorCode::PROFILE_MISSING,
                                       "profile not found: " + name));
    }
    
    // Create parent directory if needed
    create_directories((fs::path(root_) / "host").string());
    
    // Update symlink atomically
    auto result = atomic_update_symlink(link_path.string(), target);
    if (!result.ok) {
        return Result<void>::err(Error(ErrorCode::IO_ERROR, result.error));
    }
    
    return Result<void>::ok();
}

std::vector<std::string> NahHost::listProfiles() const {
    std::vector<std::string> profiles;
    
    fs::path profiles_path = fs::path(root_) / "host" / "profiles";
    
    if (!fs::exists(profiles_path) || !fs::is_directory(profiles_path)) {
        return profiles;
    }
    
    for (const auto& entry : fs::directory_iterator(profiles_path)) {
        if (!entry.is_regular_file()) continue;
        
        std::string filename = entry.path().filename().string();
        if (filename.size() > 5 && filename.substr(filename.size() - 5) == ".json") {
            profiles.push_back(filename.substr(0, filename.size() - 5));
        }
    }
    
    std::sort(profiles.begin(), profiles.end());
    return profiles;
}

Result<HostProfile> NahHost::loadProfile(const std::string& name) const {
    fs::path profile_path = fs::path(root_) / "host" / "profiles" / (name + ".json");
    
    std::string content = read_file(profile_path.string());
    if (content.empty()) {
        return Result<HostProfile>::err(Error(ErrorCode::PROFILE_MISSING,
                                              "profile not found: " + name));
    }
    
    auto parse_result = parse_host_profile_full(content, profile_path.string());
    if (!parse_result.ok) {
        return Result<HostProfile>::err(Error(ErrorCode::PROFILE_PARSE_ERROR,
                                              parse_result.error));
    }
    
    return Result<HostProfile>::ok(parse_result.profile);
}

Result<void> NahHost::validateProfile(const HostProfile& profile) const {
    if (profile.schema != "nah.host.profile.v2") {
        return Result<void>::err(Error(ErrorCode::PROFILE_PARSE_ERROR,
                                       "invalid schema"));
    }
    return Result<void>::ok();
}

Result<HostProfile> NahHost::resolveActiveProfile(const std::string& explicit_name) const {
    // Per SPEC Active Host Profile Resolution (L597-L614)
    
    // 1. If explicit name provided, load that profile
    if (!explicit_name.empty()) {
        auto result = loadProfile(explicit_name);
        if (result.isOk()) {
            return result;
        }
        // Fall back to default.json on failure
    }
    
    // 2. Check profile.current symlink
    fs::path link_path = fs::path(root_) / "host" / "profile.current";
    if (fs::exists(link_path)) {
        if (!fs::is_symlink(link_path)) {
            // profile.current exists but is not a symlink - emit profile_invalid, fall back
        } else {
            auto target = read_symlink(link_path.string());
            if (target) {
                // Resolve relative to profiles directory
                fs::path profile_path = fs::path(root_) / "host" / *target;
                std::string content = read_file(profile_path.string());
                
                if (!content.empty()) {
                    auto parse_result = parse_host_profile_full(content, profile_path.string());
                    if (parse_result.ok && parse_result.profile.schema == "nah.host.profile.v2") {
                        return Result<HostProfile>::ok(parse_result.profile);
                    }
                }
            }
        }
    }
    
    // 3. Fall back to default.json
    fs::path default_path = fs::path(root_) / "host" / "profiles" / "default.json";
    std::string content = read_file(default_path.string());
    
    if (!content.empty()) {
        auto parse_result = parse_host_profile_full(content, default_path.string());
        if (parse_result.ok && parse_result.profile.schema == "nah.host.profile.v2") {
            return Result<HostProfile>::ok(parse_result.profile);
        }
    }
    
    // 4. Use built-in empty profile
    return Result<HostProfile>::ok(get_builtin_empty_profile());
}

Result<ContractEnvelope> NahHost::getLaunchContract(
    const std::string& app_id,
    const std::string& version,
    const std::string& profile_name,
    bool enable_trace) const {
    
    // Find the application
    auto app_result = findApplication(app_id, version);
    if (app_result.isErr()) {
        return Result<ContractEnvelope>::err(app_result.error());
    }
    
    const auto& app_info = app_result.value();
    
    // Load install record
    std::string record_content = read_file(app_info.record_path);
    if (record_content.empty()) {
        return Result<ContractEnvelope>::err(
            Error(ErrorCode::INSTALL_RECORD_INVALID, "failed to read install record"));
    }
    
    auto record_result = parse_app_install_record_full(record_content, app_info.record_path);
    if (!record_result.ok) {
        return Result<ContractEnvelope>::err(
            Error(ErrorCode::INSTALL_RECORD_INVALID, record_result.error));
    }
    
    // Load manifest from app
    fs::path manifest_path = fs::path(app_info.install_root) / "manifest.nah";
    Manifest manifest;
    bool manifest_loaded = false;
    
    // Try manifest.nah first
    if (fs::exists(manifest_path)) {
        std::ifstream file(manifest_path, std::ios::binary);
        if (file) {
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
            auto parse_result = parse_manifest(data);
            if (!parse_result.critical_missing) {
                manifest = parse_result.manifest;
                manifest_loaded = true;
            }
        }
    }
    
    // Try to find binary with embedded manifest
    if (!manifest_loaded) {
        fs::path bin_dir = fs::path(app_info.install_root) / "bin";
        if (fs::exists(bin_dir) && fs::is_directory(bin_dir)) {
            for (const auto& entry : fs::directory_iterator(bin_dir)) {
                if (!entry.is_regular_file()) continue;
                
                auto section_result = read_manifest_section(entry.path().string());
                if (section_result.ok) {
                    auto parse_result = parse_manifest(section_result.data);
                    if (!parse_result.critical_missing) {
                        manifest = parse_result.manifest;
                        manifest_loaded = true;
                        break;
                    }
                }
            }
        }
    }
    
    if (!manifest_loaded) {
        return Result<ContractEnvelope>::err(
            Error(ErrorCode::MANIFEST_MISSING, "no manifest found"));
    }
    
    // Resolve profile
    auto profile_result = resolveActiveProfile(profile_name);
    if (profile_result.isErr()) {
        // Use built-in empty profile on failure
    }
    HostProfile profile = profile_result.isOk() ? profile_result.value() : get_builtin_empty_profile();
    
    // Build composition inputs
    CompositionInputs inputs;
    inputs.nah_root = root_;
    inputs.manifest = manifest;
    inputs.install_record = record_result.record;
    inputs.profile = profile;
    inputs.process_env = get_all_env();
    inputs.now = get_current_timestamp();
    inputs.enable_trace = enable_trace;
    
    // Compose contract
    auto compose_result = compose_contract(inputs);
    
    if (!compose_result.ok) {
        ErrorCode code;
        switch (*compose_result.critical_error) {
            case CriticalError::MANIFEST_MISSING:
                code = ErrorCode::MANIFEST_MISSING;
                break;
            case CriticalError::ENTRYPOINT_NOT_FOUND:
                code = ErrorCode::ENTRYPOINT_NOT_FOUND;
                break;
            case CriticalError::PATH_TRAVERSAL:
                code = ErrorCode::PATH_TRAVERSAL;
                break;
            case CriticalError::INSTALL_RECORD_INVALID:
                code = ErrorCode::INSTALL_RECORD_INVALID;
                break;
        }
        return Result<ContractEnvelope>::err(
            Error(code, critical_error_to_string(*compose_result.critical_error)));
    }
    
    return Result<ContractEnvelope>::ok(compose_result.envelope);
}

Result<ContractEnvelope> NahHost::composeContract(const CompositionInputs& inputs) const {
    auto result = compose_contract(inputs);
    
    if (!result.ok) {
        ErrorCode code;
        switch (*result.critical_error) {
            case CriticalError::MANIFEST_MISSING:
                code = ErrorCode::MANIFEST_MISSING;
                break;
            case CriticalError::ENTRYPOINT_NOT_FOUND:
                code = ErrorCode::ENTRYPOINT_NOT_FOUND;
                break;
            case CriticalError::PATH_TRAVERSAL:
                code = ErrorCode::PATH_TRAVERSAL;
                break;
            case CriticalError::INSTALL_RECORD_INVALID:
                code = ErrorCode::INSTALL_RECORD_INVALID;
                break;
        }
        return Result<ContractEnvelope>::err(
            Error(code, critical_error_to_string(*result.critical_error)));
    }
    
    return Result<ContractEnvelope>::ok(result.envelope);
}

} // namespace nah
