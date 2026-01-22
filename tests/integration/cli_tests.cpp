/**
 * Integration tests for NAH CLI commands
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <nah/nah_fs.h>
#include <nah/nah_core.h>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Portable environment variable helpers
namespace
{
    inline std::string safe_getenv(const char *name)
    {
#ifdef _WIN32
        char *buf = nullptr;
        size_t sz = 0;
        if (_dupenv_s(&buf, &sz, name) == 0 && buf != nullptr)
        {
            std::string result(buf);
            free(buf);
            return result;
        }
        return "";
#else
        const char *val = std::getenv(name);
        return val ? val : "";
#endif
    }

    inline void safe_setenv(const char *name, const char *value)
    {
#ifdef _WIN32
        _putenv_s(name, value);
#else
        setenv(name, value, 1);
#endif
    }

    inline void safe_unsetenv(const char *name)
    {
#ifdef _WIN32
        _putenv_s(name, "");
#else
        unsetenv(name);
#endif
    }

    inline std::string get_temp_dir()
    {
#ifdef _WIN32
        std::string tmp = safe_getenv("TEMP");
        if (tmp.empty())
            tmp = safe_getenv("TMP");
        if (tmp.empty())
            tmp = ".";
        return tmp;
#else
        return "/tmp";
#endif
    }

    // Counter for unique names within same second
    static int g_temp_counter = 0;

    inline std::string create_unique_temp_path(const std::string &prefix)
    {
        std::string temp_base = get_temp_dir();
        static bool seeded = false;
        if (!seeded)
        {
            std::srand(static_cast<unsigned>(std::time(nullptr)));
            seeded = true;
        }
        g_temp_counter++;
        std::string filename = prefix + "_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(std::rand()) + "_" + std::to_string(g_temp_counter);
        return nah::fs::join_paths(temp_base, filename);
    }

    // Get the path to the nah executable (platform-specific)
    inline std::string get_nah_executable()
    {
#ifdef _WIN32
        return "nah.exe";
#else
        return "./nah";
#endif
    }

} // anonymous namespace

// Helper function to execute command and capture output
struct CommandResult
{
    int exit_code;
    std::string output;
    std::string error;
};

CommandResult execute_command(const std::string &command)
{
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
class TestNahEnvironment
{
public:
    TestNahEnvironment()
    {
        // Create temp directory
        root = create_unique_temp_path("nah_cli_test");
        std::filesystem::create_directories(root);

        // Set NAH_ROOT for the duration of the tests
        original_nah_root = safe_getenv("NAH_ROOT");
        safe_setenv("NAH_ROOT", root.c_str());

        // Create NAH directory structure using nah::fs::join_paths
        std::filesystem::create_directories(nah::fs::join_paths(root, "apps"));
        std::filesystem::create_directories(nah::fs::join_paths(root, "naks"));
        std::filesystem::create_directories(nah::fs::join_paths(root, "host"));
        std::filesystem::create_directories(nah::fs::join_paths(root, "registry", "apps"));
        std::filesystem::create_directories(nah::fs::join_paths(root, "registry", "naks"));
        std::filesystem::create_directories(nah::fs::join_paths(root, "staging"));
    }

    ~TestNahEnvironment()
    {
        // Restore original NAH_ROOT
        if (!original_nah_root.empty())
        {
            safe_setenv("NAH_ROOT", original_nah_root.c_str());
        }
        else
        {
            safe_unsetenv("NAH_ROOT");
        }

        // Clean up temp directory
        if (!root.empty())
        {
            std::filesystem::remove_all(root);
        }
    }

    // Helper to normalize paths for JSON (convert backslashes to forward slashes)
    std::string normalizePathForJson(const std::string &path)
    {
        return nah::core::normalize_separators(path);
    }

    void createTestApp(const std::string &id, const std::string &version)
    {
        // Use nah::fs::join_paths for cross-platform compatibility
        std::string app_dir = nah::fs::join_paths(root, "apps", id + "-" + version);
        std::filesystem::create_directories(app_dir);

        std::string bin_dir = nah::fs::join_paths(app_dir, "bin");
        std::filesystem::create_directories(bin_dir);

        // Create executable
        std::string exec_path = nah::fs::join_paths(bin_dir, "app");
        std::ofstream exec_file(exec_path);
        exec_file << "#!/bin/sh\necho 'Hello from " << id << " v" << version << "'\n";
        exec_file.close();
        std::filesystem::permissions(exec_path,
                                     std::filesystem::perms::owner_exec |
                                         std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write);

        // Create manifest
        std::string manifest_path = nah::fs::join_paths(app_dir, "nap.json");
        std::ofstream manifest(manifest_path);
        manifest << "{\n";
        manifest << "  \"id\": \"" << id << "\",\n";
        manifest << "  \"version\": \"" << version << "\",\n";
        manifest << "  \"entrypoint\": \"bin/app\"\n";
        manifest << "}\n";
        manifest.close();

        // Create install record (normalize paths for JSON compatibility)
        std::string record_path = nah::fs::join_paths(root, "registry", "apps", id + "@" + version + ".json");
        std::ofstream record(record_path);
        record << "{\n";
        record << "  \"install\": { \"instance_id\": \"test-" << id << "\" },\n";
        record << "  \"app\": { \"id\": \"" << id << "\", \"version\": \"" << version << "\" },\n";
        record << "  \"paths\": { \"install_root\": \"" << nah::core::normalize_separators(app_dir) << "\" },\n";
        record << "  \"trust\": { \"state\": \"unknown\" }\n";
        record << "}\n";
        record.close();
    }

    void createHostJson(const std::string &content)
    {
        std::string host_file_path = nah::fs::join_paths(root, "host", "host.json");
        std::ofstream host_file(host_file_path);
        host_file << content;
        host_file.close();
    }

    std::string root;
    std::string original_nah_root;
};

TEST_CASE("nah --version")
{
    auto result = execute_command(get_nah_executable() + " --version");
    CHECK(result.exit_code == 0);
    // Version might be in stdout or stderr, check both
    std::string combined = result.output + result.error;
    CHECK(combined.find(".") != std::string::npos); // Should contain version number with dots
}

TEST_CASE("nah --help")
{
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
TEST_CASE("nah init")
{
    // Create temp directory for test project
    std::string test_dir = create_unique_temp_path("nah_init_test");
    std::filesystem::create_directory(test_dir);

    SUBCASE("init app project")
    {
        std::string myapp_path = nah::fs::join_paths(test_dir, "myapp");
        auto result = execute_command(get_nah_executable() + " init --app --id com.test.myapp " + myapp_path);
        CHECK(result.exit_code == 0);

        std::string manifest_path = nah::fs::join_paths(myapp_path, "nap.json");
        CHECK(std::filesystem::exists(manifest_path));

        // Read and verify the generated manifest
        std::ifstream manifest_file(manifest_path);
        if (manifest_file)
        {
            std::string content((std::istreambuf_iterator<char>(manifest_file)),
                                std::istreambuf_iterator<char>());
            CHECK(content.find("com.test.myapp") != std::string::npos);
        }
    }

    SUBCASE("init nak project")
    {
        std::string mynak_path = nah::fs::join_paths(test_dir, "mynak");
        auto result = execute_command(get_nah_executable() + " init --nak --id com.test.mysdk " + mynak_path);
        CHECK(result.exit_code == 0);

        std::string manifest_path = nah::fs::join_paths(mynak_path, "nak.json");
        CHECK(std::filesystem::exists(manifest_path));

        std::ifstream manifest_file(manifest_path);
        if (manifest_file)
        {
            std::string content((std::istreambuf_iterator<char>(manifest_file)),
                                std::istreambuf_iterator<char>());
            CHECK(content.find("com.test.mysdk") != std::string::npos);
        }
    }

    SUBCASE("init host project")
    {
        std::string host_path = nah::fs::join_paths(test_dir, "myhost");
        auto result = execute_command(get_nah_executable() + " init --host " + host_path);
        CHECK(result.exit_code == 0);

        std::string manifest_path = nah::fs::join_paths(host_path, "nah.json");
        CHECK(std::filesystem::exists(manifest_path));
    }

    // Clean up
    std::filesystem::remove_all(test_dir);
}

// Test the list command
TEST_CASE("nah list")
{
    TestNahEnvironment env;
    REQUIRE(!env.root.empty());

    SUBCASE("list with no apps")
    {
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

    SUBCASE("list installed apps")
    {
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

// Test the show command
TEST_CASE("nah show")
{
    TestNahEnvironment env;
    REQUIRE(!env.root.empty());

    SUBCASE("show with no target - overview mode")
    {
        env.createTestApp("com.test.app1", "1.0.0");
        env.createTestApp("com.test.app2", "2.0.0");

        auto result = execute_command(get_nah_executable() + " show");
        CHECK(result.exit_code == 0);
        std::string combined = result.output + result.error;
        // Should show overview with app count
        CHECK((combined.find("Apps") != std::string::npos ||
               combined.find("apps") != std::string::npos));
    }

    SUBCASE("show non-existent app")
    {
        auto result = execute_command(get_nah_executable() + " show com.test.nonexistent");
        CHECK(result.exit_code != 0);
        std::string combined = result.output + result.error;
        CHECK((combined.find("not installed") != std::string::npos ||
               combined.find("not found") != std::string::npos));
    }

    SUBCASE("show installed app by id")
    {
        env.createTestApp("com.test.showapp", "1.5.0");

        auto result = execute_command(get_nah_executable() + " show com.test.showapp");
        CHECK(result.exit_code == 0);
        std::string combined = result.output + result.error;
        CHECK(combined.find("com.test.showapp") != std::string::npos);
        CHECK(combined.find("1.5.0") != std::string::npos);
    }

    SUBCASE("show installed app by id@version")
    {
        env.createTestApp("com.test.versioned", "2.3.4");
        env.createTestApp("com.test.versioned", "2.3.5");

        auto result = execute_command(get_nah_executable() + " show com.test.versioned@2.3.4");
        CHECK(result.exit_code == 0);
        std::string combined = result.output + result.error;
        CHECK(combined.find("com.test.versioned") != std::string::npos);
        CHECK(combined.find("2.3.4") != std::string::npos);
    }

    SUBCASE("show reads nap.json")
    {
        env.createTestApp("com.test.naptest", "1.0.0");

        std::string app_dir = nah::fs::join_paths(env.root, "apps", "com.test.naptest-1.0.0");
        std::string nap_path = nah::fs::join_paths(app_dir, "nap.json");

        CHECK(std::filesystem::exists(nap_path));

        auto result = execute_command(get_nah_executable() + " show com.test.naptest");
        CHECK(result.exit_code == 0);
        std::string combined = result.output + result.error;
        CHECK(combined.find("com.test.naptest") != std::string::npos);
    }

    SUBCASE("show with --trace flag")
    {
        env.createTestApp("com.test.traceapp", "1.0.0");

        auto result = execute_command(get_nah_executable() + " show com.test.traceapp --trace");
        CHECK(result.exit_code == 0);
        std::string combined = result.output + result.error;
        CHECK(combined.find("com.test.traceapp") != std::string::npos);
        // Trace output might contain "Trace:" or decision information
    }

    SUBCASE("show with global --json flag")
    {
        env.createTestApp("com.test.jsonapp", "3.2.1");

        // Use global --json flag before the show subcommand
        auto result = execute_command(get_nah_executable() + " --json show com.test.jsonapp");
        CHECK(result.exit_code == 0);
        std::string combined = result.output + result.error;
        // JSON output should contain curly braces
        CHECK((combined.find("{") != std::string::npos &&
               combined.find("}") != std::string::npos));
    }
}

// End-to-end lifecycle tests
TEST_CASE("nah end-to-end workflow")
{
    TestNahEnvironment env;
    REQUIRE(!env.root.empty());

    SUBCASE("install -> show -> list lifecycle")
    {
        // Create a test app
        env.createTestApp("com.test.lifecycle", "1.0.0");

        // Test show command works after "install"
        auto show_result = execute_command(get_nah_executable() + " show com.test.lifecycle");
        CHECK(show_result.exit_code == 0);
        std::string show_output = show_result.output + show_result.error;
        CHECK(show_output.find("com.test.lifecycle") != std::string::npos);
        CHECK(show_output.find("1.0.0") != std::string::npos);

        // Verify the app appears in list
        auto list_result = execute_command(get_nah_executable() + " list");
        CHECK(list_result.exit_code == 0);
        std::string list_output = list_result.output + list_result.error;
        CHECK(list_output.find("com.test.lifecycle") != std::string::npos);

        // Note: Full uninstall testing requires actual package installation
        // which is beyond the scope of these basic integration tests
    }

    SUBCASE("multiple versions coexist")
    {
        env.createTestApp("com.test.multiversion", "1.0.0");
        env.createTestApp("com.test.multiversion", "1.1.0");
        env.createTestApp("com.test.multiversion", "2.0.0");

        // List should show all versions
        auto list_result = execute_command(get_nah_executable() + " list");
        CHECK(list_result.exit_code == 0);
        std::string list_output = list_result.output + list_result.error;
        CHECK(list_output.find("com.test.multiversion") != std::string::npos);

        // Show specific version
        auto show_v1 = execute_command(get_nah_executable() + " show com.test.multiversion@1.0.0");
        CHECK(show_v1.exit_code == 0);
        CHECK((show_v1.output + show_v1.error).find("1.0.0") != std::string::npos);

        auto show_v2 = execute_command(get_nah_executable() + " show com.test.multiversion@2.0.0");
        CHECK(show_v2.exit_code == 0);
        CHECK((show_v2.output + show_v2.error).find("2.0.0") != std::string::npos);
    }

    SUBCASE("show requires nap.json")
    {
        std::string app_dir = nah::fs::join_paths(env.root, "apps", "com.test.badmanifest-1.0.0");
        std::filesystem::create_directories(app_dir);

        std::string old_manifest = nah::fs::join_paths(app_dir, "nah.json");
        std::ofstream old_file(old_manifest);
        old_file << R"({"id": "com.test.badmanifest", "version": "1.0.0"})";
        old_file.close();

        std::string record_path = nah::fs::join_paths(env.root, "registry", "apps",
                                                      "com.test.badmanifest@1.0.0.json");
        std::ofstream record(record_path);
        record << "{\n";
        record << "  \"install\": { \"instance_id\": \"test-badmanifest\" },\n";
        record << "  \"app\": { \"id\": \"com.test.badmanifest\", \"version\": \"1.0.0\" },\n";
        record << "  \"paths\": { \"install_root\": \"" << nah::core::normalize_separators(app_dir) << "\" },\n";
        record << "  \"trust\": { \"state\": \"unknown\" }\n";
        record << "}\n";
        record.close();

        auto result = execute_command(get_nah_executable() + " show com.test.badmanifest");
        CHECK(result.exit_code != 0);
        std::string combined = result.output + result.error;
        CHECK((combined.find("nap.json") != std::string::npos ||
               combined.find("manifest") != std::string::npos));
    }
}

// Test error handling and edge cases
TEST_CASE("nah error handling")
{
    TestNahEnvironment env;
    REQUIRE(!env.root.empty());

    SUBCASE("show handles missing install record")
    {
        auto result = execute_command(get_nah_executable() + " show com.test.ghost@9.9.9");
        CHECK(result.exit_code != 0);
        std::string combined = result.output + result.error;
        CHECK((combined.find("not installed") != std::string::npos ||
               combined.find("not found") != std::string::npos));
    }

    SUBCASE("show handles corrupted manifest")
    {
        // Create app with corrupted nap.json
        std::string app_dir = nah::fs::join_paths(env.root, "apps", "com.test.corrupt-1.0.0");
        std::filesystem::create_directories(app_dir);

        // Write invalid JSON
        std::string manifest_path = nah::fs::join_paths(app_dir, "nap.json");
        std::ofstream manifest(manifest_path);
        manifest << "{ this is not valid JSON !!!";
        manifest.close();

        // Create install record
        std::string record_path = nah::fs::join_paths(env.root, "registry", "apps",
                                                      "com.test.corrupt@1.0.0.json");
        std::ofstream record(record_path);
        record << "{\n";
        record << "  \"install\": { \"instance_id\": \"test-corrupt\" },\n";
        record << "  \"app\": { \"id\": \"com.test.corrupt\", \"version\": \"1.0.0\" },\n";
        record << "  \"paths\": { \"install_root\": \"" << nah::core::normalize_separators(app_dir) << "\" },\n";
        record << "  \"trust\": { \"state\": \"unknown\" }\n";
        record << "}\n";
        record.close();

        auto result = execute_command(get_nah_executable() + " show com.test.corrupt");
        CHECK(result.exit_code != 0);
        std::string combined = result.output + result.error;
        CHECK((combined.find("Invalid") != std::string::npos ||
               combined.find("parse") != std::string::npos ||
               combined.find("error") != std::string::npos));
    }
}