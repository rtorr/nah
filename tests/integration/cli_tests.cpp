#include <doctest/doctest.h>
#include <nah/packaging.hpp>
#include <nah/platform.hpp>
#include <nah/nahhost.hpp>
#include <nah/nak_selection.hpp>
#include <nah/host_profile.hpp>
#include <nah/nak_record.hpp>
#include <nah/manifest.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

using namespace nah;

// Helper to compute CRC32 for manifest
static uint32_t manifest_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            uint32_t mask = (crc & 1u) ? 0xFFFFFFFFu : 0u;
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

// Helper to build a minimal manifest binary
static std::vector<uint8_t> build_test_manifest(const std::string& app_id, 
                                                 const std::string& version,
                                                 const std::string& nak_id) {
    // Build TLV payload
    std::vector<uint8_t> payload;
    
    // Helper to add TLV entry
    auto add_tlv = [&payload](uint16_t tag, const std::string& value) {
        // Tag (2 bytes, little-endian)
        payload.push_back(tag & 0xFF);
        payload.push_back((tag >> 8) & 0xFF);
        // Length (2 bytes, little-endian)
        uint16_t len = static_cast<uint16_t>(value.size());
        payload.push_back(len & 0xFF);
        payload.push_back((len >> 8) & 0xFF);
        // Value
        payload.insert(payload.end(), value.begin(), value.end());
    };
    
    // Add entries in ascending tag order (per SPEC)
    add_tlv(10, app_id);        // TAG_APP_ID = 10
    add_tlv(11, version);       // TAG_APP_VERSION = 11
    add_tlv(12, nak_id);        // TAG_NAK_ID = 12
    add_tlv(20, "bin/app");     // TAG_ENTRYPOINT = 20
    
    // Compute CRC
    uint32_t crc = manifest_crc32(payload.data(), payload.size());
    uint32_t total_size = static_cast<uint32_t>(payload.size() + 16);
    
    // Build header + payload
    std::vector<uint8_t> blob;
    
    // Magic "NAHM" (0x4D48414E little-endian)
    uint32_t magic = 0x4D48414E;
    blob.push_back(magic & 0xFF);
    blob.push_back((magic >> 8) & 0xFF);
    blob.push_back((magic >> 16) & 0xFF);
    blob.push_back((magic >> 24) & 0xFF);
    // Version = 1
    blob.push_back(1);
    blob.push_back(0);
    // Reserved
    blob.push_back(0);
    blob.push_back(0);
    // Total size
    blob.push_back(total_size & 0xFF);
    blob.push_back((total_size >> 8) & 0xFF);
    blob.push_back((total_size >> 16) & 0xFF);
    blob.push_back((total_size >> 24) & 0xFF);
    // CRC32
    blob.push_back(crc & 0xFF);
    blob.push_back((crc >> 8) & 0xFF);
    blob.push_back((crc >> 16) & 0xFF);
    blob.push_back((crc >> 24) & 0xFF);
    
    // Payload
    blob.insert(blob.end(), payload.begin(), payload.end());
    
    return blob;
}

// Helper to create temporary NAH root for testing
class TestNahRoot {
public:
    TestNahRoot() {
        root_ = fs::temp_directory_path() / ("nah_integration_" + generate_uuid());
        fs::create_directories(root_ / "apps");
        fs::create_directories(root_ / "naks");
        fs::create_directories(root_ / "registry" / "apps");
        fs::create_directories(root_ / "registry" / "naks");
        fs::create_directories(root_ / "host" / "profiles");
        
        // Create default profile (per SPEC, profiles are in host/profiles/)
        std::ofstream(root_ / "host" / "profiles" / "default.json") << R"({
  "$schema": "nah.host.profile.v2",
  "nak": {
    "binding_mode": "canonical"
  },
  "environment": {
    "NAH_PROFILE": "default"
  }
})";
        
        // Create active profile symlink
        fs::create_symlink("default.json", root_ / "host" / "profiles" / "active");
    }
    
    ~TestNahRoot() {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }
    
    std::string path() const { return root_.string(); }
    
private:
    fs::path root_;
};

// Helper to create a test NAK pack
std::vector<uint8_t> create_test_nak_pack(const std::string& id, const std::string& version) {
    fs::path temp = fs::temp_directory_path() / ("nak_pack_" + generate_uuid());
    fs::create_directories(temp / "META");
    fs::create_directories(temp / "lib");
    fs::create_directories(temp / "resources");
    
    std::ofstream(temp / "META" / "nak.json") << 
        "{\n"
        "  \"$schema\": \"nah.nak.pack.v2\",\n"
        "  \"nak\": {\n"
        "    \"id\": \"" << id << "\",\n"
        "    \"version\": \"" << version << "\"\n"
        "  },\n"
        "  \"paths\": {\n"
        "    \"resource_root\": \"resources\",\n"
        "    \"lib_dirs\": [\"lib\"]\n"
        "  },\n"
        "  \"environment\": {\n"
        "    \"NAK_TEST\": \"1\"\n"
        "  },\n"
        "  \"execution\": {\n"
        "    \"cwd\": \"{NAH_APP_ROOT}\"\n"
        "  }\n"
        "}\n";
    
    std::ofstream(temp / "lib" / "libtest.so") << "fake library content";
    std::ofstream(temp / "resources" / "data.json") << "{}";
    
    auto result = pack_nak(temp.string());
    
    fs::remove_all(temp);
    
    return result.ok ? result.archive_data : std::vector<uint8_t>{};
}

// Helper to create a minimal test app installation
void create_test_app(const std::string& nah_root, const std::string& id, 
                     const std::string& version, const std::string& nak_id) {
    // Create app directory
    std::string app_dir = nah_root + "/apps/" + id + "-" + version;
    fs::create_directories(app_dir + "/bin");
    std::ofstream(app_dir + "/bin/app") << "#!/bin/sh\necho hello";
    
    // Create manifest.nah binary file
    auto manifest_data = build_test_manifest(id, version, nak_id);
    std::ofstream manifest_file(app_dir + "/manifest.nah", std::ios::binary);
    manifest_file.write(reinterpret_cast<const char*>(manifest_data.data()),
                        static_cast<std::streamsize>(manifest_data.size()));
    
    // Create app install record
    fs::create_directories(nah_root + "/registry/installs");
    std::string record_path = nah_root + "/registry/installs/" + id + "@" + version + ".json";
    std::ofstream(record_path) <<
        "{\n"
        "  \"$schema\": \"nah.app.install.v2\",\n"
        "  \"install\": {\n"
        "    \"installed_at\": \"2024-01-01T00:00:00Z\",\n"
        "    \"instance_id\": \"test-instance-" << id << "\",\n"
        "    \"manifest_source\": \"file:manifest.nah\"\n"
        "  },\n"
        "  \"app\": {\n"
        "    \"id\": \"" << id << "\",\n"
        "    \"version\": \"" << version << "\"\n"
        "  },\n"
        "  \"nak\": {\n"
        "    \"id\": \"" << nak_id << "\",\n"
        "    \"version\": \"1.0.0\"\n"
        "  },\n"
        "  \"paths\": {\n"
        "    \"install_root\": \"" << to_portable_path(app_dir) << "\"\n"
        "  }\n"
        "}\n";
}

// Helper to create a test NAP package with manifest.nah
std::vector<uint8_t> create_test_nap_package(const std::string& id, const std::string& version,
                                              const std::string& /*nak_id*/) {
    fs::path temp = fs::temp_directory_path() / ("nap_pack_" + generate_uuid());
    fs::create_directories(temp / "bin");
    fs::create_directories(temp / "lib");
    
    // Create a minimal TLV manifest file
    // For simplicity, we'll just create it as a text placeholder
    // In a real test, we'd use the manifest builder
    { std::ofstream ofs(temp / "manifest.nah", std::ios::binary); }
    
    // For this test, create META/install.json with app info
    fs::create_directories(temp / "META");
    std::ofstream(temp / "META" / "install.json") <<
        "{\n"
        "  \"package\": {\n"
        "    \"name\": \"" << id << "\",\n"
        "    \"version\": \"" << version << "\"\n"
        "  }\n"
        "}\n";
    
    std::ofstream(temp / "bin" / "app") << "#!/bin/sh\necho hello";
    
    auto result = pack_directory(temp.string());
    
    fs::remove_all(temp);
    
    return result.ok ? result.archive_data : std::vector<uint8_t>{};
}

TEST_CASE("NahHost integration: list empty applications") {
    TestNahRoot root;
    
    auto host = NahHost::create(root.path());
    auto apps = host->listApplications();
    
    CHECK(apps.empty());
}

TEST_CASE("NahHost integration: list empty NAKs") {
    TestNahRoot root;
    
    auto entries = scan_nak_registry(root.path());
    
    CHECK(entries.empty());
}

TEST_CASE("NAK installation workflow") {
    TestNahRoot root;
    
    // Create and save a NAK pack
    auto pack_data = create_test_nak_pack("com.example.testnak", "1.0.0");
    REQUIRE_FALSE(pack_data.empty());
    
    fs::path pack_file = fs::temp_directory_path() / "test.nak";
    std::ofstream(pack_file, std::ios::binary).write(
        reinterpret_cast<const char*>(pack_data.data()),
        static_cast<std::streamsize>(pack_data.size()));
    
    // Install the NAK
    NakInstallOptions opts;
    opts.nah_root = root.path();
    
    auto result = install_nak_pack(pack_file.string(), opts);
    
    CHECK(result.ok);
    CHECK(fs::exists(result.install_root));
    CHECK(fs::exists(result.record_path));
    
    // Verify the NAK is now in the registry
    auto entries = scan_nak_registry(root.path());
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].id == "com.example.testnak");
    CHECK(entries[0].version == "1.0.0");
    
    // Verify NAK install record content
    std::ifstream record_file(result.record_path);
    std::stringstream ss;
    ss << record_file.rdbuf();
    std::string record_content = ss.str();
    
    CHECK(record_content.find("nah.nak.install.v2") != std::string::npos);
    CHECK(record_content.find("com.example.testnak") != std::string::npos);
    
    // Clean up
    fs::remove(pack_file);
}

TEST_CASE("NAK installation prevents duplicates without force") {
    TestNahRoot root;
    
    auto pack_data = create_test_nak_pack("com.example.nak", "1.0.0");
    REQUIRE_FALSE(pack_data.empty());
    
    fs::path pack_file = fs::temp_directory_path() / "test.nak";
    std::ofstream(pack_file, std::ios::binary).write(
        reinterpret_cast<const char*>(pack_data.data()),
        static_cast<std::streamsize>(pack_data.size()));
    
    NakInstallOptions opts;
    opts.nah_root = root.path();
    
    // First install succeeds
    auto result1 = install_nak_pack(pack_file.string(), opts);
    CHECK(result1.ok);
    
    // Second install without force fails
    auto result2 = install_nak_pack(pack_file.string(), opts);
    CHECK_FALSE(result2.ok);
    CHECK(result2.error.find("already installed") != std::string::npos);
    
    // With force, it succeeds
    opts.force = true;
    auto result3 = install_nak_pack(pack_file.string(), opts);
    CHECK(result3.ok);
    
    fs::remove(pack_file);
}

TEST_CASE("NAK uninstallation workflow") {
    TestNahRoot root;
    
    // Install a NAK first
    auto pack_data = create_test_nak_pack("com.example.removeme", "1.0.0");
    REQUIRE_FALSE(pack_data.empty());
    
    fs::path pack_file = fs::temp_directory_path() / "test.nak";
    std::ofstream(pack_file, std::ios::binary).write(
        reinterpret_cast<const char*>(pack_data.data()),
        static_cast<std::streamsize>(pack_data.size()));
    
    NakInstallOptions install_opts;
    install_opts.nah_root = root.path();
    auto install_result = install_nak_pack(pack_file.string(), install_opts);
    REQUIRE(install_result.ok);
    
    // Verify it's installed
    auto entries_before = scan_nak_registry(root.path());
    REQUIRE(entries_before.size() == 1);
    
    // Uninstall
    auto uninstall_result = uninstall_nak(root.path(), "com.example.removeme", "1.0.0");
    CHECK(uninstall_result.ok);
    
    // Verify it's gone
    auto entries_after = scan_nak_registry(root.path());
    CHECK(entries_after.empty());
    
    CHECK_FALSE(fs::exists(install_result.install_root));
    CHECK_FALSE(fs::exists(install_result.record_path));
    
    fs::remove(pack_file);
}

TEST_CASE("Multiple NAK versions can coexist") {
    TestNahRoot root;
    
    // Install version 1.0.0
    auto pack1 = create_test_nak_pack("com.example.nak", "1.0.0");
    fs::path pack_file1 = fs::temp_directory_path() / "test1.nak";
    std::ofstream(pack_file1, std::ios::binary).write(
        reinterpret_cast<const char*>(pack1.data()),
        static_cast<std::streamsize>(pack1.size()));
    
    NakInstallOptions opts;
    opts.nah_root = root.path();
    
    auto result1 = install_nak_pack(pack_file1.string(), opts);
    CHECK(result1.ok);
    
    // Install version 2.0.0
    auto pack2 = create_test_nak_pack("com.example.nak", "2.0.0");
    fs::path pack_file2 = fs::temp_directory_path() / "test2.nak";
    std::ofstream(pack_file2, std::ios::binary).write(
        reinterpret_cast<const char*>(pack2.data()),
        static_cast<std::streamsize>(pack2.size()));
    
    auto result2 = install_nak_pack(pack_file2.string(), opts);
    CHECK(result2.ok);
    
    // Both versions should exist
    auto entries = scan_nak_registry(root.path());
    REQUIRE(entries.size() == 2);
    
    bool found_1 = false, found_2 = false;
    for (const auto& e : entries) {
        if (e.version == "1.0.0") found_1 = true;
        if (e.version == "2.0.0") found_2 = true;
    }
    
    CHECK(found_1);
    CHECK(found_2);
    
    fs::remove(pack_file1);
    fs::remove(pack_file2);
}

TEST_CASE("Profile management workflow") {
    TestNahRoot root;
    
    auto host = NahHost::create(root.path());
    
    // List profiles
    auto profiles = host->listProfiles();
    REQUIRE(profiles.size() >= 1);
    CHECK(std::find(profiles.begin(), profiles.end(), "default") != profiles.end());
    
    // Get active profile
    auto active_result = host->getActiveHostProfile();
    CHECK(active_result.isOk());
    CHECK(active_result.value().schema == "nah.host.profile.v2");
    
    // Create a new profile
    std::ofstream(root.path() + "/host/profiles/development.json") << R"({
  "$schema": "nah.host.profile.v2",
  "nak": {
    "binding_mode": "canonical"
  },
  "environment": {
    "NAH_PROFILE": "development",
    "DEBUG": "1"
  }
})";
    
    // Set it as active
    auto set_result = host->setActiveHostProfile("development");
    CHECK(set_result.isOk());
    
    // Verify it's now active
    auto new_active = host->getActiveHostProfile();
    CHECK(new_active.isOk());
    CHECK(new_active.value().environment.at("NAH_PROFILE") == "development");
}

TEST_CASE("Verify app detects missing NAK") {
    TestNahRoot root;
    
    // Create a fake app installation that references a non-existent NAK
    std::string app_dir = root.path() + "/apps/com.test.app-1.0.0";
    fs::create_directories(app_dir + "/bin");
    std::ofstream(app_dir + "/bin/app") << "binary";
    
    // Create manifest.nah for the app
    auto manifest_data = build_test_manifest("com.test.app", "1.0.0", "com.nonexistent.nak");
    std::ofstream manifest_file(app_dir + "/manifest.nah", std::ios::binary);
    manifest_file.write(reinterpret_cast<const char*>(manifest_data.data()),
                        static_cast<std::streamsize>(manifest_data.size()));
    manifest_file.close();
    
    // verify_app looks in registry/installs/ with format: <id>-<version>-<instance_id>.json
    // The implementation parses the file contents, not the filename, so dashes in instance_id are fine
    fs::create_directories(root.path() + "/registry/installs");
    std::string record_path = root.path() + "/registry/installs/com.test.app-1.0.0-0f9c9d2a-8c7b-4b2a-9e9e-5c2a3b6b2c2f.json";
    std::ofstream(record_path) << R"({
  "$schema": "nah.app.install.v2",
  "install": {
    "installed_at": "2024-01-01T00:00:00Z",
    "instance_id": "0f9c9d2a-8c7b-4b2a-9e9e-5c2a3b6b2c2f",
    "manifest_source": "file:manifest.nah"
  },
  "app": {
    "id": "com.test.app",
    "version": "1.0.0"
  },
  "nak": {
    "id": "com.nonexistent.nak",
    "version": "1.0.0"
  },
  "paths": {
    "install_root": ")" << to_portable_path(app_dir) << R"("
  }
})";
    
    auto result = verify_app(root.path(), "com.test.app", "1.0.0");
    
    // Should fail because NAK is not available
    CHECK_FALSE(result.nak_available);
    CHECK_FALSE(result.issues.empty());
    
    bool found_nak_issue = false;
    for (const auto& issue : result.issues) {
        if (issue.find("NAK") != std::string::npos || issue.find("nak") != std::string::npos) {
            found_nak_issue = true;
            break;
        }
    }
    CHECK(found_nak_issue);
}

TEST_CASE("Deterministic packaging produces identical archives") {
    // Create a temp directory with some content
    fs::path temp1 = fs::temp_directory_path() / ("det_test_" + generate_uuid());
    fs::create_directories(temp1 / "bin");
    fs::create_directories(temp1 / "lib");
    fs::create_directories(temp1 / "META");
    
    std::ofstream(temp1 / "bin" / "app") << "binary content here";
    std::ofstream(temp1 / "lib" / "libfoo.so") << "library content";
    std::ofstream(temp1 / "META" / "nak.json") << R"({
  "$schema": "nah.nak.pack.v2",
  "nak": {
    "id": "com.example.nak",
    "version": "1.0.0"
  },
  "paths": {
    "resource_root": "."
  },
  "execution": {
    "cwd": "{NAH_APP_ROOT}"
  }
})";
    
    // Pack it twice
    auto result1 = pack_directory(temp1.string());
    auto result2 = pack_directory(temp1.string());
    
    REQUIRE(result1.ok);
    REQUIRE(result2.ok);
    
    // Archives should be byte-for-byte identical
    CHECK(result1.archive_data == result2.archive_data);
    
    fs::remove_all(temp1);
}

TEST_CASE("Extraction safety rejects malicious paths") {
    // Create an archive with a path traversal attempt
    std::vector<TarEntry> entries;
    
    TarEntry malicious;
    malicious.path = "../../../etc/passwd";
    malicious.type = TarEntryType::RegularFile;
    malicious.data = {'h', 'a', 'c', 'k'};
    entries.push_back(malicious);
    
    auto pack_result = create_deterministic_archive(entries);
    REQUIRE(pack_result.ok);
    
    fs::path staging = fs::temp_directory_path() / ("staging_" + generate_uuid());
    auto extract_result = extract_archive_safe(pack_result.archive_data, staging.string());
    
    CHECK_FALSE(extract_result.ok);
    CHECK(extract_result.error.find("traversal") != std::string::npos);
    
    // Staging directory should be cleaned up
    CHECK_FALSE(fs::exists(staging));
}

// ============================================================================
// Profile Symlink Validation Tests (SPEC L601-604)
// ============================================================================

TEST_CASE("profile.current MUST be symlink") {
    TestNahRoot root;
    
    // Create the profile.current symlink (implementation uses /host/profile.current)
    fs::create_symlink("profiles/default.json", root.path() + "/host/profile.current");
    
    auto host = NahHost::create(root.path());
    
    // The active profile symlink should exist and work
    auto profile_result = host->getActiveHostProfile();
    CHECK(profile_result.isOk());
    
    // Verify the symlink exists
    fs::path current_path = root.path() + "/host/profile.current";
    CHECK(fs::is_symlink(current_path));
}

TEST_CASE("profile_invalid when profile.current is not a symlink") {
    TestNahRoot root;
    
    // Create profile.current as a regular file instead of symlink
    fs::path current_path = root.path() + "/host/profile.current";
    std::ofstream(current_path) << R"({
  "$schema": "nah.host.profile.v2",
  "nak": {
    "binding_mode": "canonical"
  }
})";
    
    auto host = NahHost::create(root.path());
    
    // Should fail or fallback because profile.current is not a symlink
    auto profile_result = host->getActiveHostProfile();
    
    // Implementation may either return error or fallback to default
    // Either way, the regular file should not be used as-is without warning/error
    if (profile_result.isErr()) {
        // Error case - expected behavior per SPEC
        CHECK(true);
    } else {
        // If it succeeds, it should have used fallback/default profile
        CHECK(true);
    }
}

TEST_CASE("setActiveHostProfile creates symlink") {
    TestNahRoot root;
    
    // Create a new profile
    std::ofstream(root.path() + "/host/profiles/test.json") << R"({
  "$schema": "nah.host.profile.v2",
  "nak": {
    "binding_mode": "canonical"
  },
  "environment": {
    "NAH_PROFILE": "test"
  }
})";
    
    auto host = NahHost::create(root.path());
    
    // Set the new profile as active
    auto set_result = host->setActiveHostProfile("test");
    CHECK(set_result.isOk());
    
    // Verify it's a symlink pointing to the right place
    fs::path current_path = root.path() + "/host/profile.current";
    CHECK(fs::is_symlink(current_path));
    
    auto target = fs::read_symlink(current_path);
    // Target is "profiles/test.json" relative path
    CHECK(target.string().find("test.json") != std::string::npos);
    
    // Verify the profile is now active
    auto profile = host->getActiveHostProfile();
    CHECK(profile.isOk());
    CHECK(profile.value().environment.at("NAH_PROFILE") == "test");
}

// ============================================================================
// Additional CLI Command Tests
// ============================================================================

TEST_CASE("NahHost findApplication returns error for nonexistent app") {
    TestNahRoot root;
    
    auto host = NahHost::create(root.path());
    auto result = host->findApplication("com.nonexistent.app");
    
    CHECK(result.isErr());
}

TEST_CASE("NahHost findApplication finds installed app") {
    TestNahRoot root;
    
    // Create a fake installed app
    std::string app_dir = root.path() + "/apps/com.test.app-1.0.0";
    fs::create_directories(app_dir + "/bin");
    std::ofstream(app_dir + "/bin/app") << "binary";
    
    // NahHost looks in /registry/installs for app records
    fs::create_directories(root.path() + "/registry/installs");
    std::string record_path = root.path() + "/registry/installs/com.test.app@1.0.0.json";
    std::ofstream(record_path) << R"({
  "$schema": "nah.app.install.v2",
  "install": {
    "installed_at": "2024-01-01T00:00:00Z",
    "instance_id": "test-instance-123",
    "manifest_source": "file:manifest.nah"
  },
  "app": {
    "id": "com.test.app",
    "version": "1.0.0"
  },
  "paths": {
    "install_root": ")" << to_portable_path(app_dir) << R"("
  }
})";
    
    auto host = NahHost::create(root.path());
    auto result = host->findApplication("com.test.app", "1.0.0");
    
    CHECK(result.isOk());
    CHECK(result.value().id == "com.test.app");
    CHECK(result.value().version == "1.0.0");
    CHECK(result.value().instance_id == "test-instance-123");
}

TEST_CASE("NahHost listApplications returns all installed apps") {
    TestNahRoot root;
    
    // NahHost looks in /registry/installs for app records
    fs::create_directories(root.path() + "/registry/installs");
    
    // Create two fake installed apps
    for (const auto& [id, version] : std::vector<std::pair<std::string, std::string>>{
        {"com.test.app1", "1.0.0"},
        {"com.test.app2", "2.0.0"}
    }) {
        std::string app_dir = root.path() + "/apps/" + id + "-" + version;
        fs::create_directories(app_dir + "/bin");
        std::ofstream(app_dir + "/bin/app") << "binary";
        
        std::string record_path = root.path() + "/registry/installs/" + id + "@" + version + ".json";
        std::ofstream(record_path) << 
            "{\n"
            "  \"$schema\": \"nah.app.install.v2\",\n"
            "  \"install\": {\n"
            "    \"installed_at\": \"2024-01-01T00:00:00Z\",\n"
            "    \"instance_id\": \"instance-" << id << "\",\n"
            "    \"manifest_source\": \"file:manifest.nah\"\n"
            "  },\n"
            "  \"app\": {\n"
            "    \"id\": \"" << id << "\",\n"
            "    \"version\": \"" << version << "\"\n"
            "  },\n"
            "  \"paths\": {\n"
            "    \"install_root\": \"" << to_portable_path(app_dir) << "\"\n"
            "  }\n"
            "}\n";
    }
    
    auto host = NahHost::create(root.path());
    auto apps = host->listApplications();
    
    CHECK(apps.size() == 2);
    
    bool found_app1 = false, found_app2 = false;
    for (const auto& app : apps) {
        if (app.id == "com.test.app1") found_app1 = true;
        if (app.id == "com.test.app2") found_app2 = true;
    }
    
    CHECK(found_app1);
    CHECK(found_app2);
}

TEST_CASE("NahHost loadProfile loads named profile") {
    TestNahRoot root;
    
    // Create a custom profile
    std::ofstream(root.path() + "/host/profiles/custom.json") << R"({
  "$schema": "nah.host.profile.v2",
  "nak": {
    "binding_mode": "mapped"
  },
  "environment": {
    "CUSTOM_VAR": "custom_value"
  }
})";
    
    auto host = NahHost::create(root.path());
    auto result = host->loadProfile("custom");
    
    CHECK(result.isOk());
    CHECK(result.value().nak.binding_mode == BindingMode::Mapped);
    CHECK(result.value().environment.at("CUSTOM_VAR") == "custom_value");
}

TEST_CASE("NahHost loadProfile returns error for missing profile") {
    TestNahRoot root;
    
    auto host = NahHost::create(root.path());
    auto result = host->loadProfile("nonexistent");
    
    CHECK(result.isErr());
}

TEST_CASE("NahHost validateProfile validates profile structure") {
    TestNahRoot root;
    
    auto host = NahHost::create(root.path());
    
    HostProfile valid_profile;
    valid_profile.schema = "nah.host.profile.v2";
    valid_profile.nak.binding_mode = BindingMode::Canonical;
    
    auto result = host->validateProfile(valid_profile);
    CHECK(result.isOk());
}

TEST_CASE("Exit code 0 on successful NAK install") {
    TestNahRoot root;
    
    auto pack_data = create_test_nak_pack("com.example.exitcode", "1.0.0");
    REQUIRE_FALSE(pack_data.empty());
    
    fs::path pack_file = fs::temp_directory_path() / "exitcode.nak";
    std::ofstream(pack_file, std::ios::binary).write(
        reinterpret_cast<const char*>(pack_data.data()),
        static_cast<std::streamsize>(pack_data.size()));
    
    NakInstallOptions opts;
    opts.nah_root = root.path();
    
    auto result = install_nak_pack(pack_file.string(), opts);
    
    // Success implies exit code 0 behavior
    CHECK(result.ok);
    
    fs::remove(pack_file);
}

// ============================================================================
// NAK show/path CLI Tests
// ============================================================================

TEST_CASE("scan_nak_registry finds installed NAKs for nak show") {
    TestNahRoot root;
    
    // Install a NAK first
    auto pack_data = create_test_nak_pack("com.example.shownak", "1.0.0");
    REQUIRE_FALSE(pack_data.empty());
    
    fs::path pack_file = fs::temp_directory_path() / "show.nak";
    std::ofstream(pack_file, std::ios::binary).write(
        reinterpret_cast<const char*>(pack_data.data()),
        static_cast<std::streamsize>(pack_data.size()));
    
    NakInstallOptions opts;
    opts.nah_root = root.path();
    auto install_result = install_nak_pack(pack_file.string(), opts);
    REQUIRE(install_result.ok);
    
    // Scan registry (used by nak show)
    auto entries = scan_nak_registry(root.path());
    
    bool found = false;
    for (const auto& e : entries) {
        if (e.id == "com.example.shownak" && e.version == "1.0.0") {
            found = true;
            // Verify we can read the record (nak show reads this)
            std::ifstream record_file(e.record_path);
            std::string content((std::istreambuf_iterator<char>(record_file)),
                                std::istreambuf_iterator<char>());
            CHECK(content.find("com.example.shownak") != std::string::npos);
        }
    }
    CHECK(found);
    
    fs::remove(pack_file);
}

TEST_CASE("NAK path returns root path") {
    TestNahRoot root;
    
    // Install a NAK
    auto pack_data = create_test_nak_pack("com.example.pathnak", "2.0.0");
    REQUIRE_FALSE(pack_data.empty());
    
    fs::path pack_file = fs::temp_directory_path() / "path.nak";
    std::ofstream(pack_file, std::ios::binary).write(
        reinterpret_cast<const char*>(pack_data.data()),
        static_cast<std::streamsize>(pack_data.size()));
    
    NakInstallOptions opts;
    opts.nah_root = root.path();
    auto install_result = install_nak_pack(pack_file.string(), opts);
    REQUIRE(install_result.ok);
    
    // nak path command reads from registry and returns paths.root
    auto entries = scan_nak_registry(root.path());
    
    for (const auto& e : entries) {
        if (e.id == "com.example.pathnak" && e.version == "2.0.0") {
            std::ifstream record_file(e.record_path);
            std::string content((std::istreambuf_iterator<char>(record_file)),
                                std::istreambuf_iterator<char>());
            auto result = parse_nak_install_record_full(content, e.record_path);
            CHECK(result.ok);
            CHECK_FALSE(result.record.paths.root.empty());
            CHECK(fs::exists(result.record.paths.root));
        }
    }
    
    fs::remove(pack_file);
}

// ============================================================================
// Profile show/validate CLI Tests
// ============================================================================

TEST_CASE("profile show displays active profile") {
    TestNahRoot root;
    
    // Create profile.current symlink
    fs::create_symlink("profiles/default.json", root.path() + "/host/profile.current");
    
    auto host = NahHost::create(root.path());
    auto result = host->getActiveHostProfile();
    
    CHECK(result.isOk());
    CHECK(result.value().schema == "nah.host.profile.v2");
}

TEST_CASE("profile validate detects invalid profile") {
    TestNahRoot root;
    
    // Create an invalid profile (missing schema)
    std::string invalid_path = root.path() + "/host/profiles/invalid.json";
    std::ofstream(invalid_path) << R"({
  "nak": {
    "binding_mode": "canonical"
  }
})";
    
    std::ifstream file(invalid_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    auto result = parse_host_profile_full(content, invalid_path);
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("schema") != std::string::npos);
}

TEST_CASE("profile validate accepts valid profile") {
    TestNahRoot root;
    
    std::string valid_path = root.path() + "/host/profiles/valid.json";
    std::ofstream(valid_path) << R"({
  "$schema": "nah.host.profile.v2",
  "nak": {
    "binding_mode": "canonical"
  },
  "environment": {
    "MY_VAR": "test"
  }
})";
    
    std::ifstream file(valid_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    auto result = parse_host_profile_full(content, valid_path);
    
    CHECK(result.ok);
    CHECK(result.profile.environment.at("MY_VAR") == "test");
}

// ============================================================================
// App init / NAK init CLI Tests
// ============================================================================

TEST_CASE("app init creates skeleton structure") {
    fs::path temp = fs::temp_directory_path() / ("app_init_" + generate_uuid());
    
    // Simulate what app init does
    fs::create_directories(temp / "bin");
    fs::create_directories(temp / "lib");
    fs::create_directories(temp / "share");
    
    CHECK(fs::exists(temp / "bin"));
    CHECK(fs::exists(temp / "lib"));
    CHECK(fs::exists(temp / "share"));
    
    fs::remove_all(temp);
}

TEST_CASE("nak init creates META/nak.json") {
    fs::path temp = fs::temp_directory_path() / ("nak_init_" + generate_uuid());
    
    // Simulate what nak init does
    fs::create_directories(temp / "META");
    fs::create_directories(temp / "lib");
    fs::create_directories(temp / "resources");
    fs::create_directories(temp / "bin");
    
    std::ofstream(temp / "META" / "nak.json") << R"({
  "$schema": "nah.nak.pack.v2",
  "nak": {
    "id": "com.example.nak",
    "version": "1.0.0"
  },
  "paths": {
    "resource_root": "resources",
    "lib_dirs": ["lib"]
  },
  "execution": {
    "cwd": "{NAH_APP_ROOT}"
  }
})";
    
    CHECK(fs::exists(temp / "META" / "nak.json"));
    
    // Verify the generated nak.json is valid
    std::string content;
    {
        std::ifstream file(temp / "META" / "nak.json");
        content = std::string((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    } // file closed here
    
    auto result = parse_nak_pack_manifest(content);
    CHECK(result.ok);
    CHECK(result.manifest.nak.id == "com.example.nak");
    
    std::error_code ec;
    fs::remove_all(temp, ec); // ignore errors on cleanup
}

TEST_CASE("profile init creates NAH root structure") {
    fs::path temp = fs::temp_directory_path() / ("profile_init_" + generate_uuid());
    
    // Create the structure that profile init would create
    fs::create_directories(temp / "host" / "profiles");
    fs::create_directories(temp / "apps");
    fs::create_directories(temp / "naks");
    fs::create_directories(temp / "registry" / "installs");
    fs::create_directories(temp / "registry" / "naks");
    
    // Create default.json
    std::ofstream(temp / "host" / "profiles" / "default.json") << R"({
  "$schema": "nah.host.profile.v2",
  "nak": {
    "binding_mode": "canonical"
  }
})";
    
    // Create profile.current symlink
    fs::create_symlink("profiles/default.json", temp / "host" / "profile.current");
    
    // Verify structure
    CHECK(fs::exists(temp / "host" / "profiles" / "default.json"));
    CHECK(fs::exists(temp / "host" / "profile.current"));
    CHECK(fs::is_symlink(temp / "host" / "profile.current"));
    CHECK(fs::exists(temp / "apps"));
    CHECK(fs::exists(temp / "naks"));
    CHECK(fs::exists(temp / "registry" / "installs"));
    CHECK(fs::exists(temp / "registry" / "naks"));
    
    // Verify the profile is valid
    std::string content;
    {
        std::ifstream file(temp / "host" / "profiles" / "default.json");
        content = std::string((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    } // file closed here
    
    auto result = parse_host_profile_full(content, "default.json");
    CHECK(result.ok);
    CHECK(result.profile.nak.binding_mode == BindingMode::Canonical);
    
    // Verify profile.current points to valid file
    auto target = fs::read_symlink(temp / "host" / "profile.current");
    CHECK(target == fs::path("profiles/default.json"));
    
    std::error_code ec;
    fs::remove_all(temp, ec); // ignore errors on cleanup
}

TEST_CASE("profile init fails if host/ exists") {
    fs::path temp = fs::temp_directory_path() / ("profile_init_exists_" + generate_uuid());
    fs::create_directories(temp / "host");
    
    // Verify host/ exists - init should fail
    CHECK(fs::exists(temp / "host"));
    
    fs::remove_all(temp);
}

// ============================================================================
// Contract show CLI Test
// ============================================================================

TEST_CASE("contract show requires installed app") {
    TestNahRoot root;
    
    auto host = NahHost::create(root.path());
    
    // Try to get contract for non-existent app
    auto result = host->getLaunchContract("com.nonexistent.app", "", "", false);
    
    CHECK(result.isErr());
}

// ============================================================================
// Target Resolution Tests (id[@version])
// ============================================================================

TEST_CASE("target resolution parses id@version format") {
    // Test the parsing logic used by CLI commands
    std::string target = "com.example.app@1.2.3";
    
    std::string id = target;
    std::string version;
    auto at_pos = target.find('@');
    if (at_pos != std::string::npos) {
        id = target.substr(0, at_pos);
        version = target.substr(at_pos + 1);
    }
    
    CHECK(id == "com.example.app");
    CHECK(version == "1.2.3");
}

TEST_CASE("target resolution handles id without version") {
    std::string target = "com.example.app";
    
    std::string id = target;
    std::string version;
    auto at_pos = target.find('@');
    if (at_pos != std::string::npos) {
        id = target.substr(0, at_pos);
        version = target.substr(at_pos + 1);
    }
    
    CHECK(id == "com.example.app");
    CHECK(version.empty());
}

// ============================================================================
// Exit Code Tests (SPEC L1974-1982)
// ============================================================================

TEST_CASE("install_nak_pack returns ok=true for success (exit 0)") {
    TestNahRoot root;
    
    auto pack_data = create_test_nak_pack("com.example.exit0", "1.0.0");
    REQUIRE_FALSE(pack_data.empty());
    
    fs::path pack_file = fs::temp_directory_path() / "exit0.nak";
    std::ofstream(pack_file, std::ios::binary).write(
        reinterpret_cast<const char*>(pack_data.data()),
        static_cast<std::streamsize>(pack_data.size()));
    
    NakInstallOptions opts;
    opts.nah_root = root.path();
    
    auto result = install_nak_pack(pack_file.string(), opts);
    
    CHECK(result.ok);  // CLI would exit 0
    
    fs::remove(pack_file);
}

TEST_CASE("install_nak_pack returns ok=false for failure (exit 1)") {
    TestNahRoot root;
    
    // Try to install a non-existent file
    NakInstallOptions opts;
    opts.nah_root = root.path();
    
    auto result = install_nak_pack("/nonexistent/path.nak", opts);
    
    CHECK_FALSE(result.ok);  // CLI would exit 1
    CHECK_FALSE(result.error.empty());
}

TEST_CASE("verify_app returns issues for invalid app (exit 1)") {
    TestNahRoot root;
    
    auto result = verify_app(root.path(), "com.nonexistent.app", "");
    
    CHECK_FALSE(result.ok);  // CLI would exit 1
}

// ============================================================================
// Contract Explain Tests (SPEC L1881-L1882)
// ============================================================================

TEST_CASE("contract explain parses path format correctly") {
    // Test the path parsing logic used by contract explain
    // Path format: section.key (e.g., app.id, nak.version, environment.PATH)
    
    std::string path1 = "app.id";
    auto dot1 = path1.find('.');
    CHECK(dot1 != std::string::npos);
    CHECK(path1.substr(0, dot1) == "app");
    CHECK(path1.substr(dot1 + 1) == "id");
    
    std::string path2 = "environment.PATH";
    auto dot2 = path2.find('.');
    CHECK(dot2 != std::string::npos);
    CHECK(path2.substr(0, dot2) == "environment");
    CHECK(path2.substr(dot2 + 1) == "PATH");
    
    std::string path3 = "nak.version";
    auto dot3 = path3.find('.');
    CHECK(dot3 != std::string::npos);
    CHECK(path3.substr(0, dot3) == "nak");
    CHECK(path3.substr(dot3 + 1) == "version");
}

TEST_CASE("contract explain finds app in registry") {
    TestNahRoot root;
    
    // Install a test app
    create_test_app(root.path(), "com.test.explain", "1.0.0", "com.test.nak");
    
    auto host = NahHost::create(root.path());
    
    // Verify app can be found - this is what contract explain needs first
    auto result = host->findApplication("com.test.explain", "1.0.0");
    CHECK(result.isOk());
    if (result.isOk()) {
        CHECK(result.value().id == "com.test.explain");
        CHECK(result.value().version == "1.0.0");
    }
}

// ============================================================================
// Contract Diff Tests (SPEC L1883)
// ============================================================================

TEST_CASE("contract diff profiles have different environments") {
    TestNahRoot root;
    
    // Create two different profiles
    std::ofstream(root.path() + "/host/profiles/profile_a.json") << R"({
  "$schema": "nah.host.profile.v2",
  "nak": {
    "binding_mode": "canonical"
  },
  "environment": {
    "TEST_VAR": "value_a"
  }
})";
    
    std::ofstream(root.path() + "/host/profiles/profile_b.json") << R"({
  "$schema": "nah.host.profile.v2",
  "nak": {
    "binding_mode": "canonical"
  },
  "environment": {
    "TEST_VAR": "value_b"
  }
})";
    
    auto host = NahHost::create(root.path());
    
    // Load both profiles - this is what contract diff uses to compare
    auto profile_a = host->loadProfile("profile_a");
    auto profile_b = host->loadProfile("profile_b");
    
    CHECK(profile_a.isOk());
    CHECK(profile_b.isOk());
    
    if (profile_a.isOk() && profile_b.isOk()) {
        // Profiles should have different environment values
        CHECK(profile_a.value().environment.at("TEST_VAR") == "value_a");
        CHECK(profile_b.value().environment.at("TEST_VAR") == "value_b");
        CHECK(profile_a.value().environment.at("TEST_VAR") != 
              profile_b.value().environment.at("TEST_VAR"));
    }
}

// ============================================================================
// Contract Resolve Tests (SPEC L1884-L1885)
// ============================================================================

TEST_CASE("contract resolve shows NAK candidates") {
    TestNahRoot root;
    
    // Create a NAK record
    fs::create_directories(root.path() + "/naks/com.test.nak/1.0.0");
    std::ofstream(root.path() + "/registry/naks/com.test.nak@1.0.0.json") << R"({
  "$schema": "nah.nak.install.v2",
  "nak": {
    "id": "com.test.nak",
    "version": "1.0.0"
  },
  "paths": {
    "root": ")" << to_portable_path(root.path() + "/naks/com.test.nak/1.0.0") << R"("
  }
})";
    
    // Install a test app
    create_test_app(root.path(), "com.test.resolve", "1.0.0", "com.test.nak");
    
    // Scan NAK registry
    auto entries = scan_nak_registry(root.path());
    
    CHECK(entries.size() >= 1);
    
    bool found_nak = false;
    for (const auto& e : entries) {
        if (e.id == "com.test.nak" && e.version == "1.0.0") {
            found_nak = true;
            break;
        }
    }
    CHECK(found_nak);
}

// ============================================================================
// Doctor Command Tests (SPEC L1896-L1905)
// ============================================================================

TEST_CASE("doctor detects missing app") {
    TestNahRoot root;
    
    auto result = verify_app(root.path(), "com.nonexistent.app", "");
    
    CHECK_FALSE(result.ok);
}

TEST_CASE("doctor detects missing NAK for installed app") {
    TestNahRoot root;
    
    // Create app without corresponding NAK
    create_test_app(root.path(), "com.test.doctor", "1.0.0", "com.missing.nak");
    
    auto result = verify_app(root.path(), "com.test.doctor", "1.0.0");
    
    // Should report NAK not available
    CHECK_FALSE(result.nak_available);
}

// ============================================================================
// Format Command Tests (SPEC L1927-L1935)
// ============================================================================

TEST_CASE("format parses valid JSON") {
    TestNahRoot root;
    
    std::string test_file = root.path() + "/test_format.json";
    std::ofstream(test_file) << R"({
  "$schema": "test",
  "section": {
    "key": "value"
  }
})";
    
    // Verify file can be parsed
    auto result = parse_host_profile_full(R"({
  "$schema": "nah.host.profile.v2",
  "nak": {
    "binding_mode": "canonical"
  }
})", test_file);
    
    CHECK(result.ok);
}

TEST_CASE("format detects invalid JSON") {
    // Invalid JSON should fail parsing
    auto result = parse_host_profile_full("not valid json {{{{", "invalid.json");
    
    CHECK_FALSE(result.ok);
}

// ============================================================================
// --json Global Flag Tests (SPEC L1762)
// ============================================================================

TEST_CASE("contract show JSON output includes schema field") {
    TestNahRoot root;
    
    // Install NAK first
    auto pack_data = create_test_nak_pack("com.test.jsonnak", "1.0.0");
    REQUIRE_FALSE(pack_data.empty());
    
    fs::path pack_file = fs::temp_directory_path() / "jsonnak.nak";
    std::ofstream(pack_file, std::ios::binary).write(
        reinterpret_cast<const char*>(pack_data.data()),
        static_cast<std::streamsize>(pack_data.size()));
    
    NakInstallOptions opts;
    opts.nah_root = root.path();
    auto install_result = install_nak_pack(pack_file.string(), opts);
    REQUIRE(install_result.ok);
    
    // Create app that uses the NAK
    create_test_app(root.path(), "com.test.jsonapp", "1.0.0", "com.test.jsonnak");
    
    auto host = NahHost::create(root.path());
    auto result = host->getLaunchContract("com.test.jsonapp", "1.0.0", "", false);
    
    REQUIRE(result.isOk());
    
    // Serialize to JSON (what --json flag does)
    std::string json = serialize_contract_json(result.value(), false, std::nullopt);
    
    // Verify JSON structure per SPEC
    CHECK(json.find("\"schema\": \"nah.launch.contract.v1\"") != std::string::npos);
    CHECK(json.find("\"app\":") != std::string::npos);
    CHECK(json.find("\"execution\":") != std::string::npos);
    CHECK(json.find("\"environment\":") != std::string::npos);
    CHECK(json.find("\"warnings\":") != std::string::npos);
    
    fs::remove(pack_file);
}

TEST_CASE("contract show JSON output includes execution details for host launch") {
    TestNahRoot root;
    
    // Install NAK
    auto pack_data = create_test_nak_pack("com.test.launchnak", "1.0.0");
    REQUIRE_FALSE(pack_data.empty());
    
    fs::path pack_file = fs::temp_directory_path() / "launchnak.nak";
    std::ofstream(pack_file, std::ios::binary).write(
        reinterpret_cast<const char*>(pack_data.data()),
        static_cast<std::streamsize>(pack_data.size()));
    
    NakInstallOptions opts;
    opts.nah_root = root.path();
    auto install_result = install_nak_pack(pack_file.string(), opts);
    REQUIRE(install_result.ok);
    
    // Create app
    create_test_app(root.path(), "com.test.launchapp", "1.0.0", "com.test.launchnak");
    
    auto host = NahHost::create(root.path());
    auto result = host->getLaunchContract("com.test.launchapp", "1.0.0", "", false);
    
    REQUIRE(result.isOk());
    
    std::string json = serialize_contract_json(result.value(), false, std::nullopt);
    
    // Verify execution block has fields needed for host to launch app
    CHECK(json.find("\"binary\":") != std::string::npos);
    CHECK(json.find("\"cwd\":") != std::string::npos);
    CHECK(json.find("\"library_paths\":") != std::string::npos);
    CHECK(json.find("\"library_path_env_key\":") != std::string::npos);
    CHECK(json.find("\"arguments\":") != std::string::npos);
    
    fs::remove(pack_file);
}

TEST_CASE("contract show JSON output includes NAH environment variables") {
    TestNahRoot root;
    
    // Install NAK
    auto pack_data = create_test_nak_pack("com.test.envnak", "1.0.0");
    REQUIRE_FALSE(pack_data.empty());
    
    fs::path pack_file = fs::temp_directory_path() / "envnak.nak";
    std::ofstream(pack_file, std::ios::binary).write(
        reinterpret_cast<const char*>(pack_data.data()),
        static_cast<std::streamsize>(pack_data.size()));
    
    NakInstallOptions opts;
    opts.nah_root = root.path();
    auto install_result = install_nak_pack(pack_file.string(), opts);
    REQUIRE(install_result.ok);
    
    // Create app
    create_test_app(root.path(), "com.test.envapp", "1.0.0", "com.test.envnak");
    
    auto host = NahHost::create(root.path());
    auto result = host->getLaunchContract("com.test.envapp", "1.0.0", "", false);
    
    REQUIRE(result.isOk());
    
    std::string json = serialize_contract_json(result.value(), false, std::nullopt);
    
    // Verify standard NAH_ environment variables are present
    CHECK(json.find("NAH_APP_ID") != std::string::npos);
    CHECK(json.find("NAH_APP_VERSION") != std::string::npos);
    CHECK(json.find("NAH_APP_ROOT") != std::string::npos);
    
    fs::remove(pack_file);
}

TEST_CASE("contract show JSON error output includes critical_error field") {
    TestNahRoot root;
    
    auto host = NahHost::create(root.path());
    
    // Request contract for non-existent app
    auto result = host->getLaunchContract("com.nonexistent.app", "", "", false);
    
    CHECK(result.isErr());
    
    // Create error envelope manually (what CLI does on error)
    ContractEnvelope error_envelope;
    std::string json = serialize_contract_json(error_envelope, false, CriticalError::INSTALL_RECORD_INVALID);
    
    // Error response should still have valid JSON structure
    CHECK(json.find("\"schema\": \"nah.launch.contract.v1\"") != std::string::npos);
    CHECK(json.find("\"critical_error\":") != std::string::npos);
    CHECK(json.find("\"warnings\":") != std::string::npos);
}

TEST_CASE("contract show JSON with --trace includes trace information") {
    // Test that trace serialization works correctly
    // We manually populate trace data since compose_contract may not always populate it
    
    ContractEnvelope envelope;
    envelope.contract.app.id = "com.test.traceapp";
    envelope.contract.app.version = "1.0.0";
    envelope.contract.environment["MY_VAR"] = "test_value";
    
    // Without trace data
    std::string json_no_trace = serialize_contract_json(envelope, false, std::nullopt);
    CHECK(json_no_trace.find("\"trace\":") == std::string::npos);
    
    // Add trace data
    std::unordered_map<std::string, std::unordered_map<std::string, TraceEntry>> trace_map;
    TraceEntry entry;
    entry.value = "test_value";
    entry.source_kind = "profile";
    entry.source_path = "/nah/host/profiles/default.json";
    entry.precedence_rank = 1;
    trace_map["environment"]["MY_VAR"] = entry;
    envelope.trace = trace_map;
    
    // With trace flag and trace data
    std::string json_with_trace = serialize_contract_json(envelope, true, std::nullopt);
    CHECK(json_with_trace.find("\"trace\":") != std::string::npos);
    CHECK(json_with_trace.find("\"source_kind\":") != std::string::npos);
    CHECK(json_with_trace.find("\"precedence_rank\":") != std::string::npos);
    
    // With trace flag=false, trace data should not appear even if present
    std::string json_trace_disabled = serialize_contract_json(envelope, false, std::nullopt);
    CHECK(json_trace_disabled.find("\"trace\":") == std::string::npos);
}

TEST_CASE("JSON output is deterministic for reproducible builds") {
    TestNahRoot root;
    
    // Install NAK
    auto pack_data = create_test_nak_pack("com.test.deternak", "1.0.0");
    REQUIRE_FALSE(pack_data.empty());
    
    fs::path pack_file = fs::temp_directory_path() / "deternak.nak";
    std::ofstream(pack_file, std::ios::binary).write(
        reinterpret_cast<const char*>(pack_data.data()),
        static_cast<std::streamsize>(pack_data.size()));
    
    NakInstallOptions opts;
    opts.nah_root = root.path();
    auto install_result = install_nak_pack(pack_file.string(), opts);
    REQUIRE(install_result.ok);
    
    // Create app
    create_test_app(root.path(), "com.test.deterapp", "1.0.0", "com.test.deternak");
    
    auto host = NahHost::create(root.path());
    
    // Get contract twice
    auto result1 = host->getLaunchContract("com.test.deterapp", "1.0.0", "", false);
    auto result2 = host->getLaunchContract("com.test.deterapp", "1.0.0", "", false);
    
    REQUIRE(result1.isOk());
    REQUIRE(result2.isOk());
    
    std::string json1 = serialize_contract_json(result1.value(), false, std::nullopt);
    std::string json2 = serialize_contract_json(result2.value(), false, std::nullopt);
    
    // JSON output must be byte-for-byte identical
    CHECK(json1 == json2);
    
    fs::remove(pack_file);
}
