#include <doctest/doctest.h>
#include <nah/compose.hpp>
#include <nah/packaging.hpp>
#include <nah/platform.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

using namespace nah;

// ============================================================================
// Test Helper: Temporary Directory with NAK structures
// ============================================================================

class ComposeTempDir {
public:
    ComposeTempDir() {
        base_path_ = fs::temp_directory_path() / ("nah_compose_test_" + generate_uuid());
        fs::create_directories(base_path_);
        
        // Create NAH root structure
        nah_root_ = base_path_ / "nah_root";
        fs::create_directories(nah_root_ / "registry" / "naks");
    }
    
    ~ComposeTempDir() {
        std::error_code ec;
        fs::remove_all(base_path_, ec);
    }
    
    std::string base_path() const { return base_path_.string(); }
    std::string nah_root() const { return nah_root_.string(); }
    
    // Create a NAK directory with the given ID and version
    std::string create_nak_dir(const std::string& id, const std::string& version,
                                const std::vector<std::string>& lib_dirs = {"lib"},
                                const std::map<std::string, std::string>& env = {},
                                bool with_loader = false) {
        fs::path nak_path = base_path_ / (id + "-" + version);
        fs::create_directories(nak_path / "META");
        fs::create_directories(nak_path / "lib");
        
        // Create a sample library file
        std::ofstream lib_file(nak_path / "lib" / ("lib" + id + ".so"));
        lib_file << "# Mock library for " << id << "\n";
        lib_file.close();
        
        // Create nak.json
        std::ostringstream json;
        json << "{\n";
        json << "  \"nak\": { \"id\": \"" << id << "\", \"version\": \"" << version << "\" },\n";
        json << "  \"paths\": {\n";
        json << "    \"lib_dirs\": [";
        for (size_t i = 0; i < lib_dirs.size(); ++i) {
            if (i > 0) json << ", ";
            json << "\"" << lib_dirs[i] << "\"";
        }
        json << "]\n";
        json << "  }";
        
        if (!env.empty()) {
            json << ",\n  \"environment\": {\n";
            bool first = true;
            for (const auto& [key, val] : env) {
                if (!first) json << ",\n";
                json << "    \"" << key << "\": \"" << val << "\"";
                first = false;
            }
            json << "\n  }";
        }
        
        if (with_loader) {
            fs::create_directories(nak_path / "bin");
            std::ofstream loader(nak_path / "bin" / "loader");
            loader << "#!/bin/sh\nexec \"$@\"\n";
            loader.close();
            fs::permissions(nak_path / "bin" / "loader", 
                fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write);
            
            json << ",\n  \"loader\": {\n";
            json << "    \"exec_path\": \"bin/loader\",\n";
            json << "    \"args_template\": [\"{NAH_APP_ENTRY}\"]\n";
            json << "  }";
        }
        
        json << "\n}\n";
        
        std::ofstream nak_json(nak_path / "META" / "nak.json");
        nak_json << json.str();
        nak_json.close();
        
        return nak_path.string();
    }
    
    // Create a .nak file from a directory
    std::string create_nak_file(const std::string& dir_path) {
        auto pack_result = pack_nak(dir_path);
        if (!pack_result.ok) {
            return "";
        }
        
        fs::path nak_file = base_path_ / (fs::path(dir_path).filename().string() + ".nak");
        std::ofstream out(nak_file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(pack_result.archive_data.data()),
                  static_cast<std::streamsize>(pack_result.archive_data.size()));
        out.close();
        
        return nak_file.string();
    }
    
private:
    fs::path base_path_;
    fs::path nah_root_;
};

// ============================================================================
// resolve_compose_input Tests
// ============================================================================

TEST_CASE("resolve_compose_input: resolves directory NAK") {
    ComposeTempDir tmp;
    
    std::string nak_dir = tmp.create_nak_dir("test-nak", "1.0.0");
    
    std::string error;
    auto input = resolve_compose_input(nak_dir, tmp.nah_root(), error);
    
    CHECK(error.empty());
    CHECK(input.id == "test-nak");
    CHECK(input.version == "1.0.0");
    CHECK(input.source_type == ComposeSourceType::Directory);
    CHECK(input.root_path == fs::absolute(nak_dir).string());
}

TEST_CASE("resolve_compose_input: resolves .nak file") {
    ComposeTempDir tmp;
    
    std::string nak_dir = tmp.create_nak_dir("file-nak", "2.0.0");
    std::string nak_file = tmp.create_nak_file(nak_dir);
    REQUIRE(!nak_file.empty());
    
    std::string error;
    auto input = resolve_compose_input(nak_file, tmp.nah_root(), error);
    
    CHECK(error.empty());
    CHECK(input.id == "file-nak");
    CHECK(input.version == "2.0.0");
    CHECK(input.source_type == ComposeSourceType::NakFile);
    CHECK(!input.content_hash.empty());  // SHA-256 should be computed
}

TEST_CASE("resolve_compose_input: error for non-existent path") {
    ComposeTempDir tmp;
    
    std::string error;
    auto input = resolve_compose_input("/non/existent/path", tmp.nah_root(), error);
    
    CHECK(!error.empty());
    CHECK(error.find("not found") != std::string::npos);
}

TEST_CASE("resolve_compose_input: error for directory without nak.json") {
    ComposeTempDir tmp;
    
    fs::path empty_dir = fs::path(tmp.base_path()) / "empty-dir";
    fs::create_directories(empty_dir);
    
    std::string error;
    auto input = resolve_compose_input(empty_dir.string(), tmp.nah_root(), error);
    
    CHECK(!error.empty());
    CHECK(error.find("META/nak.json") != std::string::npos);
}

// ============================================================================
// compose_naks Tests - Basic Functionality
// ============================================================================

TEST_CASE("compose_naks: composes two directory NAKs") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0", {"lib"}, {{"VAR_A", "value_a"}});
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0", {"lib"}, {{"VAR_B", "value_b"}});
    
    fs::path output_dir = fs::path(tmp.base_path()) / "composed";
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = output_dir.string();
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(result.ok);
    CHECK(result.error.empty());
    CHECK(result.nak_id == "composed");
    CHECK(result.nak_version == "1.0.0");
    CHECK(result.sources.size() == 2);
    
    // Check output structure
    CHECK(fs::exists(output_dir / "META" / "nak.json"));
    CHECK(fs::exists(output_dir / "lib" / "libnak-a.so"));
    CHECK(fs::exists(output_dir / "lib" / "libnak-b.so"));
}

TEST_CASE("compose_naks: produces .nak file when output path ends with .nak") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0");
    
    fs::path output_file = fs::path(tmp.base_path()) / "output.nak";
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "output";
    options.output_version = "1.0.0";
    options.output_path = output_file.string();
    
    auto result = compose_naks({nak_a}, options);
    
    CHECK(result.ok);
    CHECK(fs::exists(output_file));
    CHECK(fs::file_size(output_file) > 0);
}

TEST_CASE("compose_naks: error when no inputs provided") {
    ComposeTempDir tmp;
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "test";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    
    auto result = compose_naks({}, options);
    
    CHECK(!result.ok);
    CHECK(result.error.find("at least one input") != std::string::npos);
}

TEST_CASE("compose_naks: error when required options missing") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0");
    
    SUBCASE("missing --id") {
        ComposeOptions options;
        options.nah_root = tmp.nah_root();
        options.output_version = "1.0.0";
        options.output_path = (fs::path(tmp.base_path()) / "out").string();
        
        auto result = compose_naks({nak_a}, options);
        CHECK(!result.ok);
        CHECK(result.error.find("--id") != std::string::npos);
    }
    
    SUBCASE("missing --version") {
        ComposeOptions options;
        options.nah_root = tmp.nah_root();
        options.output_id = "test";
        options.output_path = (fs::path(tmp.base_path()) / "out").string();
        
        auto result = compose_naks({nak_a}, options);
        CHECK(!result.ok);
        CHECK(result.error.find("--version") != std::string::npos);
    }
    
    SUBCASE("missing --output") {
        ComposeOptions options;
        options.nah_root = tmp.nah_root();
        options.output_id = "test";
        options.output_version = "1.0.0";
        
        auto result = compose_naks({nak_a}, options);
        CHECK(!result.ok);
        CHECK(result.error.find("-o") != std::string::npos);
    }
}

// ============================================================================
// compose_naks Tests - Conflict Detection
// ============================================================================

TEST_CASE("compose_naks: detects file conflicts") {
    ComposeTempDir tmp;
    
    // Create two NAKs with the same file but different content
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0");
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0");
    
    // Create conflicting file in both
    std::ofstream file_a(fs::path(nak_a) / "lib" / "shared.so");
    file_a << "content from A";
    file_a.close();
    
    std::ofstream file_b(fs::path(nak_b) / "lib" / "shared.so");
    file_b << "content from B";
    file_b.close();
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    options.on_conflict = ConflictStrategy::Error;
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(!result.ok);
    CHECK(!result.conflicts.empty());
    CHECK(result.conflicts[0].relative_path == "lib/shared.so");
}

TEST_CASE("compose_naks: --on-conflict=first uses first NAK's file") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0");
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0");
    
    std::ofstream file_a(fs::path(nak_a) / "lib" / "shared.so");
    file_a << "content from A";
    file_a.close();
    
    std::ofstream file_b(fs::path(nak_b) / "lib" / "shared.so");
    file_b << "content from B";
    file_b.close();
    
    fs::path output_dir = fs::path(tmp.base_path()) / "out";
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = output_dir.string();
    options.on_conflict = ConflictStrategy::First;
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(result.ok);
    CHECK(!result.conflicts.empty());  // Conflict was detected but resolved
    
    // Verify content is from first NAK
    std::ifstream output_file(output_dir / "lib" / "shared.so");
    std::string content((std::istreambuf_iterator<char>(output_file)),
                        std::istreambuf_iterator<char>());
    CHECK(content == "content from A");
}

TEST_CASE("compose_naks: --on-conflict=last uses last NAK's file") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0");
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0");
    
    std::ofstream file_a(fs::path(nak_a) / "lib" / "shared.so");
    file_a << "content from A";
    file_a.close();
    
    std::ofstream file_b(fs::path(nak_b) / "lib" / "shared.so");
    file_b << "content from B";
    file_b.close();
    
    fs::path output_dir = fs::path(tmp.base_path()) / "out";
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = output_dir.string();
    options.on_conflict = ConflictStrategy::Last;
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(result.ok);
    
    // Verify content is from last NAK
    std::ifstream output_file(output_dir / "lib" / "shared.so");
    std::string content((std::istreambuf_iterator<char>(output_file)),
                        std::istreambuf_iterator<char>());
    CHECK(content == "content from B");
}

TEST_CASE("compose_naks: identical files are deduplicated (no conflict)") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0");
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0");
    
    // Create identical file in both
    std::ofstream file_a(fs::path(nak_a) / "lib" / "shared.so");
    file_a << "identical content";
    file_a.close();
    
    std::ofstream file_b(fs::path(nak_b) / "lib" / "shared.so");
    file_b << "identical content";
    file_b.close();
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    options.on_conflict = ConflictStrategy::Error;
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(result.ok);
    CHECK(result.conflicts.empty());  // No conflict for identical files
}

// ============================================================================
// compose_naks Tests - lib_dirs Merging
// ============================================================================

TEST_CASE("compose_naks: lib_dirs are concatenated in input order") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0", {"lib/a"});
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0", {"lib/b"});
    
    fs::path output_dir = fs::path(tmp.base_path()) / "out";
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = output_dir.string();
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(result.ok);
    REQUIRE(result.lib_dirs.size() == 2);
    CHECK(result.lib_dirs[0] == "lib/a");
    CHECK(result.lib_dirs[1] == "lib/b");
}

TEST_CASE("compose_naks: duplicate lib_dirs are deduplicated") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0", {"lib"});
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0", {"lib"});  // Same lib_dir
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(result.ok);
    CHECK(result.lib_dirs.size() == 1);
    CHECK(result.lib_dirs[0] == "lib");
}

TEST_CASE("compose_naks: --add-lib-dir appends to lib_dirs") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0", {"lib/a"});
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    options.add_lib_dirs = {"lib/extra"};
    
    auto result = compose_naks({nak_a}, options);
    
    CHECK(result.ok);
    REQUIRE(result.lib_dirs.size() == 2);
    CHECK(result.lib_dirs[0] == "lib/a");
    CHECK(result.lib_dirs[1] == "lib/extra");
}

// ============================================================================
// compose_naks Tests - Environment Merging
// ============================================================================

TEST_CASE("compose_naks: environment variables from different keys are merged") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0", {"lib"}, {{"VAR_A", "value_a"}});
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0", {"lib"}, {{"VAR_B", "value_b"}});
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(result.ok);
    CHECK(result.merged_environment.count("VAR_A") == 1);
    CHECK(result.merged_environment.count("VAR_B") == 1);
    CHECK(result.merged_environment["VAR_A"].value == "value_a");
    CHECK(result.merged_environment["VAR_B"].value == "value_b");
}

TEST_CASE("compose_naks: same key with set - last wins") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0", {"lib"}, {{"SHARED", "from_a"}});
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0", {"lib"}, {{"SHARED", "from_b"}});
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(result.ok);
    CHECK(result.merged_environment["SHARED"].value == "from_b");
}

TEST_CASE("compose_naks: --add-env overrides merged environment") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0", {"lib"}, {{"VAR", "original"}});
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    options.add_env = {{"VAR", "overridden"}, {"NEW_VAR", "new_value"}};
    
    auto result = compose_naks({nak_a}, options);
    
    CHECK(result.ok);
    CHECK(result.merged_environment["VAR"].value == "overridden");
    CHECK(result.merged_environment["NEW_VAR"].value == "new_value");
}

// ============================================================================
// compose_naks Tests - Loader Selection
// ============================================================================

TEST_CASE("compose_naks: single NAK with loader - uses its loader") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0", {"lib"}, {}, true);  // with_loader = true
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    
    auto result = compose_naks({nak_a}, options);
    
    CHECK(result.ok);
    CHECK(result.selected_loader_from.has_value());
    CHECK(result.selected_loader_from.value() == "nak-a");
}

TEST_CASE("compose_naks: multiple NAKs with loaders - error without --loader-from") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0", {"lib"}, {}, true);
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0", {"lib"}, {}, true);
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(!result.ok);
    CHECK(result.error.find("Multiple NAKs define loaders") != std::string::npos);
}

TEST_CASE("compose_naks: multiple NAKs with loaders - --loader-from selects one") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0", {"lib"}, {}, true);
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0", {"lib"}, {}, true);
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    options.loader_from = "nak-b";
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(result.ok);
    CHECK(result.selected_loader_from.value() == "nak-b");
}

TEST_CASE("compose_naks: no loaders - libs-only NAK") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0", {"lib"}, {}, false);
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0", {"lib"}, {}, false);
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(result.ok);
    CHECK(!result.selected_loader_from.has_value());
}

// ============================================================================
// compose_naks Tests - Dry Run
// ============================================================================

TEST_CASE("compose_naks: --dry-run does not create output") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0");
    
    fs::path output_dir = fs::path(tmp.base_path()) / "should-not-exist";
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = output_dir.string();
    options.dry_run = true;
    
    auto result = compose_naks({nak_a}, options);
    
    CHECK(result.ok);
    CHECK(!result.files_to_copy.empty());
    CHECK(!fs::exists(output_dir));  // Output should NOT be created
}

// ============================================================================
// compose_naks Tests - Provenance
// ============================================================================

TEST_CASE("compose_naks: provenance is included by default") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0");
    
    fs::path output_dir = fs::path(tmp.base_path()) / "out";
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = output_dir.string();
    
    auto result = compose_naks({nak_a}, options);
    
    CHECK(result.ok);
    
    // Read and check the generated nak.json
    std::ifstream nak_json(output_dir / "META" / "nak.json");
    std::string content((std::istreambuf_iterator<char>(nak_json)),
                        std::istreambuf_iterator<char>());
    
    CHECK(content.find("\"provenance\"") != std::string::npos);
    CHECK(content.find("\"composed\": true") != std::string::npos);
    CHECK(content.find("\"sources\"") != std::string::npos);
}

// ============================================================================
// Manifest Emission and Parsing Tests
// ============================================================================

TEST_CASE("compose_naks: --emit-manifest creates manifest file") {
    ComposeTempDir tmp;
    
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0");
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0");
    
    fs::path manifest_path = fs::path(tmp.base_path()) / "manifest.json";
    
    ComposeOptions options;
    options.nah_root = tmp.nah_root();
    options.output_id = "composed";
    options.output_version = "1.0.0";
    options.output_path = (fs::path(tmp.base_path()) / "out").string();
    options.emit_manifest = manifest_path.string();
    
    auto result = compose_naks({nak_a, nak_b}, options);
    
    CHECK(result.ok);
    CHECK(fs::exists(manifest_path));
    
    // Read manifest and verify
    std::ifstream manifest_file(manifest_path);
    std::string content((std::istreambuf_iterator<char>(manifest_file)),
                        std::istreambuf_iterator<char>());
    
    CHECK(content.find("\"$schema\": \"nah.nak.compose.v1\"") != std::string::npos);
    CHECK(content.find("\"id\": \"composed\"") != std::string::npos);
    CHECK(content.find("\"nak-a\"") != std::string::npos);
    CHECK(content.find("\"nak-b\"") != std::string::npos);
    CHECK(content.find("\"source_type\": \"directory\"") != std::string::npos);
}

TEST_CASE("parse_compose_manifest: parses valid manifest") {
    std::string manifest_json = R"({
        "$schema": "nah.nak.compose.v1",
        "output": {
            "id": "my-composed",
            "version": "1.0.0"
        },
        "inputs": [
            {"id": "nak-a", "version": "1.0.0", "source_type": "directory", "source": "/path/to/a"},
            {"id": "nak-b", "version": "2.0.0", "source_type": "file", "source": "/path/to/b.nak", "sha256": "abc123"}
        ],
        "options": {
            "on_conflict": "first",
            "loader_from": "nak-a"
        },
        "overrides": {
            "environment": {"KEY": "value"},
            "lib_dirs_append": ["lib/extra"]
        }
    })";
    
    auto result = parse_compose_manifest(manifest_json);
    
    CHECK(result.ok);
    CHECK(result.manifest.output_id == "my-composed");
    CHECK(result.manifest.output_version == "1.0.0");
    REQUIRE(result.manifest.inputs.size() == 2);
    CHECK(result.manifest.inputs[0].id == "nak-a");
    CHECK(result.manifest.inputs[0].source_type == "directory");
    CHECK(result.manifest.inputs[1].sha256 == "abc123");
    CHECK(result.manifest.options.on_conflict == ConflictStrategy::First);
    CHECK(result.manifest.options.loader_from.value() == "nak-a");
    CHECK(result.manifest.overrides.environment.size() == 1);
    CHECK(result.manifest.overrides.lib_dirs_append.size() == 1);
}

TEST_CASE("parse_compose_manifest: error for invalid JSON") {
    auto result = parse_compose_manifest("not valid json");
    
    CHECK(!result.ok);
    CHECK(result.error.find("parse error") != std::string::npos);
}

TEST_CASE("parse_compose_manifest: error for missing required fields") {
    std::string manifest_json = R"({"inputs": []})";  // Missing output
    
    auto result = parse_compose_manifest(manifest_json);
    
    CHECK(!result.ok);
    CHECK(result.error.find("output") != std::string::npos);
}

// ============================================================================
// compose_from_manifest Tests
// ============================================================================

TEST_CASE("compose_from_manifest: reproduces composition from manifest") {
    ComposeTempDir tmp;
    
    // Create original NAKs
    std::string nak_a = tmp.create_nak_dir("nak-a", "1.0.0");
    std::string nak_b = tmp.create_nak_dir("nak-b", "2.0.0");
    
    // Create manifest file
    fs::path manifest_path = fs::path(tmp.base_path()) / "manifest.json";
    std::ofstream manifest_file(manifest_path);
    manifest_file << R"({
        "$schema": "nah.nak.compose.v1",
        "output": {"id": "reproduced", "version": "1.0.0"},
        "inputs": [
            {"id": "nak-a", "version": "1.0.0", "source_type": "directory", "source": ")" << nak_a << R"("},
            {"id": "nak-b", "version": "2.0.0", "source_type": "directory", "source": ")" << nak_b << R"("}
        ],
        "options": {"on_conflict": "error"}
    })";
    manifest_file.close();
    
    fs::path output_dir = fs::path(tmp.base_path()) / "reproduced";
    
    auto result = compose_from_manifest(manifest_path.string(), output_dir.string(), tmp.nah_root());
    
    CHECK(result.ok);
    CHECK(result.nak_id == "reproduced");
    CHECK(fs::exists(output_dir / "META" / "nak.json"));
}

TEST_CASE("compose_from_manifest: verifies SHA-256 for .nak files") {
    ComposeTempDir tmp;
    
    // Create NAK and get its hash
    std::string nak_dir = tmp.create_nak_dir("hash-test", "1.0.0");
    std::string nak_file = tmp.create_nak_file(nak_dir);
    REQUIRE(!nak_file.empty());
    
    std::string correct_hash = compute_file_sha256(nak_file);
    
    // Create manifest with correct hash
    fs::path manifest_path = fs::path(tmp.base_path()) / "manifest.json";
    std::ofstream manifest_file(manifest_path);
    manifest_file << R"({
        "$schema": "nah.nak.compose.v1",
        "output": {"id": "verified", "version": "1.0.0"},
        "inputs": [
            {"id": "hash-test", "version": "1.0.0", "source_type": "file", 
             "source": ")" << nak_file << R"(", "sha256": ")" << correct_hash << R"("}
        ]
    })";
    manifest_file.close();
    
    fs::path output_dir = fs::path(tmp.base_path()) / "verified";
    
    auto result = compose_from_manifest(manifest_path.string(), output_dir.string(), tmp.nah_root());
    
    CHECK(result.ok);
}

TEST_CASE("compose_from_manifest: fails on SHA-256 mismatch") {
    ComposeTempDir tmp;
    
    std::string nak_dir = tmp.create_nak_dir("hash-test", "1.0.0");
    std::string nak_file = tmp.create_nak_file(nak_dir);
    REQUIRE(!nak_file.empty());
    
    // Create manifest with WRONG hash
    fs::path manifest_path = fs::path(tmp.base_path()) / "manifest.json";
    std::ofstream manifest_file(manifest_path);
    manifest_file << R"({
        "$schema": "nah.nak.compose.v1",
        "output": {"id": "should-fail", "version": "1.0.0"},
        "inputs": [
            {"id": "hash-test", "version": "1.0.0", "source_type": "file", 
             "source": ")" << nak_file << R"(", "sha256": "0000000000000000000000000000000000000000000000000000000000000000"}
        ]
    })";
    manifest_file.close();
    
    fs::path output_dir = fs::path(tmp.base_path()) / "should-not-exist";
    
    auto result = compose_from_manifest(manifest_path.string(), output_dir.string(), tmp.nah_root());
    
    CHECK(!result.ok);
    CHECK(result.error.find("hash mismatch") != std::string::npos);
}

TEST_CASE("compose_from_manifest: error for non-existent source") {
    ComposeTempDir tmp;
    
    fs::path manifest_path = fs::path(tmp.base_path()) / "manifest.json";
    std::ofstream manifest_file(manifest_path);
    manifest_file << R"({
        "$schema": "nah.nak.compose.v1",
        "output": {"id": "should-fail", "version": "1.0.0"},
        "inputs": [
            {"id": "missing", "version": "1.0.0", "source_type": "directory", 
             "source": "/non/existent/path"}
        ]
    })";
    manifest_file.close();
    
    auto result = compose_from_manifest(manifest_path.string(), 
                                         (fs::path(tmp.base_path()) / "out").string(),
                                         tmp.nah_root());
    
    CHECK(!result.ok);
    CHECK(result.error.find("not found") != std::string::npos);
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST_CASE("conflict_strategy_to_string and parse_conflict_strategy roundtrip") {
    CHECK(conflict_strategy_to_string(ConflictStrategy::Error) == "error");
    CHECK(conflict_strategy_to_string(ConflictStrategy::First) == "first");
    CHECK(conflict_strategy_to_string(ConflictStrategy::Last) == "last");
    
    CHECK(parse_conflict_strategy("error") == ConflictStrategy::Error);
    CHECK(parse_conflict_strategy("first") == ConflictStrategy::First);
    CHECK(parse_conflict_strategy("last") == ConflictStrategy::Last);
    CHECK(parse_conflict_strategy("unknown") == ConflictStrategy::Error);  // Default
}

TEST_CASE("source_type_to_string and parse_source_type roundtrip") {
    CHECK(source_type_to_string(ComposeSourceType::Installed) == "installed");
    CHECK(source_type_to_string(ComposeSourceType::NakFile) == "file");
    CHECK(source_type_to_string(ComposeSourceType::Directory) == "directory");
    
    CHECK(parse_source_type("installed") == ComposeSourceType::Installed);
    CHECK(parse_source_type("file") == ComposeSourceType::NakFile);
    CHECK(parse_source_type("directory") == ComposeSourceType::Directory);
    CHECK(parse_source_type("unknown") == ComposeSourceType::Installed);  // Default
}
