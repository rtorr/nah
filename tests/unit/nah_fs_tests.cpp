/**
 * Unit tests for nah_fs.h filesystem functions
 */

#define NAH_FS_IMPLEMENTATION
#include <nah/nah_fs.h>
#include <doctest/doctest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <ctime>

// Helper to create temporary test directory
class TempTestDir {
public:
    TempTestDir() {
        // Create unique temp directory using portable C++
        std::string temp_base;
#ifdef _WIN32
        const char* tmp = std::getenv("TEMP");
        if (!tmp) tmp = std::getenv("TMP");
        if (!tmp) tmp = ".";
        temp_base = tmp;
#else
        temp_base = "/tmp";
#endif
        // Generate unique name using time and random
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        std::string unique_name = "nah_test_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(std::rand());
        path = temp_base + "/" + unique_name;
        std::filesystem::create_directories(path);
    }

    ~TempTestDir() {
        if (!path.empty()) {
            std::filesystem::remove_all(path);
        }
    }

    std::string path;
};

TEST_CASE("nah::fs::exists") {
    TempTestDir temp_dir;
    REQUIRE(!temp_dir.path.empty());

    SUBCASE("directory exists") {
        CHECK(nah::fs::exists(temp_dir.path));
    }

    SUBCASE("file exists") {
        std::string file_path = temp_dir.path + "/test.txt";
        std::ofstream(file_path) << "test content";
        CHECK(nah::fs::exists(file_path));
    }

    SUBCASE("non-existent path") {
        CHECK(!nah::fs::exists(temp_dir.path + "/non_existent"));
    }
}

TEST_CASE("nah::fs::is_directory") {
    TempTestDir temp_dir;
    REQUIRE(!temp_dir.path.empty());

    SUBCASE("is directory") {
        CHECK(nah::fs::is_directory(temp_dir.path));
    }

    SUBCASE("file is not directory") {
        std::string file_path = temp_dir.path + "/test.txt";
        std::ofstream(file_path) << "test content";
        CHECK(!nah::fs::is_directory(file_path));
    }

    SUBCASE("non-existent path is not directory") {
        CHECK(!nah::fs::is_directory(temp_dir.path + "/non_existent"));
    }
}

TEST_CASE("nah::fs::is_file") {
    TempTestDir temp_dir;
    REQUIRE(!temp_dir.path.empty());

    SUBCASE("regular file is file") {
        std::string file_path = temp_dir.path + "/test.txt";
        std::ofstream(file_path) << "test content";
        CHECK(nah::fs::is_file(file_path));
    }

    SUBCASE("directory is not file") {
        CHECK(!nah::fs::is_file(temp_dir.path));
    }

    SUBCASE("non-existent path is not file") {
        CHECK(!nah::fs::is_file(temp_dir.path + "/non_existent"));
    }
}

TEST_CASE("nah::fs::read_file") {
    TempTestDir temp_dir;
    REQUIRE(!temp_dir.path.empty());

    SUBCASE("read existing file") {
        std::string test_file = temp_dir.path + "/test.txt";
        std::string content = "Hello, NAH!";
        std::ofstream(test_file) << content;

        auto result = nah::fs::read_file(test_file);
        REQUIRE(result.has_value());
        CHECK(*result == content);
    }

    SUBCASE("read non-existent file") {
        auto result = nah::fs::read_file(temp_dir.path + "/non_existent.txt");
        CHECK(!result.has_value());
    }

    SUBCASE("read empty file") {
        std::string empty_file = temp_dir.path + "/empty.txt";
        std::ofstream ofs(empty_file);  // Create empty file
        ofs.close();

        auto result = nah::fs::read_file(empty_file);
        REQUIRE(result.has_value());
        CHECK(result->empty());
    }

    SUBCASE("read file with newlines") {
        std::string multi_file = temp_dir.path + "/multiline.txt";
        std::string content = "Line 1\nLine 2\nLine 3";
        std::ofstream(multi_file) << content;

        auto result = nah::fs::read_file(multi_file);
        REQUIRE(result.has_value());
        CHECK(*result == content);
    }
}

TEST_CASE("nah::fs::write_file") {
    TempTestDir temp_dir;
    REQUIRE(!temp_dir.path.empty());

    SUBCASE("write new file") {
        std::string new_file = temp_dir.path + "/new.txt";
        std::string content = "New file content";

        bool success = nah::fs::write_file(new_file, content);
        CHECK(success);

        // Verify content
        auto read_result = nah::fs::read_file(new_file);
        REQUIRE(read_result.has_value());
        CHECK(*read_result == content);
    }

    SUBCASE("overwrite existing file") {
        std::string existing_file = temp_dir.path + "/existing.txt";
        std::ofstream(existing_file) << "Old content";

        std::string new_content = "New content";
        bool success = nah::fs::write_file(existing_file, new_content);
        CHECK(success);

        // Verify new content
        auto read_result = nah::fs::read_file(existing_file);
        REQUIRE(read_result.has_value());
        CHECK(*read_result == new_content);
    }

    SUBCASE("write to non-existent directory fails") {
        std::string bad_file = temp_dir.path + "/non_existent_dir/file.txt";
        bool success = nah::fs::write_file(bad_file, "content");
        CHECK(!success);
    }

    SUBCASE("write empty content") {
        std::string empty_out = temp_dir.path + "/empty.txt";
        bool success = nah::fs::write_file(empty_out, "");
        CHECK(success);

        // Verify empty file was created
        CHECK(nah::fs::exists(empty_out));
        auto read_result = nah::fs::read_file(empty_out);
        REQUIRE(read_result.has_value());
        CHECK(read_result->empty());
    }
}

// Note: write_file_atomic not in current nah_fs.h API

TEST_CASE("nah::fs::list_directory") {
    TempTestDir temp_dir;
    REQUIRE(!temp_dir.path.empty());

    SUBCASE("list empty directory") {
        auto entries = nah::fs::list_directory(temp_dir.path);
        CHECK(entries.empty());
    }

    SUBCASE("list directory with files") {
        // Create some test files
        std::ofstream(temp_dir.path + "/file1.txt") << "content1";
        std::ofstream(temp_dir.path + "/file2.json") << "{}";
        std::ofstream(temp_dir.path + "/file3.dat") << "data";

        auto entries = nah::fs::list_directory(temp_dir.path);
        CHECK(entries.size() == 3);

        // Entries should be full paths
        for (const auto& entry : entries) {
            CHECK(entry.find(temp_dir.path) == 0);
        }
    }

    SUBCASE("list directory with subdirectories") {
        // Create subdirectory
        std::string subdir = temp_dir.path + "/subdir";
        std::filesystem::create_directory(subdir);
        std::ofstream(temp_dir.path + "/file.txt") << "content";

        auto entries = nah::fs::list_directory(temp_dir.path);
        CHECK(entries.size() == 2);  // One file, one directory
    }

    SUBCASE("list non-existent directory") {
        auto entries = nah::fs::list_directory(temp_dir.path + "/non_existent");
        CHECK(entries.empty());
    }
}

// Note: make_executable not in current nah_fs.h API

TEST_CASE("nah::fs::current_path") {
    auto cwd = nah::fs::current_path();
    CHECK(!cwd.empty());
    CHECK(nah::fs::exists(cwd));
    CHECK(nah::fs::is_directory(cwd));
}

TEST_CASE("nah::fs::create_directories") {
    TempTestDir temp_dir;
    REQUIRE(!temp_dir.path.empty());

    SUBCASE("create new directory") {
        std::string new_dir = temp_dir.path + "/new_directory";
        CHECK(!nah::fs::exists(new_dir));

        bool success = nah::fs::create_directories(new_dir);
        CHECK(success);
        CHECK(nah::fs::exists(new_dir));
        CHECK(nah::fs::is_directory(new_dir));
    }

    SUBCASE("create nested directories") {
        std::string nested = temp_dir.path + "/level1/level2/level3";
        CHECK(!nah::fs::exists(nested));

        bool success = nah::fs::create_directories(nested);
        CHECK(success);
        CHECK(nah::fs::exists(nested));
        CHECK(nah::fs::is_directory(nested));
    }

    SUBCASE("existing directory returns true") {
        bool success = nah::fs::create_directories(temp_dir.path);
        CHECK(success);
    }
}

TEST_CASE("nah::fs::remove_file") {
    TempTestDir temp_dir;
    REQUIRE(!temp_dir.path.empty());

    SUBCASE("remove existing file") {
        std::string file_path = temp_dir.path + "/to_remove.txt";
        std::ofstream(file_path) << "content";
        CHECK(nah::fs::exists(file_path));

        bool success = nah::fs::remove_file(file_path);
        CHECK(success);
        CHECK(!nah::fs::exists(file_path));
    }

    SUBCASE("remove non-existent file") {
        std::string file_path = temp_dir.path + "/non_existent.txt";
        bool success = nah::fs::remove_file(file_path);
        CHECK(success);  // Removing non-existent file is considered success
    }

    SUBCASE("can remove directory with remove_file") {
        // Note: nah::fs::remove_file appears to handle both files and directories
        std::string subdir = temp_dir.path + "/subdir";
        std::filesystem::create_directory(subdir);

        bool success = nah::fs::remove_file(subdir);
        CHECK(success);
        CHECK(!nah::fs::exists(subdir));  // Directory should be removed
    }
}

TEST_CASE("nah::fs::remove_directory") {
    TempTestDir temp_dir;
    REQUIRE(!temp_dir.path.empty());

    SUBCASE("remove empty directory") {
        std::string subdir = temp_dir.path + "/empty_dir";
        std::filesystem::create_directory(subdir);
        CHECK(nah::fs::exists(subdir));

        bool success = nah::fs::remove_directory(subdir);
        CHECK(success);
        CHECK(!nah::fs::exists(subdir));
    }

    SUBCASE("remove directory with contents") {
        std::string subdir = temp_dir.path + "/full_dir";
        std::filesystem::create_directory(subdir);
        std::ofstream(subdir + "/file1.txt") << "content1";
        std::ofstream(subdir + "/file2.txt") << "content2";
        std::filesystem::create_directory(subdir + "/nested");
        std::ofstream(subdir + "/nested/file3.txt") << "content3";

        bool success = nah::fs::remove_directory(subdir);
        CHECK(success);
        CHECK(!nah::fs::exists(subdir));
    }

    SUBCASE("remove non-existent directory") {
        std::string subdir = temp_dir.path + "/non_existent";
        bool success = nah::fs::remove_directory(subdir);
        CHECK(success);  // Removing non-existent directory is considered success
    }
}

TEST_CASE("nah::fs::copy_file") {
    TempTestDir temp_dir;
    REQUIRE(!temp_dir.path.empty());

    SUBCASE("copy file") {
        std::string source = temp_dir.path + "/source.txt";
        std::string dest = temp_dir.path + "/dest.txt";
        std::string content = "File content to copy";

        std::ofstream(source) << content;

        bool success = nah::fs::copy_file(source, dest);
        CHECK(success);
        CHECK(nah::fs::exists(dest));

        auto dest_content = nah::fs::read_file(dest);
        REQUIRE(dest_content.has_value());
        CHECK(*dest_content == content);
    }

    SUBCASE("overwrite existing destination") {
        std::string source = temp_dir.path + "/source.txt";
        std::string dest = temp_dir.path + "/dest.txt";

        std::ofstream(source) << "New content";
        std::ofstream(dest) << "Old content";

        bool success = nah::fs::copy_file(source, dest);
        CHECK(success);

        auto dest_content = nah::fs::read_file(dest);
        REQUIRE(dest_content.has_value());
        CHECK(*dest_content == "New content");
    }

    SUBCASE("copy non-existent source fails") {
        std::string source = temp_dir.path + "/non_existent.txt";
        std::string dest = temp_dir.path + "/dest.txt";

        bool success = nah::fs::copy_file(source, dest);
        CHECK(!success);
        CHECK(!nah::fs::exists(dest));
    }
}

TEST_CASE("nah::fs::filename") {
    SUBCASE("extract filename from path") {
        CHECK(nah::fs::filename("/path/to/file.txt") == "file.txt");
        CHECK(nah::fs::filename("/path/to/directory/") == "");
        CHECK(nah::fs::filename("file.txt") == "file.txt");
        CHECK(nah::fs::filename("/") == "");
        CHECK(nah::fs::filename("") == "");
    }
}

TEST_CASE("nah::fs::parent_path") {
    SUBCASE("extract parent directory from path") {
        CHECK(nah::fs::parent_path("/path/to/file.txt") == "/path/to");
        CHECK(nah::fs::parent_path("/path/to/directory/") == "/path/to/directory");
        CHECK(nah::fs::parent_path("file.txt") == "");
        CHECK(nah::fs::parent_path("/file.txt") == "/");
        CHECK(nah::fs::parent_path("/") == "/");
    }
}

TEST_CASE("nah::fs::canonical_path") {
    TempTestDir temp_dir;
    REQUIRE(!temp_dir.path.empty());

    SUBCASE("resolve existing path") {
        auto resolved = nah::fs::canonical_path(temp_dir.path);
        REQUIRE(resolved.has_value());
        CHECK(nah::fs::exists(*resolved));
    }

    SUBCASE("resolve non-existent path returns nullopt") {
        auto resolved = nah::fs::canonical_path(temp_dir.path + "/non_existent");
        CHECK(!resolved.has_value());
    }
}

TEST_CASE("nah::fs::absolute_path") {
    SUBCASE("convert relative to absolute") {
        std::string abs = nah::fs::absolute_path(".");
        CHECK(!abs.empty());
        CHECK(abs[0] == '/');  // On Unix, absolute paths start with /
    }

    SUBCASE("absolute path unchanged") {
        std::string path = "/absolute/path";
        CHECK(nah::fs::absolute_path(path) == path);
    }
}