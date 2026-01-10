#include <doctest/doctest.h>
#include <nah/materializer.hpp>
#include <nah/packaging.hpp>
#include <nah/platform.hpp>
#include <nah/nak_record.hpp>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

using namespace nah;

// Helper to create temporary directory
class TempDir {
public:
    TempDir() {
        path_ = fs::temp_directory_path() / ("nah_mat_test_" + generate_uuid());
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

// ============================================================================
// Reference Parsing Tests
// ============================================================================

TEST_CASE("parse_artifact_reference accepts file: references") {
    auto ref = parse_artifact_reference("file:/path/to/pack.nak");
    CHECK(ref.type == ReferenceType::File);
    CHECK(ref.path_or_url == "/path/to/pack.nak");
    CHECK(ref.sha256_digest.empty());
    CHECK(ref.error.empty());
}

TEST_CASE("parse_artifact_reference accepts relative file: paths") {
    auto ref = parse_artifact_reference("file:./local/pack.nak");
    CHECK(ref.type == ReferenceType::File);
    CHECK(ref.path_or_url == "./local/pack.nak");
}

TEST_CASE("parse_artifact_reference rejects empty file path") {
    auto ref = parse_artifact_reference("file:");
    CHECK(ref.type == ReferenceType::Invalid);
    CHECK(ref.error.find("empty") != std::string::npos);
}

TEST_CASE("parse_artifact_reference accepts https: with sha256") {
    std::string url = "https://releases.example.com/sdk-1.0.0.nak";
    std::string digest = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    std::string reference = url + "#sha256=" + digest;
    
    auto ref = parse_artifact_reference(reference);
    CHECK(ref.type == ReferenceType::Https);
    CHECK(ref.path_or_url == url);
    CHECK(ref.sha256_digest == digest);
    CHECK(ref.error.empty());
}

TEST_CASE("parse_artifact_reference normalizes sha256 to lowercase") {
    std::string digest = "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789";
    std::string reference = "https://example.com/pack.nak#sha256=" + digest;
    
    auto ref = parse_artifact_reference(reference);
    CHECK(ref.type == ReferenceType::Https);
    CHECK(ref.sha256_digest == "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
}

TEST_CASE("parse_artifact_reference accepts https without sha256") {
    auto ref = parse_artifact_reference("https://example.com/pack.nak");
    CHECK(ref.type == ReferenceType::Https);
    CHECK(ref.path_or_url == "https://example.com/pack.nak");
    CHECK(ref.sha256_digest.empty());
    CHECK(ref.error.empty());
}

TEST_CASE("parse_artifact_reference rejects https with wrong fragment") {
    auto ref = parse_artifact_reference("https://example.com/pack.nak#md5=abc123");
    CHECK(ref.type == ReferenceType::Invalid);
    CHECK(ref.error.find("sha256") != std::string::npos);
}

TEST_CASE("parse_artifact_reference rejects sha256 with wrong length") {
    auto ref = parse_artifact_reference("https://example.com/pack.nak#sha256=tooshort");
    CHECK(ref.type == ReferenceType::Invalid);
    CHECK(ref.error.find("64") != std::string::npos);
}

TEST_CASE("parse_artifact_reference rejects sha256 with invalid chars") {
    std::string bad_digest = "zzzzzz0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    auto ref = parse_artifact_reference("https://example.com/pack.nak#sha256=" + bad_digest);
    CHECK(ref.type == ReferenceType::Invalid);
    CHECK(ref.error.find("invalid") != std::string::npos);
}

TEST_CASE("parse_artifact_reference rejects http (non-TLS)") {
    auto ref = parse_artifact_reference("http://example.com/pack.nak#sha256=abc123");
    CHECK(ref.type == ReferenceType::Invalid);
    CHECK(ref.error.find("HTTPS") != std::string::npos);
}

TEST_CASE("parse_artifact_reference rejects unknown schemes") {
    auto ref = parse_artifact_reference("ftp://example.com/pack.nak");
    CHECK(ref.type == ReferenceType::Invalid);
    CHECK(ref.error.find("unsupported") != std::string::npos);
}

TEST_CASE("parse_artifact_reference rejects empty reference") {
    auto ref = parse_artifact_reference("");
    CHECK(ref.type == ReferenceType::Invalid);
    CHECK(ref.error.find("empty") != std::string::npos);
}

// ============================================================================
// SHA-256 Tests
// ============================================================================

TEST_CASE("compute_sha256 computes correct hash for known data") {
    // SHA-256 of "hello" is well-known
    std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
    auto result = compute_sha256(data);
    
    CHECK(result.ok);
    // SHA-256("hello") = 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
    CHECK(result.hex_digest == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
}

TEST_CASE("compute_sha256 computes correct hash for empty data") {
    std::vector<uint8_t> data;
    auto result = compute_sha256(data);
    
    CHECK(result.ok);
    // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    CHECK(result.hex_digest == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("compute_sha256 from file works") {
    TempDir temp;
    std::string file_path = temp.path() + "/test.txt";
    
    std::ofstream file(file_path, std::ios::binary);
    file << "hello";
    file.close();
    
    auto result = compute_sha256(file_path);
    
    CHECK(result.ok);
    CHECK(result.hex_digest == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
}

TEST_CASE("compute_sha256 from nonexistent file fails") {
    auto result = compute_sha256("/nonexistent/file/path");
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("open") != std::string::npos);
}

TEST_CASE("verify_sha256 succeeds with matching digest") {
    std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
    std::string expected = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    
    Sha256VerifyResult result = verify_sha256(data, expected);
    
    CHECK(result.ok);
    CHECK(result.actual_digest == expected);
    CHECK(result.error.empty());
}

TEST_CASE("verify_sha256 succeeds with uppercase expected digest") {
    std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
    std::string expected = "2CF24DBA5FB0A30E26E83B2AC5B9E29E1B161E5C1FA7425E73043362938B9824";
    
    auto result = verify_sha256(data, expected);
    
    CHECK(result.ok);
}

TEST_CASE("verify_sha256 fails with mismatched digest") {
    std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
    std::string expected = "0000000000000000000000000000000000000000000000000000000000000000";
    
    auto result = verify_sha256(data, expected);
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("mismatch") != std::string::npos);
    CHECK(result.actual_digest == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
}

// ============================================================================
// install_nak Tests (unified install from various sources)
// ============================================================================

// Helper to create a valid NAK pack
std::vector<uint8_t> create_test_nak_pack(const std::string& nak_id, const std::string& version) {
    TempDir pack_dir;
    
    fs::create_directories(pack_dir.path() + "/META");
    fs::create_directories(pack_dir.path() + "/lib");
    
    std::ofstream(pack_dir.path() + "/META/nak.json") << 
        "{\n"
        "  \"$schema\": \"nah.nak.pack.v2\",\n"
        "  \"nak\": {\n"
        "    \"id\": \"" << nak_id << "\",\n"
        "    \"version\": \"" << version << "\"\n"
        "  },\n"
        "  \"paths\": {\n"
        "    \"resource_root\": \".\",\n"
        "    \"lib_dirs\": [\"lib\"]\n"
        "  },\n"
        "  \"execution\": {\n"
        "    \"cwd\": \"{NAH_APP_ROOT}\"\n"
        "  }\n"
        "}\n";
    
    std::ofstream(pack_dir.path() + "/lib/libtest.so") << "fake library";
    
    auto pack_result = pack_nak(pack_dir.path());
    if (!pack_result.ok) {
        return {};
    }
    return pack_result.archive_data;
}

TEST_CASE("install_nak from file: reference succeeds") {
    TempDir temp;
    
    // Create NAK pack file
    auto pack_data = create_test_nak_pack("com.test.install", "1.0.0");
    REQUIRE(!pack_data.empty());
    
    std::string pack_path = temp.path() + "/test.nak";
    std::ofstream pack_file(pack_path, std::ios::binary);
    pack_file.write(reinterpret_cast<const char*>(pack_data.data()), 
                    static_cast<std::streamsize>(pack_data.size()));
    pack_file.close();
    
    // Initialize NAH root
    std::string nah_root = temp.path() + "/nah";
    fs::create_directories(nah_root + "/registry/naks");
    fs::create_directories(nah_root + "/naks");
    
    // Install using file: URL
    NakInstallOptions opts;
    opts.nah_root = nah_root;
    opts.installed_by = "test-runner";
    
    auto result = install_nak("file:" + pack_path, opts);
    
    CHECK(result.ok);
    CHECK(result.nak_id == "com.test.install");
    CHECK(result.nak_version == "1.0.0");
    CHECK_FALSE(result.package_hash.empty());
    CHECK(result.package_hash.size() == 64);
    
    // Verify install location
    CHECK(fs::exists(result.install_root));
    CHECK(fs::exists(result.install_root + "/lib/libtest.so"));
    
    // Verify install record
    CHECK(fs::exists(result.record_path));
    
    // Verify provenance in record
    std::ifstream record_file(result.record_path);
    std::string record_content((std::istreambuf_iterator<char>(record_file)),
                                std::istreambuf_iterator<char>());
    
    CHECK(record_content.find("\"provenance\"") != std::string::npos);
    CHECK(record_content.find("\"installed_by\": \"test-runner\"") != std::string::npos);
}

TEST_CASE("install_nak from plain file path succeeds") {
    TempDir temp;
    
    auto pack_data = create_test_nak_pack("com.test.plainpath", "2.0.0");
    REQUIRE(!pack_data.empty());
    
    std::string pack_path = temp.path() + "/plain.nak";
    std::ofstream pack_file(pack_path, std::ios::binary);
    pack_file.write(reinterpret_cast<const char*>(pack_data.data()),
                    static_cast<std::streamsize>(pack_data.size()));
    pack_file.close();
    
    std::string nah_root = temp.path() + "/nah";
    fs::create_directories(nah_root + "/registry/naks");
    fs::create_directories(nah_root + "/naks");
    
    NakInstallOptions opts;
    opts.nah_root = nah_root;
    
    // Use plain path (not file: URL)
    auto result = install_nak(pack_path, opts);
    
    CHECK(result.ok);
    CHECK(result.nak_id == "com.test.plainpath");
    CHECK(result.nak_version == "2.0.0");
    CHECK(fs::exists(result.install_root + "/lib/libtest.so"));
}

TEST_CASE("install_nak fails on existing NAK without force") {
    TempDir temp;
    
    auto pack_data = create_test_nak_pack("com.test.existing", "1.0.0");
    REQUIRE(!pack_data.empty());
    
    std::string pack_path = temp.path() + "/existing.nak";
    std::ofstream pack_file(pack_path, std::ios::binary);
    pack_file.write(reinterpret_cast<const char*>(pack_data.data()),
                    static_cast<std::streamsize>(pack_data.size()));
    pack_file.close();
    
    std::string nah_root = temp.path() + "/nah";
    fs::create_directories(nah_root + "/registry/naks");
    fs::create_directories(nah_root + "/naks/com.test.existing/1.0.0");
    
    NakInstallOptions opts;
    opts.nah_root = nah_root;
    opts.force = false;
    
    auto result = install_nak("file:" + pack_path, opts);
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("already installed") != std::string::npos);
}

TEST_CASE("install_nak succeeds on existing NAK with force") {
    TempDir temp;
    
    auto pack_data = create_test_nak_pack("com.test.force", "1.0.0");
    REQUIRE(!pack_data.empty());
    
    std::string pack_path = temp.path() + "/force.nak";
    std::ofstream pack_file(pack_path, std::ios::binary);
    pack_file.write(reinterpret_cast<const char*>(pack_data.data()),
                    static_cast<std::streamsize>(pack_data.size()));
    pack_file.close();
    
    std::string nah_root = temp.path() + "/nah";
    fs::create_directories(nah_root + "/registry/naks");
    fs::create_directories(nah_root + "/naks/com.test.force/1.0.0");
    std::ofstream(nah_root + "/naks/com.test.force/1.0.0/old_file.txt") << "old";
    
    NakInstallOptions opts;
    opts.nah_root = nah_root;
    opts.force = true;
    
    auto result = install_nak("file:" + pack_path, opts);
    
    CHECK(result.ok);
    CHECK(fs::exists(result.install_root + "/lib/libtest.so"));
    CHECK_FALSE(fs::exists(result.install_root + "/old_file.txt"));
}

TEST_CASE("install_nak fails on invalid NAK pack") {
    TempDir temp;
    
    std::string pack_path = temp.path() + "/invalid.nak";
    std::ofstream pack_file(pack_path, std::ios::binary);
    pack_file << "this is not a valid NAK pack";
    pack_file.close();
    
    std::string nah_root = temp.path() + "/nah";
    fs::create_directories(nah_root);
    
    NakInstallOptions opts;
    opts.nah_root = nah_root;
    
    auto result = install_nak("file:" + pack_path, opts);
    
    CHECK_FALSE(result.ok);
}

TEST_CASE("install_nak fails on nonexistent file") {
    TempDir temp;
    
    NakInstallOptions opts;
    opts.nah_root = temp.path();
    
    auto result = install_nak("file:/nonexistent/path/to/pack.nak", opts);
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("open") != std::string::npos);
}

TEST_CASE("install_nak with expected_hash verifies integrity") {
    TempDir temp;
    
    auto pack_data = create_test_nak_pack("com.test.hash", "1.0.0");
    REQUIRE(!pack_data.empty());
    
    // Compute correct hash
    auto hash_result = compute_sha256(pack_data);
    REQUIRE(hash_result.ok);
    
    std::string pack_path = temp.path() + "/hash.nak";
    std::ofstream pack_file(pack_path, std::ios::binary);
    pack_file.write(reinterpret_cast<const char*>(pack_data.data()),
                    static_cast<std::streamsize>(pack_data.size()));
    pack_file.close();
    
    std::string nah_root = temp.path() + "/nah";
    fs::create_directories(nah_root + "/registry/naks");
    fs::create_directories(nah_root + "/naks");
    
    NakInstallOptions opts;
    opts.nah_root = nah_root;
    opts.expected_hash = hash_result.hex_digest;
    
    auto result = install_nak(pack_path, opts);
    
    CHECK(result.ok);
    CHECK(result.package_hash == hash_result.hex_digest);
}

TEST_CASE("install_nak with wrong expected_hash fails") {
    TempDir temp;
    
    auto pack_data = create_test_nak_pack("com.test.badhash", "1.0.0");
    REQUIRE(!pack_data.empty());
    
    std::string pack_path = temp.path() + "/badhash.nak";
    std::ofstream pack_file(pack_path, std::ios::binary);
    pack_file.write(reinterpret_cast<const char*>(pack_data.data()),
                    static_cast<std::streamsize>(pack_data.size()));
    pack_file.close();
    
    std::string nah_root = temp.path() + "/nah";
    fs::create_directories(nah_root);
    
    NakInstallOptions opts;
    opts.nah_root = nah_root;
    opts.expected_hash = "0000000000000000000000000000000000000000000000000000000000000000";
    
    auto result = install_nak(pack_path, opts);
    
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("mismatch") != std::string::npos);
}

// ============================================================================
// Provenance Recording Tests
// ============================================================================

TEST_CASE("install_nak records complete provenance") {
    TempDir temp;
    
    auto pack_data = create_test_nak_pack("com.test.provenance", "3.0.0");
    REQUIRE(!pack_data.empty());
    
    std::string pack_path = temp.path() + "/prov.nak";
    std::ofstream pack_file(pack_path, std::ios::binary);
    pack_file.write(reinterpret_cast<const char*>(pack_data.data()),
                    static_cast<std::streamsize>(pack_data.size()));
    pack_file.close();
    
    std::string nah_root = temp.path() + "/nah";
    fs::create_directories(nah_root + "/registry/naks");
    fs::create_directories(nah_root + "/naks");
    
    NakInstallOptions opts;
    opts.nah_root = nah_root;
    opts.installed_by = "ci-pipeline";
    opts.source = "file:" + pack_path;
    
    auto result = install_nak("file:" + pack_path, opts);
    
    REQUIRE(result.ok);
    
    // Verify provenance in install record file
    std::ifstream record_file(result.record_path);
    std::string content((std::istreambuf_iterator<char>(record_file)),
                         std::istreambuf_iterator<char>());
    
    auto record_result = parse_nak_install_record_full(content, result.record_path);
    CHECK(record_result.ok);
    
    // Check provenance section
    CHECK(content.find("\"provenance\"") != std::string::npos);
    CHECK(content.find("\"installed_by\": \"ci-pipeline\"") != std::string::npos);
    CHECK(content.find("\"package_hash\":") != std::string::npos);
    CHECK(content.find("\"installed_at\":") != std::string::npos);
}
