#include <doctest/doctest.h>
#include <nah/nah_digest.h>

#include <filesystem>
#include <fstream>

TEST_CASE("SHA-256 binds an artifact to its bytes") {
    const auto path = std::filesystem::temp_directory_path() / "nah-digest-test";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "abc";
    }

    const auto result = nah::digest::sha256_file(path);
    CHECK(result.ok);
    CHECK(result.file.size == 3);
    CHECK(result.file.value.canonical() ==
          "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("SHA-256 rejects directories and links") {
    const auto base = std::filesystem::temp_directory_path() / "nah-digest-input-test";
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    std::filesystem::create_directories(base, ec);
    REQUIRE_FALSE(ec);
    CHECK_FALSE(nah::digest::sha256_file(base).ok);

#ifndef _WIN32
    std::filesystem::create_symlink(base, base / "link", ec);
    REQUIRE_FALSE(ec);
    CHECK_FALSE(nah::digest::sha256_file(base / "link").ok);
#endif

    std::filesystem::remove_all(base, ec);
}
