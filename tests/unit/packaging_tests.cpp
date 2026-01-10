#include <doctest/doctest.h>
#include <nah/packaging.hpp>
#include <nah/platform.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

using namespace nah;

// Helper to create temporary directory
class TempDir {
public:
    TempDir() {
        path_ = fs::temp_directory_path() / ("nah_test_" + generate_uuid());
        fs::create_directories(path_);
    }
    
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    
    std::string path() const { return path_.string(); }
    
private:
    fs::path path_;
};

TEST_CASE("validate_extraction_path accepts safe relative paths") {
    auto result = validate_extraction_path("bin/app", "/extract");
    CHECK(result.safe);
    CHECK(result.normalized_path == "bin/app");
}

TEST_CASE("validate_extraction_path accepts nested paths") {
    auto result = validate_extraction_path("lib/sub/dir/file.so", "/extract");
    CHECK(result.safe);
    CHECK(result.normalized_path == "lib/sub/dir/file.so");
}

TEST_CASE("validate_extraction_path rejects absolute paths") {
    auto result = validate_extraction_path("/etc/passwd", "/extract");
    CHECK_FALSE(result.safe);
    CHECK(result.error.find("absolute") != std::string::npos);
}

TEST_CASE("validate_extraction_path rejects path traversal") {
    auto result1 = validate_extraction_path("../etc/passwd", "/extract");
    CHECK_FALSE(result1.safe);
    CHECK(result1.error.find("traversal") != std::string::npos);
    
    auto result2 = validate_extraction_path("bin/../../../etc/passwd", "/extract");
    CHECK_FALSE(result2.safe);
}

TEST_CASE("validate_extraction_path normalizes paths with dots") {
    auto result = validate_extraction_path("./bin/./app", "/extract");
    CHECK(result.safe);
    CHECK(result.normalized_path == "bin/app");
}

TEST_CASE("create_deterministic_archive creates valid gzip tar") {
    std::vector<TarEntry> entries;
    
    TarEntry dir;
    dir.path = "bin";
    dir.type = TarEntryType::Directory;
    entries.push_back(dir);
    
    TarEntry file;
    file.path = "bin/app";
    file.type = TarEntryType::RegularFile;
    file.data = {'h', 'e', 'l', 'l', 'o'};
    file.executable = true;
    entries.push_back(file);
    
    auto result = create_deterministic_archive(entries);
    
    CHECK(result.ok);
    CHECK_FALSE(result.archive_data.empty());
    
    // Verify gzip magic bytes
    REQUIRE(result.archive_data.size() >= 2);
    CHECK(result.archive_data[0] == 0x1f);
    CHECK(result.archive_data[1] == 0x8b);
}

TEST_CASE("create_deterministic_archive rejects symlinks") {
    std::vector<TarEntry> entries;
    
    TarEntry symlink;
    symlink.path = "link";
    symlink.type = TarEntryType::Symlink;
    entries.push_back(symlink);
    
    auto result = create_deterministic_archive(entries);
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("symlink") != std::string::npos);
}

TEST_CASE("create_deterministic_archive rejects hardlinks") {
    std::vector<TarEntry> entries;
    
    TarEntry hardlink;
    hardlink.path = "link";
    hardlink.type = TarEntryType::Hardlink;
    entries.push_back(hardlink);
    
    auto result = create_deterministic_archive(entries);
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("hardlink") != std::string::npos);
}

TEST_CASE("create_deterministic_archive sorts entries lexicographically") {
    std::vector<TarEntry> entries;
    
    TarEntry z_file;
    z_file.path = "z_file";
    z_file.type = TarEntryType::RegularFile;
    z_file.data = {'z'};
    entries.push_back(z_file);
    
    TarEntry a_file;
    a_file.path = "a_file";
    a_file.type = TarEntryType::RegularFile;
    a_file.data = {'a'};
    entries.push_back(a_file);
    
    TarEntry m_file;
    m_file.path = "m_file";
    m_file.type = TarEntryType::RegularFile;
    m_file.data = {'m'};
    entries.push_back(m_file);
    
    auto result1 = create_deterministic_archive(entries);
    
    // Reverse the order
    std::reverse(entries.begin(), entries.end());
    
    auto result2 = create_deterministic_archive(entries);
    
    // Both should produce identical output due to sorting
    CHECK(result1.ok);
    CHECK(result2.ok);
    CHECK(result1.archive_data == result2.archive_data);
}

TEST_CASE("create_deterministic_archive is reproducible") {
    std::vector<TarEntry> entries;
    
    TarEntry file;
    file.path = "test.txt";
    file.type = TarEntryType::RegularFile;
    file.data = {'t', 'e', 's', 't'};
    entries.push_back(file);
    
    auto result1 = create_deterministic_archive(entries);
    auto result2 = create_deterministic_archive(entries);
    
    CHECK(result1.ok);
    CHECK(result2.ok);
    CHECK(result1.archive_data == result2.archive_data);
}

TEST_CASE("collect_directory_entries collects files from directory") {
    TempDir temp;
    
    // Create test structure
    fs::create_directories(temp.path() + "/bin");
    fs::create_directories(temp.path() + "/lib");
    
    std::ofstream(temp.path() + "/bin/app") << "binary";
    std::ofstream(temp.path() + "/lib/lib.so") << "library";
    
    auto result = collect_directory_entries(temp.path());
    
    CHECK(result.ok);
    CHECK(result.entries.size() >= 4);  // 2 dirs + 2 files
    
    bool found_bin = false, found_app = false;
    for (const auto& entry : result.entries) {
        if (entry.path == "bin") found_bin = true;
        if (entry.path == "bin/app") found_app = true;
    }
    
    CHECK(found_bin);
    CHECK(found_app);
}

TEST_CASE("pack_directory creates archive from directory") {
    TempDir temp;
    
    fs::create_directories(temp.path() + "/bin");
    std::ofstream(temp.path() + "/bin/app") << "binary content";
    
    auto result = pack_directory(temp.path());
    
    CHECK(result.ok);
    CHECK_FALSE(result.archive_data.empty());
}

TEST_CASE("extract_archive_safe extracts to staging directory") {
    TempDir temp;
    
    // Create a simple archive
    std::vector<TarEntry> entries;
    
    TarEntry dir;
    dir.path = "bin";
    dir.type = TarEntryType::Directory;
    entries.push_back(dir);
    
    TarEntry file;
    file.path = "bin/app";
    file.type = TarEntryType::RegularFile;
    file.data = {'h', 'e', 'l', 'l', 'o'};
    entries.push_back(file);
    
    auto pack_result = create_deterministic_archive(entries);
    REQUIRE(pack_result.ok);
    
    std::string staging = temp.path() + "/staging";
    auto extract_result = extract_archive_safe(pack_result.archive_data, staging);
    
    CHECK(extract_result.ok);
    CHECK(fs::exists(staging + "/bin"));
    CHECK(fs::exists(staging + "/bin/app"));
    
    // Verify content
    std::ifstream f(staging + "/bin/app");
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    CHECK(content == "hello");
}

TEST_CASE("pack_nak validates META/nak.json presence") {
    TempDir temp;
    
    // Directory without META/nak.json
    auto result = pack_nak(temp.path());
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("META/nak.json") != std::string::npos);
}

TEST_CASE("pack_nak validates schema in META/nak.json") {
    TempDir temp;
    
    fs::create_directories(temp.path() + "/META");
    
    // Invalid schema
    std::ofstream(temp.path() + "/META/nak.json") << R"({
        "$schema": "nah.nak.pack.v1",
        "nak": {
            "id": "com.example.nak",
            "version": "1.0.0"
        }
    })";
    
    auto result = pack_nak(temp.path());
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("schema") != std::string::npos);
}

TEST_CASE("pack_nak succeeds with valid structure") {
    TempDir temp;
    
    fs::create_directories(temp.path() + "/META");
    fs::create_directories(temp.path() + "/lib");
    
    std::ofstream(temp.path() + "/META/nak.json") << R"({
        "$schema": "nah.nak.pack.v2",
        "nak": {
            "id": "com.example.nak",
            "version": "1.0.0"
        },
        "paths": {
            "resource_root": ".",
            "lib_dirs": ["lib"]
        },
        "execution": {
            "cwd": "{NAH_APP_ROOT}"
        }
    })";
    
    std::ofstream(temp.path() + "/lib/lib.so") << "library";
    
    auto result = pack_nak(temp.path());
    
    CHECK(result.ok);
    CHECK_FALSE(result.archive_data.empty());
}

TEST_CASE("inspect_nak_pack extracts metadata") {
    TempDir temp;
    
    fs::create_directories(temp.path() + "/META");
    
    std::ofstream(temp.path() + "/META/nak.json") << R"({
        "$schema": "nah.nak.pack.v2",
        "nak": {
            "id": "com.example.nak",
            "version": "2.1.0"
        },
        "paths": {
            "resource_root": "resources",
            "lib_dirs": ["lib"]
        },
        "execution": {
            "cwd": "{NAH_APP_ROOT}"
        }
    })";
    
    auto pack_result = pack_directory(temp.path());
    REQUIRE(pack_result.ok);
    
    auto info = inspect_nak_pack(pack_result.archive_data);
    
    CHECK(info.ok);
    CHECK(info.schema == "nah.nak.pack.v2");
    CHECK(info.nak_id == "com.example.nak");
    CHECK(info.nak_version == "2.1.0");
    CHECK(info.resource_root == "resources");
}

TEST_CASE("pack_nap validates manifest presence") {
    TempDir temp;
    
    fs::create_directories(temp.path() + "/bin");
    std::ofstream(temp.path() + "/bin/app") << "binary";
    
    // No manifest.nah and no embedded manifest
    auto result = pack_nap(temp.path());
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("manifest") != std::string::npos);
}

// ============================================================================
// Metadata Normalization Tests (SPEC L1825-1831)
// ============================================================================

TEST_CASE("deterministic archive has gzip mtime=0") {
    std::vector<TarEntry> entries;
    
    TarEntry file;
    file.path = "test.txt";
    file.type = TarEntryType::RegularFile;
    file.data = {'t', 'e', 's', 't'};
    entries.push_back(file);
    
    auto result = create_deterministic_archive(entries);
    REQUIRE(result.ok);
    REQUIRE(result.archive_data.size() >= 10);
    
    // Gzip header bytes 4-7 are mtime (should all be 0)
    CHECK(result.archive_data[4] == 0x00);
    CHECK(result.archive_data[5] == 0x00);
    CHECK(result.archive_data[6] == 0x00);
    CHECK(result.archive_data[7] == 0x00);
}

TEST_CASE("deterministic archive has gzip OS=255 (unknown)") {
    std::vector<TarEntry> entries;
    
    TarEntry file;
    file.path = "test.txt";
    file.type = TarEntryType::RegularFile;
    file.data = {'t', 'e', 's', 't'};
    entries.push_back(file);
    
    auto result = create_deterministic_archive(entries);
    REQUIRE(result.ok);
    REQUIRE(result.archive_data.size() >= 10);
    
    // Gzip header byte 9 is OS (should be 255)
    CHECK(result.archive_data[9] == 0xff);
}

TEST_CASE("deterministic archive has no gzip filename flag") {
    std::vector<TarEntry> entries;
    
    TarEntry file;
    file.path = "test.txt";
    file.type = TarEntryType::RegularFile;
    file.data = {'t', 'e', 's', 't'};
    entries.push_back(file);
    
    auto result = create_deterministic_archive(entries);
    REQUIRE(result.ok);
    REQUIRE(result.archive_data.size() >= 10);
    
    // Gzip header byte 3 is flags (should be 0x00 - no name, no comment)
    CHECK(result.archive_data[3] == 0x00);
}

TEST_CASE("deterministic archive directories have mode 0755") {
    std::vector<TarEntry> entries;
    
    TarEntry dir;
    dir.path = "bin";
    dir.type = TarEntryType::Directory;
    entries.push_back(dir);
    
    auto pack_result = create_deterministic_archive(entries);
    REQUIRE(pack_result.ok);
    
    // Extract and verify
    TempDir temp;
    std::string staging = temp.path() + "/staging";
    auto extract_result = extract_archive_safe(pack_result.archive_data, staging);
    
    REQUIRE(extract_result.ok);
    
    // Check directory permissions (on Unix)
#ifndef _WIN32
    struct stat st;
    REQUIRE(stat((staging + "/bin").c_str(), &st) == 0);
    CHECK((st.st_mode & 0777) == 0755);
#endif
}

TEST_CASE("deterministic archive regular files have mode 0644") {
    std::vector<TarEntry> entries;
    
    TarEntry file;
    file.path = "data.txt";
    file.type = TarEntryType::RegularFile;
    file.data = {'d', 'a', 't', 'a'};
    file.executable = false;
    entries.push_back(file);
    
    auto pack_result = create_deterministic_archive(entries);
    REQUIRE(pack_result.ok);
    
    // Extract and verify
    TempDir temp;
    std::string staging = temp.path() + "/staging";
    auto extract_result = extract_archive_safe(pack_result.archive_data, staging);
    
    REQUIRE(extract_result.ok);
    
#ifndef _WIN32
    struct stat st;
    REQUIRE(stat((staging + "/data.txt").c_str(), &st) == 0);
    CHECK((st.st_mode & 0777) == 0644);
#endif
}

TEST_CASE("deterministic archive executable files have mode 0755") {
    std::vector<TarEntry> entries;
    
    TarEntry file;
    file.path = "bin/app";
    file.type = TarEntryType::RegularFile;
    file.data = {'b', 'i', 'n'};
    file.executable = true;
    entries.push_back(file);
    
    auto pack_result = create_deterministic_archive(entries);
    REQUIRE(pack_result.ok);
    
    // Extract and verify
    TempDir temp;
    std::string staging = temp.path() + "/staging";
    auto extract_result = extract_archive_safe(pack_result.archive_data, staging);
    
    REQUIRE(extract_result.ok);
    
#ifndef _WIN32
    struct stat st;
    REQUIRE(stat((staging + "/bin/app").c_str(), &st) == 0);
    CHECK((st.st_mode & 0777) == 0755);
#endif
}

// ============================================================================
// Tar Path Stripping Tests (for ./ prefix handling)
// ============================================================================

TEST_CASE("validate_extraction_path handles ./ prefix") {
    // After stripping ./ prefix, path should still be valid
    auto result = validate_extraction_path("META/nak.json", "/extract");
    CHECK(result.safe);
    CHECK(result.normalized_path == "META/nak.json");
}

TEST_CASE("extract_archive_safe handles archives with ./ prefix entries") {
    // Create archive with ./ prefix paths (like CMake generates)
    std::vector<TarEntry> entries;
    
    TarEntry dir;
    dir.path = "./META";
    dir.type = TarEntryType::Directory;
    entries.push_back(dir);
    
    TarEntry file;
    file.path = "./META/nak.json";
    file.type = TarEntryType::RegularFile;
    file.data = {'t', 'e', 's', 't'};
    entries.push_back(file);
    
    auto pack_result = create_deterministic_archive(entries);
    REQUIRE(pack_result.ok);
    
    TempDir temp;
    std::string staging = temp.path() + "/staging";
    auto extract_result = extract_archive_safe(pack_result.archive_data, staging);
    
    CHECK(extract_result.ok);
    CHECK(fs::exists(staging + "/META/nak.json"));
}

TEST_CASE("inspect_nak_pack handles ./ prefix in archive entries") {
    // Create NAK pack with ./ prefixed paths
    TempDir temp;
    
    fs::create_directories(temp.path() + "/META");
    fs::create_directories(temp.path() + "/lib");
    
    std::ofstream(temp.path() + "/META/nak.json") << R"({
  "$schema": "nah.nak.pack.v2",
  "nak": {
    "id": "com.example.dotprefix",
    "version": "1.0.0"
  },
  "paths": {
    "resource_root": "resources",
    "lib_dirs": ["lib"]
  }
})";
    
    // pack_nak creates archives with ./ prefix (via CMake tar)
    auto pack_result = pack_nak(temp.path());
    REQUIRE(pack_result.ok);
    
    // inspect_nak_pack should handle ./ prefix correctly
    auto info = inspect_nak_pack(pack_result.archive_data);
    
    CHECK(info.ok);
    CHECK(info.nak_id == "com.example.dotprefix");
    CHECK(info.nak_version == "1.0.0");
}
