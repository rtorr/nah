#include "nah/compose.hpp"
#include "nah/packaging.hpp"
#include "nah/platform.hpp"
#include "nah/nak_record.hpp"
#include "nah/nak_selection.hpp"
#include "nah/materializer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace nah {

// ============================================================================
// Helper Functions
// ============================================================================

std::string conflict_strategy_to_string(ConflictStrategy strategy) {
    switch (strategy) {
        case ConflictStrategy::Error: return "error";
        case ConflictStrategy::First: return "first";
        case ConflictStrategy::Last: return "last";
        default: return "error";
    }
}

ConflictStrategy parse_conflict_strategy(const std::string& str) {
    if (str == "first") return ConflictStrategy::First;
    if (str == "last") return ConflictStrategy::Last;
    return ConflictStrategy::Error;
}

std::string source_type_to_string(ComposeSourceType type) {
    switch (type) {
        case ComposeSourceType::Installed: return "installed";
        case ComposeSourceType::NakFile: return "file";
        case ComposeSourceType::Directory: return "directory";
        default: return "installed";
    }
}

ComposeSourceType parse_source_type(const std::string& str) {
    if (str == "file") return ComposeSourceType::NakFile;
    if (str == "directory") return ComposeSourceType::Directory;
    return ComposeSourceType::Installed;
}

std::string compute_file_sha256(const std::string& path) {
    auto result = compute_sha256(path);
    return result.ok ? result.hex_digest : "";
}

// ============================================================================
// NAK Resolution
// ============================================================================

ComposeInput resolve_compose_input(const std::string& ref,
                                   const std::string& nah_root,
                                   std::string& error) {
    ComposeInput input;
    
    // Check if it's a file path
    if (path_exists(ref)) {
        if (is_directory(ref)) {
            // Directory - read META/nak.json directly
            std::string nak_json_path = join_path(ref, "META/nak.json");
            if (!path_exists(nak_json_path)) {
                error = "directory does not contain META/nak.json: " + ref;
                return input;
            }
            
            std::ifstream file(nak_json_path);
            if (!file) {
                error = "failed to read META/nak.json in: " + ref;
                return input;
            }
            
            std::stringstream ss;
            ss << file.rdbuf();
            auto parse_result = parse_nak_pack_manifest(ss.str());
            if (!parse_result.ok) {
                error = "invalid META/nak.json: " + parse_result.error;
                return input;
            }
            
            input.id = parse_result.manifest.nak.id;
            input.version = parse_result.manifest.nak.version;
            input.source = fs::absolute(ref).string();
            input.source_type = ComposeSourceType::Directory;
            input.root_path = fs::absolute(ref).string();
            
            // Convert NakPackManifest to NakPackInfo
            input.pack_info.ok = true;
            input.pack_info.nak_id = parse_result.manifest.nak.id;
            input.pack_info.nak_version = parse_result.manifest.nak.version;
            input.pack_info.resource_root = parse_result.manifest.paths.resource_root;
            input.pack_info.lib_dirs = parse_result.manifest.paths.lib_dirs;
            input.pack_info.environment = parse_result.manifest.environment;
            input.pack_info.loaders = parse_result.manifest.loaders;
            input.pack_info.execution_cwd = parse_result.manifest.execution.cwd;
            
            return input;
        } else {
            // .nak file - inspect it
            auto pack_info = inspect_nak_pack(ref);
            if (!pack_info.ok) {
                error = "failed to inspect NAK pack: " + pack_info.error;
                return input;
            }
            
            input.id = pack_info.nak_id;
            input.version = pack_info.nak_version;
            input.source = fs::absolute(ref).string();
            input.source_type = ComposeSourceType::NakFile;
            input.pack_info = pack_info;
            
            // Compute hash of the .nak file for integrity
            input.content_hash = compute_file_sha256(ref);
            
            // Note: root_path will be set after extraction during composition
            
            return input;
        }
    }
    
    // Parse as nak_id[@version]
    std::string id, version;
    size_t at_pos = ref.find('@');
    if (at_pos != std::string::npos) {
        id = ref.substr(0, at_pos);
        version = ref.substr(at_pos + 1);
    } else {
        id = ref;
    }
    
    // Scan NAK registry
    auto entries = scan_nak_registry(nah_root);
    
    // Find matching NAK
    std::vector<NakRegistryEntry> matches;
    for (const auto& entry : entries) {
        if (entry.id == id) {
            if (version.empty() || entry.version == version ||
                entry.version.find(version) == 0) {  // Version prefix match
                matches.push_back(entry);
            }
        }
    }
    
    if (matches.empty()) {
        error = "NAK not found: " + ref;
        return input;
    }
    
    // If multiple matches, use the latest (last in sorted order)
    // Sort by version in case version wasn't specified
    std::sort(matches.begin(), matches.end(), 
        [](const NakRegistryEntry& a, const NakRegistryEntry& b) {
            return a.version < b.version;
        });
    
    const auto& match = matches.back();
    
    // Read the NAK install record
    std::ifstream record_file(match.record_path);
    if (!record_file) {
        error = "failed to read NAK install record: " + match.record_path;
        return input;
    }
    
    std::stringstream ss;
    ss << record_file.rdbuf();
    auto record_result = parse_nak_install_record_full(ss.str(), match.record_path);
    if (!record_result.ok) {
        error = "failed to parse NAK install record: " + record_result.error;
        return input;
    }
    
    input.id = match.id;
    input.version = match.version;
    input.source = match.id + "@" + match.version;
    input.source_type = ComposeSourceType::Installed;
    input.root_path = record_result.record.paths.root;
    
    // Use package hash from provenance if available
    if (!record_result.record.provenance.package_hash.empty()) {
        input.content_hash = record_result.record.provenance.package_hash;
    }
    
    // Convert NakInstallRecord to NakPackInfo (for consistency)
    input.pack_info.ok = true;
    input.pack_info.nak_id = record_result.record.nak.id;
    input.pack_info.nak_version = record_result.record.nak.version;
    
    // For installed NAKs, paths are absolute - convert to relative for pack_info
    // The root_path is stored separately
    if (!record_result.record.paths.resource_root.empty()) {
        // Extract relative path from absolute
        std::string root = record_result.record.paths.root;
        std::string res = record_result.record.paths.resource_root;
        if (res.find(root) == 0 && res.size() > root.size()) {
            input.pack_info.resource_root = res.substr(root.size() + 1);
        } else if (res != root) {
            input.pack_info.resource_root = res;
        }
        // If res == root, leave resource_root empty
    }
    
    for (const auto& lib_dir : record_result.record.paths.lib_dirs) {
        std::string root = record_result.record.paths.root;
        if (lib_dir.find(root) == 0 && lib_dir.size() > root.size()) {
            input.pack_info.lib_dirs.push_back(lib_dir.substr(root.size() + 1));
        } else if (lib_dir != root) {
            input.pack_info.lib_dirs.push_back(lib_dir);
        }
        // If lib_dir == root, skip (invalid state)
    }
    
    input.pack_info.environment = record_result.record.environment;
    
    // Convert loaders - paths are absolute in install record
    for (const auto& [name, loader] : record_result.record.loaders) {
        LoaderConfig relative_loader;
        std::string root = record_result.record.paths.root;
        if (loader.exec_path.find(root) == 0 && loader.exec_path.size() > root.size()) {
            relative_loader.exec_path = loader.exec_path.substr(root.size() + 1);
        } else if (loader.exec_path != root) {
            relative_loader.exec_path = loader.exec_path;
        } else {
            // exec_path equals root - this is invalid, use original path
            relative_loader.exec_path = loader.exec_path;
        }
        relative_loader.args_template = loader.args_template;
        input.pack_info.loaders[name] = relative_loader;
    }
    
    input.pack_info.execution_cwd = record_result.record.execution.cwd;
    
    return input;
}

// ============================================================================
// File Tree Collection
// ============================================================================

struct FileEntry {
    std::string relative_path;
    std::string absolute_path;
    std::string source_nak;  // NAK ID for provenance
    bool is_directory = false;
    bool is_executable = false;
};

static std::vector<FileEntry> collect_files(const std::string& root_path,
                                            const std::string& source_nak) {
    std::vector<FileEntry> entries;
    
    if (!is_directory(root_path)) {
        return entries;
    }
    
    std::function<void(const std::string&, const std::string&)> collect_recursive;
    collect_recursive = [&](const std::string& dir, const std::string& prefix) {
        for (const auto& name : list_directory(dir)) {
            std::string full_path = join_path(dir, name);
            std::string rel_path = prefix.empty() ? name : (prefix + "/" + name);
            
            // Skip META/nak.json - we generate a new one during composition
            if (rel_path == "META/nak.json") {
                continue;
            }
            
            FileEntry entry;
            entry.relative_path = rel_path;
            entry.absolute_path = full_path;
            entry.source_nak = source_nak;
            
            if (is_directory(full_path)) {
                entry.is_directory = true;
                entries.push_back(entry);
                collect_recursive(full_path, rel_path);
            } else if (is_regular_file(full_path)) {
                // Check if executable
                fs::perms perms = fs::status(full_path).permissions();
                entry.is_executable = (perms & fs::perms::owner_exec) != fs::perms::none;
                entries.push_back(entry);
            }
            // Skip symlinks per SPEC
        }
    };
    
    collect_recursive(root_path, "");
    return entries;
}

// ============================================================================
// Environment Merging
// ============================================================================

static EnvMap merge_environments(const std::vector<ComposeInput>& inputs,
                                 const std::vector<std::pair<std::string, std::string>>& add_env) {
    EnvMap result;
    
    // Apply each input's environment in order
    for (const auto& input : inputs) {
        for (const auto& [key, value] : input.pack_info.environment) {
            auto it = result.find(key);
            
            if (it == result.end()) {
                // First occurrence - use as-is
                result[key] = value;
            } else if (value.op == EnvOp::Set) {
                // Set operations: last wins
                result[key] = value;
            } else if (value.op == EnvOp::Prepend) {
                // Prepend: accumulate prepends
                if (it->second.op == EnvOp::Prepend) {
                    // Combine: existing prepend + new prepend
                    it->second.value = value.value + value.separator + it->second.value;
                } else if (it->second.op == EnvOp::Set) {
                    // Convert to prepend with combined value
                    EnvValue new_val;
                    new_val.op = EnvOp::Prepend;
                    new_val.value = value.value;
                    new_val.separator = value.separator;
                    result[key] = new_val;
                } else {
                    result[key] = value;
                }
            } else if (value.op == EnvOp::Append) {
                // Append: accumulate appends
                if (it->second.op == EnvOp::Append) {
                    // Combine: existing append + new append
                    it->second.value = it->second.value + value.separator + value.value;
                } else if (it->second.op == EnvOp::Set) {
                    // Convert to append with combined value
                    EnvValue new_val;
                    new_val.op = EnvOp::Append;
                    new_val.value = value.value;
                    new_val.separator = value.separator;
                    result[key] = new_val;
                } else {
                    result[key] = value;
                }
            } else if (value.op == EnvOp::Unset) {
                // Unset removes the variable
                result.erase(key);
            }
        }
    }
    
    // Apply --add-env overrides (set semantics)
    for (const auto& [key, val] : add_env) {
        result[key] = EnvValue(val);
    }
    
    return result;
}

// ============================================================================
// Lib Dirs Merging
// ============================================================================

static std::vector<std::string> merge_lib_dirs(const std::vector<ComposeInput>& inputs,
                                                const std::vector<std::string>& add_lib_dirs) {
    std::vector<std::string> result;
    std::set<std::string> seen;
    
    // Concatenate in input order, removing duplicates
    for (const auto& input : inputs) {
        for (const auto& lib_dir : input.pack_info.lib_dirs) {
            if (seen.insert(lib_dir).second) {
                result.push_back(lib_dir);
            }
        }
    }
    
    // Add extra lib_dirs
    for (const auto& lib_dir : add_lib_dirs) {
        if (seen.insert(lib_dir).second) {
            result.push_back(lib_dir);
        }
    }
    
    return result;
}

// ============================================================================
// Loader Selection
// ============================================================================

struct LoaderSelection {
    bool ok = false;
    std::string error;
    std::unordered_map<std::string, LoaderConfig> loaders;
    std::string source_nak;  // Which NAK the loaders came from
};

static LoaderSelection select_loaders(const std::vector<ComposeInput>& inputs,
                                       const std::optional<std::string>& loader_from) {
    LoaderSelection result;
    
    // Find all inputs with loaders
    std::vector<const ComposeInput*> with_loaders;
    for (const auto& input : inputs) {
        if (!input.pack_info.loaders.empty()) {
            with_loaders.push_back(&input);
        }
    }
    
    if (with_loaders.empty()) {
        // No loaders - libs-only NAK
        result.ok = true;
        return result;
    }
    
    if (with_loaders.size() == 1) {
        // Single NAK with loaders - use it
        result.ok = true;
        result.loaders = with_loaders[0]->pack_info.loaders;
        result.source_nak = with_loaders[0]->id;
        return result;
    }
    
    // Multiple NAKs have loaders - require --loader-from
    if (!loader_from.has_value()) {
        std::string error_msg = "Multiple NAKs define loaders:\n";
        for (const auto* input : with_loaders) {
            error_msg += "  " + input->id + " defines: ";
            bool first = true;
            for (const auto& [name, _] : input->pack_info.loaders) {
                if (!first) error_msg += ", ";
                error_msg += name;
                first = false;
            }
            error_msg += "\n";
        }
        error_msg += "\nUse --loader-from=<nak-id> to specify which NAK's loaders to use.";
        result.error = error_msg;
        return result;
    }
    
    // Find the specified NAK
    for (const auto* input : with_loaders) {
        if (input->id == loader_from.value()) {
            result.ok = true;
            result.loaders = input->pack_info.loaders;
            result.source_nak = input->id;
            return result;
        }
    }
    
    result.error = "NAK specified by --loader-from not found: " + loader_from.value();
    return result;
}

// ============================================================================
// NAK JSON Generation
// ============================================================================

static std::string generate_nak_json(const std::string& id,
                                      const std::string& version,
                                      const std::vector<std::string>& lib_dirs,
                                      const std::string& resource_root,
                                      const EnvMap& environment,
                                      const std::unordered_map<std::string, LoaderConfig>& loaders,
                                      const std::string& execution_cwd,
                                      bool include_provenance,
                                      const std::vector<ComposeInput>& sources) {
    json j;
    j["$schema"] = "nah.nak.pack.v2";
    
    j["nak"]["id"] = id;
    j["nak"]["version"] = version;
    
    // Paths
    if (!lib_dirs.empty()) {
        j["paths"]["lib_dirs"] = lib_dirs;
    }
    if (!resource_root.empty()) {
        j["paths"]["resource_root"] = resource_root;
    }
    
    // Environment
    if (!environment.empty()) {
        json env_json = json::object();
        for (const auto& [key, value] : environment) {
            if (value.is_simple()) {
                env_json[key] = value.value;
            } else {
                json op_json;
                op_json["op"] = env_op_to_string(value.op);
                op_json["value"] = value.value;
                if (value.op == EnvOp::Prepend || value.op == EnvOp::Append) {
                    op_json["separator"] = value.separator;
                }
                env_json[key] = op_json;
            }
        }
        j["environment"] = env_json;
    }
    
    // Loaders
    if (!loaders.empty()) {
        if (loaders.size() == 1 && loaders.count("default")) {
            // Singular format
            const auto& loader = loaders.at("default");
            j["loader"]["exec_path"] = loader.exec_path;
            if (!loader.args_template.empty()) {
                j["loader"]["args_template"] = loader.args_template;
            }
        } else {
            // Named loaders format
            json loaders_json = json::object();
            for (const auto& [name, loader] : loaders) {
                json loader_json;
                loader_json["exec_path"] = loader.exec_path;
                if (!loader.args_template.empty()) {
                    loader_json["args_template"] = loader.args_template;
                }
                loaders_json[name] = loader_json;
            }
            j["loaders"] = loaders_json;
        }
    }
    
    // Execution
    if (!execution_cwd.empty()) {
        j["execution"]["cwd"] = execution_cwd;
    }
    
    // Provenance
    if (include_provenance) {
        json sources_json = json::array();
        for (const auto& src : sources) {
            json src_json;
            src_json["id"] = src.id;
            src_json["version"] = src.version;
            sources_json.push_back(src_json);
        }
        j["provenance"]["composed"] = true;
        j["provenance"]["sources"] = sources_json;
        j["provenance"]["composed_at"] = get_current_timestamp();
        j["provenance"]["tool"] = "nah nak compose";
    }
    
    return j.dump(2);
}

// ============================================================================
// Main Composition Function
// ============================================================================

ComposeResult compose_naks(const std::vector<std::string>& input_refs,
                           const ComposeOptions& options) {
    ComposeResult result;
    
    if (input_refs.empty()) {
        result.error = "at least one input NAK is required";
        return result;
    }
    
    if (options.output_id.empty()) {
        result.error = "--id is required";
        return result;
    }
    
    if (options.output_version.empty()) {
        result.error = "--version is required";
        return result;
    }
    
    if (options.output_path.empty()) {
        result.error = "-o/--output is required";
        return result;
    }
    
    // 1. Resolve all input NAKs
    std::vector<ComposeInput> inputs;
    for (const auto& ref : input_refs) {
        std::string error;
        auto input = resolve_compose_input(ref, options.nah_root, error);
        if (!error.empty()) {
            result.error = error;
            return result;
        }
        
        // For .nak files, we need to extract them to get file access
        if (input.root_path.empty() && path_exists(ref) && !is_directory(ref)) {
            // Create temp directory and extract
            std::string temp_dir = join_path(
                fs::temp_directory_path().string(),
                "nah-compose-" + generate_uuid());
            
            if (!create_directories(temp_dir)) {
                result.error = "failed to create temp directory for extraction";
                return result;
            }
            
            // Read .nak file
            std::ifstream file(ref, std::ios::binary);
            std::vector<uint8_t> archive_data(
                (std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());
            
            auto extract_result = extract_archive_safe(archive_data, temp_dir);
            if (!extract_result.ok) {
                remove_directory(temp_dir);
                result.error = "failed to extract NAK: " + extract_result.error;
                return result;
            }
            
            input.root_path = temp_dir;
            // Note: temp directory cleanup would need to be handled properly
            // For now, we'll leave cleanup to the OS temp directory mechanism
        }
        
        if (options.verbose) {
            // Collect for verbose output
        }
        
        inputs.push_back(input);
    }
    
    result.sources = inputs;
    
    // 2. Collect files from all inputs
    std::map<std::string, FileEntry> file_map;  // relative_path -> entry
    std::map<std::string, std::string> file_hashes;  // relative_path -> sha256
    
    for (const auto& input : inputs) {
        auto files = collect_files(input.root_path, input.id);
        
        for (const auto& file : files) {
            auto it = file_map.find(file.relative_path);
            if (it != file_map.end()) {
                // Potential conflict
                if (!file.is_directory) {
                    // Compute hashes to check if content differs
                    std::string hash_a = file_hashes[file.relative_path];
                    if (hash_a.empty()) {
                        hash_a = compute_file_sha256(it->second.absolute_path);
                        file_hashes[file.relative_path] = hash_a;
                    }
                    std::string hash_b = compute_file_sha256(file.absolute_path);
                    
                    if (hash_a != hash_b) {
                        // Real conflict
                        FileConflict conflict;
                        conflict.relative_path = file.relative_path;
                        conflict.source_a = it->second.source_nak;
                        conflict.source_b = file.source_nak;
                        conflict.hash_a = hash_a;
                        conflict.hash_b = hash_b;
                        result.conflicts.push_back(conflict);
                        
                        // Handle based on strategy
                        switch (options.on_conflict) {
                            case ConflictStrategy::Error:
                                // Will report error later
                                break;
                            case ConflictStrategy::First:
                                // Keep first (already in map)
                                break;
                            case ConflictStrategy::Last:
                                // Replace with new
                                file_map[file.relative_path] = file;
                                file_hashes[file.relative_path] = hash_b;
                                break;
                        }
                    }
                    // Same hash = deduplicate (keep first)
                }
            } else {
                file_map[file.relative_path] = file;
            }
        }
    }
    
    // Check for unresolved conflicts
    if (!result.conflicts.empty() && options.on_conflict == ConflictStrategy::Error) {
        std::string error_msg = "File conflicts detected:\n\n";
        for (const auto& conflict : result.conflicts) {
            error_msg += "  " + conflict.relative_path + "\n";
            error_msg += "    " + conflict.source_a + ": " + conflict.hash_a.substr(0, 12) + "...\n";
            error_msg += "    " + conflict.source_b + ": " + conflict.hash_b.substr(0, 12) + "...\n\n";
        }
        error_msg += "Resolution options:\n";
        error_msg += "  --on-conflict=first    Use file from first NAK\n";
        error_msg += "  --on-conflict=last     Use file from last NAK\n";
        result.error = error_msg;
        return result;
    }
    
    // 3. Merge lib_dirs
    result.lib_dirs = merge_lib_dirs(inputs, options.add_lib_dirs);
    
    // 4. Merge environments
    result.merged_environment = merge_environments(inputs, options.add_env);
    
    // 5. Select loaders
    auto loader_result = select_loaders(inputs, options.loader_from);
    if (!loader_result.ok) {
        result.error = loader_result.error;
        return result;
    }
    result.selected_loader_from = loader_result.source_nak.empty() ? 
        std::nullopt : std::make_optional(loader_result.source_nak);
    
    // 6. Determine resource_root
    std::string resource_root;
    std::set<std::string> resource_roots;
    for (const auto& input : inputs) {
        if (!input.pack_info.resource_root.empty()) {
            resource_roots.insert(input.pack_info.resource_root);
        }
    }
    
    if (options.resource_root.has_value()) {
        resource_root = options.resource_root.value();
    } else if (resource_roots.size() == 1) {
        resource_root = *resource_roots.begin();
    } else if (resource_roots.size() > 1) {
        result.error = "Multiple inputs have different resource_root values. "
                       "Use --resource-root to specify which to use.";
        return result;
    }
    
    // 7. Determine execution.cwd
    std::string execution_cwd;
    if (!loader_result.source_nak.empty()) {
        for (const auto& input : inputs) {
            if (input.id == loader_result.source_nak) {
                execution_cwd = input.pack_info.execution_cwd;
                break;
            }
        }
    }
    
    // Populate files to copy for dry-run
    for (const auto& [path, entry] : file_map) {
        result.files_to_copy.push_back(path);
    }
    std::sort(result.files_to_copy.begin(), result.files_to_copy.end());
    
    // 8. If dry-run, stop here
    if (options.dry_run) {
        result.ok = true;
        result.nak_id = options.output_id;
        result.nak_version = options.output_version;
        return result;
    }
    
    // 9. Create output directory
    std::string output_dir;
    bool pack_as_nak = false;
    
    if (options.output_path.size() > 4 && 
        options.output_path.substr(options.output_path.size() - 4) == ".nak") {
        // Output to .nak file - create temp staging directory
        output_dir = join_path(
            fs::temp_directory_path().string(),
            "nah-compose-out-" + generate_uuid());
        pack_as_nak = true;
    } else {
        // Output to directory
        output_dir = options.output_path;
    }
    
    if (!create_directories(output_dir)) {
        result.error = "failed to create output directory: " + output_dir;
        return result;
    }
    
    // Create META directory
    std::string meta_dir = join_path(output_dir, "META");
    if (!create_directories(meta_dir)) {
        result.error = "failed to create META directory";
        return result;
    }
    
    // 10. Copy files
    for (const auto& [rel_path, entry] : file_map) {
        std::string dest_path = join_path(output_dir, rel_path);
        
        if (entry.is_directory) {
            if (!create_directories(dest_path)) {
                result.error = "failed to create directory: " + rel_path;
                return result;
            }
        } else {
            // Ensure parent directory exists
            std::string parent = get_parent_directory(dest_path);
            if (!parent.empty() && !path_exists(parent)) {
                if (!create_directories(parent)) {
                    result.error = "failed to create parent directory for: " + rel_path;
                    return result;
                }
            }
            
            if (!copy_file(entry.absolute_path, dest_path)) {
                result.error = "failed to copy file: " + rel_path;
                return result;
            }
            
            // Preserve executable permission
            if (entry.is_executable) {
                fs::permissions(dest_path, 
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);
            }
        }
    }
    
    // 11. Generate META/nak.json
    std::string nak_json = generate_nak_json(
        options.output_id,
        options.output_version,
        result.lib_dirs,
        resource_root,
        result.merged_environment,
        loader_result.loaders,
        execution_cwd,
        options.include_provenance,
        inputs);
    
    std::string nak_json_path = join_path(meta_dir, "nak.json");
    auto write_result = atomic_write_file(nak_json_path, nak_json);
    if (!write_result.ok) {
        result.error = "failed to write META/nak.json: " + write_result.error;
        return result;
    }
    
    // 12. Pack as .nak if needed
    if (pack_as_nak) {
        auto pack_result = pack_nak(output_dir);
        if (!pack_result.ok) {
            remove_directory(output_dir);
            result.error = "failed to pack NAK: " + pack_result.error;
            return result;
        }
        
        // Write the .nak file
        std::ofstream out_file(options.output_path, std::ios::binary);
        if (!out_file) {
            remove_directory(output_dir);
            result.error = "failed to create output file: " + options.output_path;
            return result;
        }
        
        out_file.write(reinterpret_cast<const char*>(pack_result.archive_data.data()),
                       static_cast<std::streamsize>(pack_result.archive_data.size()));
        out_file.close();
        
        // Clean up staging directory
        remove_directory(output_dir);
        
        result.output_path = options.output_path;
    } else {
        result.output_path = output_dir;
    }
    
    // 13. Emit manifest if requested
    if (options.emit_manifest.has_value()) {
        json manifest;
        manifest["$schema"] = "nah.nak.compose.v1";
        manifest["output"]["id"] = options.output_id;
        manifest["output"]["version"] = options.output_version;
        
        json inputs_json = json::array();
        for (const auto& input : inputs) {
            json inp;
            inp["id"] = input.id;
            inp["version"] = input.version;
            inp["source_type"] = source_type_to_string(input.source_type);
            inp["source"] = input.source;
            if (!input.content_hash.empty()) {
                inp["sha256"] = input.content_hash;
            }
            inputs_json.push_back(inp);
        }
        manifest["inputs"] = inputs_json;
        
        manifest["options"]["on_conflict"] = conflict_strategy_to_string(options.on_conflict);
        if (options.loader_from.has_value()) {
            manifest["options"]["loader_from"] = options.loader_from.value();
        }
        
        if (!options.add_env.empty()) {
            json env_json = json::object();
            for (const auto& [k, v] : options.add_env) {
                env_json[k] = v;
            }
            manifest["overrides"]["environment"] = env_json;
        }
        
        if (!options.add_lib_dirs.empty()) {
            manifest["overrides"]["lib_dirs_append"] = options.add_lib_dirs;
        }
        
        std::ofstream manifest_file(options.emit_manifest.value());
        if (manifest_file) {
            manifest_file << manifest.dump(2) << std::endl;
        }
    }
    
    result.ok = true;
    result.nak_id = options.output_id;
    result.nak_version = options.output_version;
    
    return result;
}

// ============================================================================
// Manifest Parsing
// ============================================================================

ComposeManifestParseResult parse_compose_manifest(const std::string& json_str) {
    ComposeManifestParseResult result;
    
    try {
        json j = json::parse(json_str);
        
        if (!j.contains("output") || !j["output"].contains("id") || 
            !j["output"].contains("version")) {
            result.error = "manifest missing output.id or output.version";
            return result;
        }
        
        result.manifest.output_id = j["output"]["id"];
        result.manifest.output_version = j["output"]["version"];
        
        if (!j.contains("inputs") || !j["inputs"].is_array()) {
            result.error = "manifest missing inputs array";
            return result;
        }
        
        for (const auto& inp : j["inputs"]) {
            ComposeManifest::SourceEntry entry;
            entry.id = inp.value("id", "");
            entry.version = inp.value("version", "");
            entry.source_type = inp.value("source_type", "installed");
            entry.source = inp.value("source", "");
            entry.sha256 = inp.value("sha256", "");
            result.manifest.inputs.push_back(entry);
        }
        
        if (j.contains("options")) {
            if (j["options"].contains("on_conflict")) {
                result.manifest.options.on_conflict = 
                    parse_conflict_strategy(j["options"]["on_conflict"]);
            }
            if (j["options"].contains("loader_from")) {
                result.manifest.options.loader_from = j["options"]["loader_from"];
            }
        }
        
        if (j.contains("overrides")) {
            if (j["overrides"].contains("environment")) {
                for (auto& [key, val] : j["overrides"]["environment"].items()) {
                    result.manifest.overrides.environment.emplace_back(key, val.get<std::string>());
                }
            }
            if (j["overrides"].contains("lib_dirs_append")) {
                result.manifest.overrides.lib_dirs_append = 
                    j["overrides"]["lib_dirs_append"].get<std::vector<std::string>>();
            }
        }
        
        result.ok = true;
        
    } catch (const json::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }
    
    return result;
}

ComposeResult compose_from_manifest(const std::string& manifest_path,
                                    const std::string& output_path,
                                    const std::string& nah_root,
                                    bool verbose) {
    ComposeResult result;
    
    std::ifstream file(manifest_path);
    if (!file) {
        result.error = "failed to open manifest: " + manifest_path;
        return result;
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    
    auto parse_result = parse_compose_manifest(ss.str());
    if (!parse_result.ok) {
        result.error = parse_result.error;
        return result;
    }
    
    const auto& manifest = parse_result.manifest;
    
    // Build input refs based on source type
    std::vector<std::string> input_refs;
    for (const auto& inp : manifest.inputs) {
        ComposeSourceType source_type = parse_source_type(inp.source_type);
        
        switch (source_type) {
            case ComposeSourceType::NakFile:
            case ComposeSourceType::Directory:
                // Use the stored source path
                if (inp.source.empty()) {
                    result.error = "manifest entry for " + inp.id + "@" + inp.version + 
                                   " has source_type=" + inp.source_type + " but no source path";
                    return result;
                }
                if (!path_exists(inp.source)) {
                    result.error = "source path not found: " + inp.source + 
                                   " (for " + inp.id + "@" + inp.version + ")";
                    return result;
                }
                // Verify hash if available (for .nak files)
                if (source_type == ComposeSourceType::NakFile && !inp.sha256.empty()) {
                    std::string actual_hash = compute_file_sha256(inp.source);
                    if (actual_hash != inp.sha256) {
                        result.error = "hash mismatch for " + inp.source + 
                                       "\n  expected: " + inp.sha256 +
                                       "\n  actual:   " + actual_hash;
                        return result;
                    }
                }
                input_refs.push_back(inp.source);
                break;
                
            case ComposeSourceType::Installed:
            default:
                // Use id@version to resolve from registry
                input_refs.push_back(inp.id + "@" + inp.version);
                break;
        }
    }
    
    // Build options
    ComposeOptions options;
    options.nah_root = nah_root;
    options.output_id = manifest.output_id;
    options.output_version = manifest.output_version;
    options.output_path = output_path;
    options.on_conflict = manifest.options.on_conflict;
    options.loader_from = manifest.options.loader_from;
    options.add_env = manifest.overrides.environment;
    options.add_lib_dirs = manifest.overrides.lib_dirs_append;
    options.verbose = verbose;
    
    return compose_naks(input_refs, options);
}

} // namespace nah
