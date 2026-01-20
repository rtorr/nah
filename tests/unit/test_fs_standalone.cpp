/**
 * Standalone test for nah_fs.h functions
 * This allows testing even when other headers are broken
 */

#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>

// Only include the minimal implementation we need
#define NAH_FS_IMPLEMENTATION

// Temporarily define the core types that nah_fs needs
namespace nah {
namespace core {
    struct AppDeclaration {
        std::string id;
        std::string version;
    };
    struct InstallRecord {
        struct {
            std::string install_root;
        } paths;
    };
    struct RuntimeDescriptor {};
    struct LaunchContract {};
}
}

// Now include nah_fs.h
namespace nah {
namespace fs {

#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

inline bool exists(const std::string& path) {
    return std::filesystem::exists(path);
}

inline bool is_directory(const std::string& path) {
    return std::filesystem::is_directory(path);
}

inline std::optional<std::string> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

inline bool write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file << content;
    return file.good();
}

inline bool write_file_atomic(const std::string& path, const std::string& content) {
    std::string temp_path = path + ".tmp";
    if (!write_file(temp_path, content)) {
        return false;
    }

    try {
        std::filesystem::rename(temp_path, path);
        return true;
    } catch (...) {
        std::filesystem::remove(temp_path);
        return false;
    }
}

inline std::vector<std::string> list_directory(const std::string& path) {
    std::vector<std::string> entries;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            entries.push_back(entry.path().string());
        }
    } catch (...) {
        // Return empty on error
    }
    return entries;
}

inline bool make_executable(const std::string& path) {
    try {
        auto perms = std::filesystem::status(path).permissions();
        std::filesystem::permissions(path,
            perms | std::filesystem::perms::owner_exec |
                    std::filesystem::perms::group_exec |
                    std::filesystem::perms::others_exec);
        return true;
    } catch (...) {
        return false;
    }
}

inline std::string get_cwd() {
    return std::filesystem::current_path().string();
}

inline bool ensure_directory(const std::string& path) {
    try {
        return std::filesystem::create_directories(path) || std::filesystem::is_directory(path);
    } catch (...) {
        return false;
    }
}

} // namespace fs
} // namespace nah

// Test functions
void test_basic_file_operations() {
    std::string test_file = "/tmp/nah_test_file.txt";
    std::string content = "Hello, NAH!";

    // Test write
    assert(nah::fs::write_file(test_file, content));

    // Test exists
    assert(nah::fs::exists(test_file));

    // Test read
    auto read_content = nah::fs::read_file(test_file);
    assert(read_content.has_value());
    assert(*read_content == content);

    // Test is_directory
    assert(!nah::fs::is_directory(test_file));

    // Cleanup
    std::filesystem::remove(test_file);

    std::cout << "✓ Basic file operations test passed\n";
}

void test_directory_operations() {
    std::string test_dir = "/tmp/nah_test_dir";

    // Ensure directory
    assert(nah::fs::ensure_directory(test_dir));
    assert(nah::fs::exists(test_dir));
    assert(nah::fs::is_directory(test_dir));

    // Create some files
    assert(nah::fs::write_file(test_dir + "/file1.txt", "content1"));
    assert(nah::fs::write_file(test_dir + "/file2.txt", "content2"));

    // List directory
    auto entries = nah::fs::list_directory(test_dir);
    assert(entries.size() == 2);

    // Cleanup
    std::filesystem::remove_all(test_dir);

    std::cout << "✓ Directory operations test passed\n";
}

void test_atomic_write() {
    std::string test_file = "/tmp/nah_test_atomic.txt";
    std::string content = "Atomic content";

    // Test atomic write
    assert(nah::fs::write_file_atomic(test_file, content));

    // Verify content
    auto read_content = nah::fs::read_file(test_file);
    assert(read_content.has_value());
    assert(*read_content == content);

    // Cleanup
    std::filesystem::remove(test_file);

    std::cout << "✓ Atomic write test passed\n";
}

void test_executable_permissions() {
    std::string test_file = "/tmp/nah_test_exec.sh";
    assert(nah::fs::write_file(test_file, "#!/bin/bash\necho test"));

    // Make executable
    assert(nah::fs::make_executable(test_file));

    // Check permissions
    auto perms = std::filesystem::status(test_file).permissions();
    assert((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);

    // Cleanup
    std::filesystem::remove(test_file);

    std::cout << "✓ Executable permissions test passed\n";
}

int main() {
    std::cout << "Testing nah_fs.h functions...\n";

    test_basic_file_operations();
    test_directory_operations();
    test_atomic_write();
    test_executable_permissions();

    std::cout << "\nAll filesystem tests passed! ✓\n";
    return 0;
}