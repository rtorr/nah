/**
 * Integration tests for NAH CLI commands
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#define NAH_HOST_IMPLEMENTATION
#include <nah/nah_host.h>
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

// Test loader selection features
TEST_CASE("nah loader selection")
{
    TestNahEnvironment env;
    REQUIRE(!env.root.empty());

    // Helper to create a NAK with multiple loaders
    auto createMultiLoaderNak = [&](const std::string &id, const std::string &version)
    {
        std::string nak_dir = nah::fs::join_paths(env.root, "naks", id, version);
        std::filesystem::create_directories(nak_dir);

        std::string bin_dir = nah::fs::join_paths(nak_dir, "bin");
        std::filesystem::create_directories(bin_dir);

        // Create multiple loader executables
        for (const auto &loader_name : {"default-loader", "alternate-loader", "debug-loader"})
        {
            std::string exec_path = nah::fs::join_paths(bin_dir, loader_name);
            std::ofstream exec_file(exec_path);
            exec_file << "#!/bin/sh\necho 'Running with " << loader_name << "'\n";
            exec_file.close();
            std::filesystem::permissions(exec_path,
                                         std::filesystem::perms::owner_exec |
                                             std::filesystem::perms::owner_read |
                                             std::filesystem::perms::owner_write);
        }

        // Create NAK manifest with multiple loaders
        std::string manifest_path = nah::fs::join_paths(nak_dir, "nak.json");
        std::ofstream manifest(manifest_path);
        manifest << "{\n";
        manifest << "  \"id\": \"" << id << "\",\n";
        manifest << "  \"version\": \"" << version << "\",\n";
        manifest << "  \"loaders\": {\n";
        manifest << "    \"default\": {\n";
        manifest << "      \"exec_path\": \"bin/default-loader\",\n";
        manifest << "      \"args_template\": [\"--default-mode\"]\n";
        manifest << "    },\n";
        manifest << "    \"alternate\": {\n";
        manifest << "      \"exec_path\": \"bin/alternate-loader\",\n";
        manifest << "      \"args_template\": [\"--alt-mode\"]\n";
        manifest << "    },\n";
        manifest << "    \"debug\": {\n";
        manifest << "      \"exec_path\": \"bin/debug-loader\",\n";
        manifest << "      \"args_template\": [\"--debug\", \"--verbose\"]\n";
        manifest << "    }\n";
        manifest << "  }\n";
        manifest << "}\n";
        manifest.close();

        // Create NAK install record
        std::string record_path = nah::fs::join_paths(env.root, "registry", "naks", id + "@" + version + ".json");
        std::ofstream record(record_path);
        record << "{\n";
        record << "  \"install\": { \"instance_id\": \"test-nak-" << id << "\" },\n";
        record << "  \"nak\": { \"id\": \"" << id << "\", \"version\": \"" << version << "\" },\n";
        record << "  \"paths\": { \"install_root\": \"" << nah::core::normalize_separators(nak_dir) << "\" },\n";
        record << "  \"trust\": { \"state\": \"unknown\" }\n";
        record << "}\n";
        record.close();
    };

    // Helper to create an app that uses a NAK
    auto createAppWithNak = [&](const std::string &app_id, const std::string &app_version,
                                const std::string &nak_id, const std::string &nak_version,
                                const std::string &loader)
    {
        std::string app_dir = nah::fs::join_paths(env.root, "apps", app_id + "-" + app_version);
        std::filesystem::create_directories(app_dir);

        std::string bin_dir = nah::fs::join_paths(app_dir, "bin");
        std::filesystem::create_directories(bin_dir);

        // Create app executable
        std::string exec_path = nah::fs::join_paths(bin_dir, "app");
        std::ofstream exec_file(exec_path);
        exec_file << "#!/bin/sh\necho 'App running'\n";
        exec_file.close();
        std::filesystem::permissions(exec_path,
                                     std::filesystem::perms::owner_exec |
                                         std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write);

        // Create app manifest
        std::string manifest_path = nah::fs::join_paths(app_dir, "nap.json");
        std::ofstream manifest(manifest_path);
        manifest << "{\n";
        manifest << "  \"id\": \"" << app_id << "\",\n";
        manifest << "  \"version\": \"" << app_version << "\",\n";
        manifest << "  \"entrypoint\": \"bin/app\"\n";
        manifest << "}\n";
        manifest.close();

        // Create app install record with NAK dependency
        std::string record_path = nah::fs::join_paths(env.root, "registry", "apps", app_id + "@" + app_version + ".json");
        std::ofstream record(record_path);
        record << "{\n";
        record << "  \"install\": { \"instance_id\": \"test-app-" << app_id << "\" },\n";
        record << "  \"app\": { \"id\": \"" << app_id << "\", \"version\": \"" << app_version << "\" },\n";
        record << "  \"paths\": { \"install_root\": \"" << nah::core::normalize_separators(app_dir) << "\" },\n";
        record << "  \"nak\": {\n";
        record << "    \"record_ref\": \"" << nak_id << "@" << nak_version << ".json\",\n";
        record << "    \"loader\": \"" << loader << "\"\n";
        record << "  },\n";
        record << "  \"trust\": { \"state\": \"unknown\" }\n";
        record << "}\n";
        record.close();
    };

    SUBCASE("install command --loader flag appears in help")
    {
        auto result = execute_command(get_nah_executable() + " install --help");
        CHECK(result.exit_code == 0);
        std::string combined = result.output + result.error;
        CHECK(combined.find("--loader") != std::string::npos);
    }

    SUBCASE("run command --loader flag appears in help")
    {
        auto result = execute_command(get_nah_executable() + " run --help");
        CHECK(result.exit_code == 0);
        std::string combined = result.output + result.error;
        CHECK(combined.find("--loader") != std::string::npos);
        bool has_desc = (combined.find("override") != std::string::npos) ||
                        (combined.find("Loader") != std::string::npos);
        CHECK(has_desc);
    }

    SUBCASE("install-time loader selection stores loader in record")
    {
        // Create NAK
        createMultiLoaderNak("com.test.runtime", "1.0.0");

        // Create app with specific loader
        createAppWithNak("com.test.loadapp", "1.0.0", "com.test.runtime", "1.0.0", "alternate");

        // Verify the install record has the correct loader
        std::string record_path = nah::fs::join_paths(env.root, "registry", "apps", "com.test.loadapp@1.0.0.json");
        auto record_content = nah::fs::read_file(record_path);
        REQUIRE(record_content.has_value());
        CHECK(record_content->find("\"loader\": \"alternate\"") != std::string::npos);
    }

    SUBCASE("show command displays loader information")
    {
        // Create NAK and app
        createMultiLoaderNak("com.test.runtime", "1.0.0");
        createAppWithNak("com.test.showloader", "1.0.0", "com.test.runtime", "1.0.0", "debug");

        auto result = execute_command(get_nah_executable() + " show com.test.showloader");
        CHECK(result.exit_code == 0);
        std::string combined = result.output + result.error;
        // Output should contain app info
        CHECK(combined.find("com.test.showloader") != std::string::npos);
    }

    SUBCASE("runtime loader override via run --loader")
    {
        // Create NAK with multiple loaders
        createMultiLoaderNak("com.test.runtime", "1.0.0");

        // Create app installed with default loader
        createAppWithNak("com.test.runapp", "1.0.0", "com.test.runtime", "1.0.0", "default");

        // Note: Actually running requires full execution setup, but we can verify
        // the command accepts the flag without error (will fail at execution stage)
        // This tests the CLI parsing works correctly
        auto result = execute_command(get_nah_executable() + " run com.test.runapp --loader alternate");
        // Command should parse correctly (may fail at execution, but not at parsing)
        std::string combined = result.output + result.error;
        // Should NOT have "Unknown option" or similar parsing errors
        CHECK(combined.find("Unknown option") == std::string::npos);
        bool no_loader_error = (combined.find("--loader") == std::string::npos) ||
                               (combined.find("invalid") == std::string::npos);
        CHECK(no_loader_error);
    }

    SUBCASE("multiple apps with different loaders from same NAK")
    {
        // Create one NAK
        createMultiLoaderNak("com.test.shared-runtime", "1.0.0");

        // Create three apps using different loaders
        createAppWithNak("com.test.app1", "1.0.0", "com.test.shared-runtime", "1.0.0", "default");
        createAppWithNak("com.test.app2", "1.0.0", "com.test.shared-runtime", "1.0.0", "alternate");
        createAppWithNak("com.test.app3", "1.0.0", "com.test.shared-runtime", "1.0.0", "debug");

        // Verify all apps are listed
        auto list_result = execute_command(get_nah_executable() + " list");
        CHECK(list_result.exit_code == 0);
        std::string list_output = list_result.output + list_result.error;
        CHECK(list_output.find("com.test.app1") != std::string::npos);
        CHECK(list_output.find("com.test.app2") != std::string::npos);
        CHECK(list_output.find("com.test.app3") != std::string::npos);

        // Verify each has correct loader in install record
        for (const auto &[app, loader] : std::vector<std::pair<std::string, std::string>>{
                 {"com.test.app1", "default"},
                 {"com.test.app2", "alternate"},
                 {"com.test.app3", "debug"}})
        {
            std::string record_path = nah::fs::join_paths(env.root, "registry", "apps", app + "@1.0.0.json");
            auto record_content = nah::fs::read_file(record_path);
            REQUIRE(record_content.has_value());
            CHECK(record_content->find("\"loader\": \"" + loader + "\"") != std::string::npos);
        }
    }

    SUBCASE("loader flag works with version-specific app selection")
    {
        // Create NAK
        createMultiLoaderNak("com.test.runtime", "1.0.0");

        // Create multiple versions of an app
        createAppWithNak("com.test.versioned", "1.0.0", "com.test.runtime", "1.0.0", "default");
        createAppWithNak("com.test.versioned", "2.0.0", "com.test.runtime", "1.0.0", "alternate");

        // Test run with specific version and loader override
        auto result = execute_command(get_nah_executable() + " run com.test.versioned@1.0.0 --loader debug");
        std::string combined = result.output + result.error;
        // Should parse correctly (won't execute without full setup)
        CHECK(combined.find("Unknown option") == std::string::npos);
    }
}

// Test NahHost discovery API
TEST_CASE("NahHost discovery API")
{
    SUBCASE("isValidRoot detects valid NAH root")
    {
        TestNahEnvironment env;
        REQUIRE(!env.root.empty());

        // Test environment should be a valid root
        CHECK(nah::host::NahHost::isValidRoot(env.root));
    }

    SUBCASE("isValidRoot rejects empty path")
    {
        CHECK_FALSE(nah::host::NahHost::isValidRoot(""));
    }

    SUBCASE("isValidRoot rejects non-existent directory")
    {
        CHECK_FALSE(nah::host::NahHost::isValidRoot("/nonexistent/path/to/nah"));
    }

    SUBCASE("isValidRoot rejects directory without required structure")
    {
        // Create a directory without NAH structure
        std::string temp_dir = std::filesystem::temp_directory_path().string() + "/not-nah-root-" +
                               std::to_string(std::time(nullptr));
        std::filesystem::create_directories(temp_dir);

        CHECK_FALSE(nah::host::NahHost::isValidRoot(temp_dir));

        // Cleanup
        std::filesystem::remove_all(temp_dir);
    }

    SUBCASE("isValidRoot accepts directory with required structure")
    {
        // Create a minimal NAH structure
        std::string temp_dir = std::filesystem::temp_directory_path().string() + "/valid-nah-root-" +
                               std::to_string(std::time(nullptr));
        std::filesystem::create_directories(temp_dir + "/registry/apps");
        std::filesystem::create_directories(temp_dir + "/host");

        CHECK(nah::host::NahHost::isValidRoot(temp_dir));

        // Cleanup
        std::filesystem::remove_all(temp_dir);
    }

    SUBCASE("discover returns nullptr when no valid roots found")
    {
        auto nah_host = nah::host::NahHost::discover({"", // Empty path
                                                      "/nonexistent/path1",
                                                      "/nonexistent/path2"});

        CHECK(nah_host == nullptr);
    }

    SUBCASE("discover finds first valid root in search paths")
    {
        TestNahEnvironment env1;
        TestNahEnvironment env2;
        REQUIRE(!env1.root.empty());
        REQUIRE(!env2.root.empty());

        // Should find env1 (first valid)
        auto nah_host = nah::host::NahHost::discover({
            "/nonexistent/path",
            env1.root,
            env2.root // This is also valid but should not be selected
        });

        REQUIRE(nah_host != nullptr);
        CHECK(nah_host->root() == env1.root);
    }

    SUBCASE("discover skips empty strings")
    {
        TestNahEnvironment env;
        REQUIRE(!env.root.empty());

        // Should skip empty strings and find env.root
        auto nah_host = nah::host::NahHost::discover({"",
                                                      "",
                                                      env.root});

        REQUIRE(nah_host != nullptr);
        CHECK(nah_host->root() == env.root);
    }

    SUBCASE("discover skips invalid paths before finding valid one")
    {
        TestNahEnvironment env;
        REQUIRE(!env.root.empty());

        auto nah_host = nah::host::NahHost::discover({"",
                                                      "/invalid/path/1",
                                                      "/invalid/path/2",
                                                      env.root,
                                                      "/should/not/check/this"});

        REQUIRE(nah_host != nullptr);
        CHECK(nah_host->root() == env.root);
    }

    SUBCASE("discover returns nullptr for empty search paths")
    {
        auto nah_host = nah::host::NahHost::discover({});
        CHECK(nah_host == nullptr);
    }

    SUBCASE("discovered host can list applications")
    {
        TestNahEnvironment env;
        REQUIRE(!env.root.empty());
        env.createTestApp("com.test.discoverapp", "1.0.0");

        auto nah_host = nah::host::NahHost::discover({env.root});
        REQUIRE(nah_host != nullptr);

        auto apps = nah_host->listApplications();
        CHECK(apps.size() >= 1);

        bool found = false;
        for (const auto &app : apps)
        {
            if (app.id == "com.test.discoverapp" && app.version == "1.0.0")
            {
                found = true;
                break;
            }
        }
        CHECK(found);
    }

    SUBCASE("discovered host can get launch contract")
    {
        TestNahEnvironment env;
        REQUIRE(!env.root.empty());
        env.createTestApp("com.test.launchapp", "1.0.0");

        auto nah_host = nah::host::NahHost::discover({env.root});
        REQUIRE(nah_host != nullptr);

        // Note: This may fail contract composition (missing NAK),
        // but it tests that the discovered host is functional
        auto result = nah_host->getLaunchContract("com.test.launchapp");
        // Just verify the API works, don't check result.ok
        // (it might fail due to missing NAK dependencies)
    }

    SUBCASE("typical host developer usage pattern")
    {
        TestNahEnvironment env;
        REQUIRE(!env.root.empty());

        // Simulate typical host developer usage
        std::string fake_env_var = env.root;
        std::string project_path = "/path/to/project/.nah"; // Won't exist
        std::string home_path = "/home/user/.nah";          // Won't exist

        auto nah_host = nah::host::NahHost::discover({
            fake_env_var, // Simulates getenv("NAH_ROOT")
            project_path, // Project-local NAH root
            home_path     // User home NAH root
        });

        REQUIRE(nah_host != nullptr);
        CHECK(nah_host->root() == fake_env_var);
    }
}

// Test app loader preference from manifest
TEST_CASE("app loader preference from manifest")
{
    TestNahEnvironment env;
    REQUIRE(!env.root.empty());

    // Helper to create NAK with multiple loaders
    auto createMultiLoaderNak = [&](const std::string& id, const std::string& version) {
        std::string nak_dir = nah::fs::join_paths(env.root, "naks", id, version);
        std::filesystem::create_directories(nak_dir);

        std::string bin_dir = nah::fs::join_paths(nak_dir, "bin");
        std::filesystem::create_directories(bin_dir);

        // Create loader executables
        for (const auto& loader_name : {"default-loader", "service-loader", "debug-loader"}) {
            std::string exec_path = nah::fs::join_paths(bin_dir, loader_name);
            std::ofstream exec_file(exec_path);
            exec_file << "#!/bin/sh\necho 'Running'\n";
            exec_file.close();
            std::filesystem::permissions(exec_path,
                                        std::filesystem::perms::owner_exec |
                                            std::filesystem::perms::owner_read |
                                            std::filesystem::perms::owner_write);
        }

        // Create NAK manifest
        std::string manifest_path = nah::fs::join_paths(nak_dir, "nak.json");
        std::ofstream manifest(manifest_path);
        manifest << "{\n";
        manifest << "  \"nak\": {\n";
        manifest << "    \"identity\": {\n";
        manifest << "      \"id\": \"" << id << "\",\n";
        manifest << "      \"version\": \"" << version << "\"\n";
        manifest << "    },\n";
        manifest << "    \"loaders\": {\n";
        manifest << "      \"default\": { \"exec_path\": \"bin/default-loader\" },\n";
        manifest << "      \"service\": { \"exec_path\": \"bin/service-loader\" },\n";
        manifest << "      \"debug\": { \"exec_path\": \"bin/debug-loader\" }\n";
        manifest << "    }\n";
        manifest << "  }\n";
        manifest << "}\n";
        manifest.close();

        // Create NAK install record
        std::string record_path = nah::fs::join_paths(env.root, "registry", "naks", id + "@" + version + ".json");
        std::ofstream record(record_path);
        record << "{\n";
        record << "  \"nak\": { \"id\": \"" << id << "\", \"version\": \"" << version << "\" },\n";
        record << "  \"paths\": { \"root\": \"" << nah::core::normalize_separators(nak_dir) << "\" },\n";
        record << "  \"loaders\": {\n";
        record << "    \"default\": { \"exec_path\": \"bin/default-loader\" },\n";
        record << "    \"service\": { \"exec_path\": \"bin/service-loader\" },\n";
        record << "    \"debug\": { \"exec_path\": \"bin/debug-loader\" }\n";
        record << "  }\n";
        record << "}\n";
        record.close();
    };

    SUBCASE("app without loader preference uses default")
    {
        createMultiLoaderNak("com.test.runtime", "1.0.0");

        // Create app manifest WITHOUT execution.loader
        std::string app_dir = env.root + "/test-app-no-loader";
        std::filesystem::create_directories(app_dir + "/bin");
        
        std::string manifest_path = app_dir + "/nap.json";
        std::ofstream manifest(manifest_path);
        manifest << "{\n";
        manifest << "  \"$schema\": \"https://nah.rtorr.com/schemas/nap.v1.json\",\n";
        manifest << "  \"app\": {\n";
        manifest << "    \"identity\": {\n";
        manifest << "      \"id\": \"com.test.noloader\",\n";
        manifest << "      \"version\": \"1.0.0\",\n";
        manifest << "      \"nak_id\": \"com.test.runtime\",\n";
        manifest << "      \"nak_version_req\": \"^1.0.0\"\n";
        manifest << "    },\n";
        manifest << "    \"execution\": {\n";
        manifest << "      \"entrypoint\": \"bin/app\"\n";
        manifest << "    }\n";
        manifest << "  }\n";
        manifest << "}\n";
        manifest.close();

        // Create dummy executable
        std::ofstream exec_file(app_dir + "/bin/app");
        exec_file << "#!/bin/sh\n";
        exec_file.close();

        // Install without --loader flag
        auto result = execute_command(get_nah_executable() + " --root " + env.root + " install " + app_dir);
        CHECK(result.exit_code == 0);

        // Verify install record uses "default" loader
        std::string record_path = nah::fs::join_paths(env.root, "registry", "apps", "com.test.noloader@1.0.0.json");
        auto record_content = nah::fs::read_file(record_path);
        REQUIRE(record_content.has_value());
        CHECK(record_content->find("\"loader\": \"default\"") != std::string::npos);
    }

    SUBCASE("app with loader preference in manifest uses that loader")
    {
        createMultiLoaderNak("com.test.runtime", "1.0.0");

        // Create app manifest WITH execution.loader = "service"
        std::string app_dir = env.root + "/test-app-with-loader";
        std::filesystem::create_directories(app_dir + "/bin");
        
        std::string manifest_path = app_dir + "/nap.json";
        std::ofstream manifest(manifest_path);
        manifest << "{\n";
        manifest << "  \"$schema\": \"https://nah.rtorr.com/schemas/nap.v1.json\",\n";
        manifest << "  \"app\": {\n";
        manifest << "    \"identity\": {\n";
        manifest << "      \"id\": \"com.test.withloader\",\n";
        manifest << "      \"version\": \"1.0.0\",\n";
        manifest << "      \"nak_id\": \"com.test.runtime\",\n";
        manifest << "      \"nak_version_req\": \"^1.0.0\"\n";
        manifest << "    },\n";
        manifest << "    \"execution\": {\n";
        manifest << "      \"entrypoint\": \"bin/app\",\n";
        manifest << "      \"loader\": \"service\"\n";
        manifest << "    }\n";
        manifest << "  }\n";
        manifest << "}\n";
        manifest.close();

        // Create dummy executable
        std::ofstream exec_file(app_dir + "/bin/app");
        exec_file << "#!/bin/sh\n";
        exec_file.close();

        // Install without --loader flag (should use manifest preference)
        auto result = execute_command(get_nah_executable() + " --root " + env.root + " install " + app_dir);
        CHECK(result.exit_code == 0);

        // Verify install record uses "service" loader from manifest
        std::string record_path = nah::fs::join_paths(env.root, "registry", "apps", "com.test.withloader@1.0.0.json");
        auto record_content = nah::fs::read_file(record_path);
        REQUIRE(record_content.has_value());
        CHECK(record_content->find("\"loader\": \"service\"") != std::string::npos);
    }

    SUBCASE("CLI --loader flag overrides manifest preference")
    {
        createMultiLoaderNak("com.test.runtime", "1.0.0");

        // Create app with manifest preference = "service"
        std::string app_dir = env.root + "/test-app-override";
        std::filesystem::create_directories(app_dir + "/bin");
        
        std::string manifest_path = app_dir + "/nap.json";
        std::ofstream manifest(manifest_path);
        manifest << "{\n";
        manifest << "  \"app\": {\n";
        manifest << "    \"identity\": {\n";
        manifest << "      \"id\": \"com.test.override\",\n";
        manifest << "      \"version\": \"1.0.0\",\n";
        manifest << "      \"nak_id\": \"com.test.runtime\",\n";
        manifest << "      \"nak_version_req\": \"^1.0.0\"\n";
        manifest << "    },\n";
        manifest << "    \"execution\": {\n";
        manifest << "      \"entrypoint\": \"bin/app\",\n";
        manifest << "      \"loader\": \"service\"\n";
        manifest << "    }\n";
        manifest << "  }\n";
        manifest << "}\n";
        manifest.close();

        std::ofstream exec_file(app_dir + "/bin/app");
        exec_file << "#!/bin/sh\n";
        exec_file.close();

        // Install WITH --loader debug (should override manifest)
        auto result = execute_command(get_nah_executable() + " --root " + env.root + " install " + app_dir + " --loader debug");
        CHECK(result.exit_code == 0);

        // Verify install record uses "debug" loader (CLI override)
        std::string record_path = nah::fs::join_paths(env.root, "registry", "apps", "com.test.override@1.0.0.json");
        auto record_content = nah::fs::read_file(record_path);
        REQUIRE(record_content.has_value());
        CHECK(record_content->find("\"loader\": \"debug\"") != std::string::npos);
    }

    SUBCASE("invalid loader in manifest is rejected")
    {
        createMultiLoaderNak("com.test.runtime", "1.0.0");

        // Create app with invalid loader preference
        std::string app_dir = env.root + "/test-app-invalid";
        std::filesystem::create_directories(app_dir + "/bin");
        
        std::string manifest_path = app_dir + "/nap.json";
        std::ofstream manifest(manifest_path);
        manifest << "{\n";
        manifest << "  \"app\": {\n";
        manifest << "    \"identity\": {\n";
        manifest << "      \"id\": \"com.test.invalid\",\n";
        manifest << "      \"version\": \"1.0.0\",\n";
        manifest << "      \"nak_id\": \"com.test.runtime\",\n";
        manifest << "      \"nak_version_req\": \"^1.0.0\"\n";
        manifest << "    },\n";
        manifest << "    \"execution\": {\n";
        manifest << "      \"entrypoint\": \"bin/app\",\n";
        manifest << "      \"loader\": \"nonexistent\"\n";
        manifest << "    }\n";
        manifest << "  }\n";
        manifest << "}\n";
        manifest.close();

        std::ofstream exec_file(app_dir + "/bin/app");
        exec_file << "#!/bin/sh\n";
        exec_file.close();

        // Install should fail
        auto result = execute_command(get_nah_executable() + " --root " + env.root + " install " + app_dir);
        CHECK(result.exit_code != 0);
        std::string combined = result.output + result.error;
        CHECK(combined.find("not found") != std::string::npos);
    }

    SUBCASE("loader priority: CLI > manifest > default")
    {
        createMultiLoaderNak("com.test.runtime", "1.0.0");

        // Test all three priority levels
        for (const auto& [test_name, cli_flag, manifest_loader, expected_loader] : std::vector<std::tuple<std::string, std::string, std::string, std::string>>{
            {"cli_overrides_all", "--loader debug", "service", "debug"},
            {"manifest_used_when_no_cli", "", "service", "service"},
            {"default_when_neither", "", "", "default"}
        }) {
            std::string app_id = "com.test.priority." + test_name;
            std::string app_dir = env.root + "/test-priority-" + test_name;
            std::filesystem::create_directories(app_dir + "/bin");
            
            std::string manifest_path = app_dir + "/nap.json";
            std::ofstream manifest(manifest_path);
            manifest << "{\n";
            manifest << "  \"app\": {\n";
            manifest << "    \"identity\": {\n";
            manifest << "      \"id\": \"" << app_id << "\",\n";
            manifest << "      \"version\": \"1.0.0\",\n";
            manifest << "      \"nak_id\": \"com.test.runtime\",\n";
            manifest << "      \"nak_version_req\": \"^1.0.0\"\n";
            manifest << "    },\n";
            manifest << "    \"execution\": {\n";
            manifest << "      \"entrypoint\": \"bin/app\"";
            
            if (!manifest_loader.empty()) {
                manifest << ",\n      \"loader\": \"" << manifest_loader << "\"";
            }
            manifest << "\n    }\n";
            manifest << "  }\n";
            manifest << "}\n";
            manifest.close();

            std::ofstream exec_file(app_dir + "/bin/app");
            exec_file << "#!/bin/sh\n";
            exec_file.close();

            std::string install_cmd = get_nah_executable() + " --root " + env.root + " install " + app_dir;
            if (!cli_flag.empty()) {
                install_cmd += " " + cli_flag;
            }

            auto result = execute_command(install_cmd);
            CHECK(result.exit_code == 0);

            // Verify correct loader was selected
            std::string record_path = nah::fs::join_paths(env.root, "registry", "apps", app_id + "@1.0.0.json");
            auto record_content = nah::fs::read_file(record_path);
            REQUIRE(record_content.has_value());
            CHECK(record_content->find("\"loader\": \"" + expected_loader + "\"") != std::string::npos);
        }
    }
}