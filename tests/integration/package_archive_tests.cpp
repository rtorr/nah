#include <doctest/doctest.h>
#include <nah/nah_archive.h>

#include <array>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("package extraction rejects parent traversal") {
    using namespace nah::archive;
    const auto base = fs::temp_directory_path() / "nah-archive-traversal-test";
    const auto archive = base / "malicious.nap";
    const auto destination = base / "destination";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base);

    gzFile output = gzopen(archive.string().c_str(), "wb");
    REQUIRE(output != nullptr);
    std::array<unsigned char, 512> header{};
    const std::string name = "../escaped";
    std::memcpy(header.data(), name.data(), name.size());
    CHECK(put_octal(header.data() + 100, 8, 0644));
    CHECK(put_octal(header.data() + 108, 8, 0));
    CHECK(put_octal(header.data() + 116, 8, 0));
    CHECK(put_octal(header.data() + 124, 12, 1));
    CHECK(put_octal(header.data() + 136, 12, 0));
    std::memset(header.data() + 148, ' ', 8);
    header[156] = '0';
    std::memcpy(header.data() + 257, "ustar", 5);
    std::memcpy(header.data() + 263, "00", 2);
    std::uint64_t checksum = 0;
    for (const auto byte : header) checksum += byte;
    CHECK(put_octal(header.data() + 148, 7, checksum));
    header[155] = ' ';
    REQUIRE(write_gzip(output, header.data(), header.size()));
    std::array<unsigned char, 512> data{};
    data[0] = 'x';
    REQUIRE(write_gzip(output, data.data(), data.size()));
    REQUIRE(write_gzip(output, data.data(), data.size()));
    REQUIRE(write_gzip(output, data.data(), data.size()));
    REQUIRE(gzclose(output) == Z_OK);

    const auto result = extract(archive, destination);
    CHECK_FALSE(result.ok);
    CHECK_FALSE(fs::exists(base / "escaped"));
    fs::remove_all(base, ec);
}

#ifndef _WIN32
TEST_CASE("package creation rejects symbolic links") {
    using namespace nah::archive;
    const auto base = fs::temp_directory_path() / "nah-archive-symlink-test";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base / "source");
    fs::create_symlink("missing", base / "source" / "link", ec);
    REQUIRE_FALSE(ec);
    const auto result = create(base / "source", base / "output.nap");
    CHECK_FALSE(result.ok);
    fs::remove_all(base, ec);
}
#endif
