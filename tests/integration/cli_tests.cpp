/**
 * Integration tests for NAH CLI commands
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Portable environment variable helpers
namespace {

inline std::string safe_getenv(const char* name) {
#ifdef _WIN32
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, name) == 0 && buf != nullptr) {
        std::string result(buf);
        free(buf);
        return result;
    }
    return "";
#else
    const char* val = std::getenv(name);
    return val ? val : "";
#endif
}

inline void safe_setenv(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

inline void safe_unsetenv(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

inline std::string get_temp_dir() {
#ifdef _WIN32
    std::string tmp = safe_getenv("TEMP");
    if (tmp.empty()) tmp = safe_getenv("TMP");
    if (tmp.empty()) tmp = ".";
    return tmp;
#else
    return "/tmp";
#endif
}

// Counter for unique names within same second
static int g_temp_counter = 0;

inline std::string create_unique_temp_path(const std::string& prefix) {
    std::string temp_base = get_temp_dir();
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }
    g_temp_counter++;
    std::string path = temp_base + "/" + prefix + "_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(std::rand()) + "_" + std::to_string(g_temp_counter);
    return path;
}

// Get the path to the nah executable (platform-specific)
inline std::string get_nah_executable() {
#ifdef _WIN32
    return "nah.exe";
#else
    return "./nah";
#endif
}

} // anonymous namespace

// Helper function to execute command and capture output
struct CommandResult {
    int exit_code;
    std::string output;
    std::string error;
};

CommandResult execute_command(const std::string& command) {
    CommandResult result;

    // Create temp files for output
    std::string out_path = create_unique_temp_path("nah_test_out");
    std::string err_path = create_unique_temp_path("nah_test_err");

    // Execute command with output redirection
    std::string full_command = command + " >" + out_path + " 2>" + err_path;
    result.exit_code = std::system(full_command.c_str());

    // Read output files (scope ensures files are closed before delete)
    {
        std::ifstream out_file(out_path);
        std::stringstream out_buffer;
        out_buffer << out_file.rdbuf();
        result.output = out_buffer.str();
    }

    {
        std::ifstream err_file(err_path);
        std::stringstream err_buffer;
        err_buffer << err_file.rdbuf();
        result.error = err_buffer.str();
    }

    // Clean up temp files (ignore errors)
    std::error_code ec;
    std::filesystem::remove(out_path, ec);
    std::filesystem::remove(err_path, ec);

    return result;
}

// Helper to create a test NAH environment
class TestNahEnvironment {
public:
    TestNahEnvironment() {
        // Create temp directory
        root = create_unique_temp_path("nah_cli_test");
        std::filesystem::create_directories(root);

        // Set NAH_ROOT for the duration of the tests
        original_nah_root = safe_getenv("NAH_ROOT");
        safe_setenv("NAH_ROOT", root.c_str());

        // Create NAH directory structure
        std::filesystem::create_directories(root + "/apps");
        std::filesystem::create_directories(root + "/naks");
        std::filesystem::create_directories(root + "/host");
        std::filesystem::create_directories(root + "/registry/apps");
        std::filesystem::create_directories(root + "/registry/naks");
        std::filesystem::create_directories(root + "/staging");
    }

    ~TestNahEnvironment() {
        // Restore original NAH_ROOT
        if (!original_nah_root.empty()) {
            safe_setenv("NAH_ROOT", original_nah_root.c_str());
        } else {
            safe_unsetenv("NAH_ROOT");
        }

        // Clean up temp directory
        if (!root.empty()) {
            std::filesystem::remove_all(root);
        }
    }

    void createTestApp(const std::string& id, const std::string& version) {
        std::string app_dir = root + "/apps/" + id + "-" + version;
        std::filesystem::create_directories(app_dir);
        std::filesystem::create_directories(app_dir + "/bin");

        // Create executable
        std::string exec_path = app_dir + "/bin/app";
        std::ofstream exec_file(exec_path);
        exec_file << "#!/bin/sh\necho 'Hello from " << id << " v" << version << "'\n";
        exec_file.close();
        std::filesystem::permissions(exec_path,
            std::filesystem::perms::owner_exec |
            std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write);

        // Create manifest
        std::ofstream manifest(app_dir + "/nah.json");
        manifest << "{\n";
        manifest << "  \"id\": \"" << id << "\",\n";
        manifest << "  \"version\": \"" << version << "\",\n";
        manifest << "  \"entrypoint\": \"bin/app\"\n";
        manifest << "}\n";
        manifest.close();

        // Create install record
        std::string record_path = root + "/registry/apps/" + id + "@" + version + ".json";
        std::ofstream record(record_path);
        record << "{\n";
        record << "  \"install\": { \"instance_id\": \"test-" << id << "\" },\n";
        record << "  \"app\": { \"id\": \"" << id << "\", \"version\": \"" << version << "\" },\n";
        record << "  \"paths\": { \"install_root\": \"" << app_dir << "\" },\n";
        record << "  \"trust\": { \"state\": \"unknown\" }\n";
        record << "}\n";
        record.close();
    }

    void createHostJson(const std::string& content) {
        std::ofstream host_file(root + "/host/host.json");
        host_file << content;
        host_file.close();
    }

    std::string root;
    std::string original_nah_root;
};

TEST_CASE("nah --version") {
    auto result = execute_command(get_nah_executable() + " --version");
    CHECK(result.exit_code == 0);
    // Version might be in stdout or stderr, check both
    std::string combined = result.output + result.error;
    CHECK(combined.find(".") != std::string::npos);  // Should contain version number with dots
}

TEST_CASE("nah --help") {
    auto result = execute_command(get_nah_executable() + " --help");
    CHECK(result.exit_code == 0);
    // Help text might be in stdout or stderr
    std::string combined = result.output + result.error;
    CHECK((combined.find("Native Application Host") != std::string::npos ||
           combined.find("nah") != std::string::npos));
    CHECK((combined.find("Usage:") != std::string::npos ||
           combined.find("OPTIONS") != std::string::npos));
}

// Test the init command for project scaffolding
TEST_CASE("nah init") {
    // Create temp directory for test project
    std::string test_dir = create_unique_temp_path("nah_init_test");
    std::filesystem::create_directory(test_dir);

    SUBCASE("init app project") {
        auto result = execute_command(get_nah_executable() + " init --app --id com.test.myapp " + test_dir + "/myapp");
        CHECK(result.exit_code == 0);
        CHECK(std::filesystem::exists(test_dir + "/myapp/nah.json"));

        // Read and verify the generated manifest
        std::ifstream manifest_file(test_dir + "/myapp/nah.json");
        if (manifest_file) {
            std::string content((std::istreambuf_iterator<char>(manifest_file)),
                              std::istreambuf_iterator<char>());
            CHECK(content.find("com.test.myapp") != std::string::npos);
        }
    }

    // Clean up
    std::filesystem::remove_all(test_dir);
}

// Test the list command
TEST_CASE("nah list") {
    TestNahEnvironment env;
    REQUIRE(!env.root.empty());

    SUBCASE("list with no apps") {
        auto result = execute_command(get_nah_executable() + " list");
        CHECK(result.exit_code == 0);
        // List output should indicate no apps installed
        std::string combined = result.output + result.error;
        bool empty_or_no_packages = combined.empty() ||
                                   combined == "\n" ||
                                   combined.find("No apps") != std::string::npos ||
                                   combined.find("No packages") != std::string::npos;
        CHECK(empty_or_no_packages);
    }

    SUBCASE("list installed apps") {
        env.createTestApp("com.test.app1", "1.0.0");
        env.createTestApp("com.test.app2", "2.0.0");

        auto result = execute_command(get_nah_executable() + " list");
        CHECK(result.exit_code == 0);
        std::string combined = result.output + result.error;
        CHECK(combined.find("com.test.app1") != std::string::npos);
        CHECK(combined.find("1.0.0") != std::string::npos);
        CHECK(combined.find("com.test.app2") != std::string::npos);
        CHECK(combined.find("2.0.0") != std::string::npos);
    }

    // TODO: Add --json flag support to list command
    // SUBCASE("list with --json flag") {
    //     env.createTestApp("com.test.app", "1.0.0");
    //
    //     auto result = execute_command("./nah list --json");
    //     CHECK(result.exit_code == 0);
    //     std::string combined = result.output + result.error;
    //     CHECK(combined.find("[") != std::string::npos); // JSON array
    //     CHECK(combined.find("com.test.app") != std::string::npos);
    //     CHECK(combined.find("1.0.0") != std::string::npos);
    // }
}