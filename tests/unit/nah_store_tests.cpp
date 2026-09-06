#include <doctest/doctest.h>
#include <nah/nah_store.h>

#include <filesystem>
#include <fstream>

namespace {

void write_text(const std::filesystem::path& path, const std::string& value) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << value;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct TempStore {
    std::filesystem::path root = std::filesystem::temp_directory_path() / "nah-store-test";
    TempStore() { std::error_code ec; std::filesystem::remove_all(root, ec); }
    ~TempStore() { std::error_code ec; std::filesystem::remove_all(root, ec); }
};

} // namespace

TEST_CASE("store installs, replaces, and removes one package transactionally") {
    TempStore temp;
    const auto source = temp.root.parent_path() / "nah-store-source";
    std::error_code ec;
    std::filesystem::remove_all(source, ec);
    write_text(source / "nap.json", "first");

    nah::store::Store store(temp.root);
    auto result = store.install(source, "apps/com.example-1.0.0",
                                "registry/apps/com.example@1.0.0.json", "first-record\n");
    REQUIRE(result.ok);
    CHECK(read_text(temp.root / "apps/com.example-1.0.0/nap.json") == "first");
    CHECK_FALSE(store.install(source, "apps/com.example-1.0.0",
                              "registry/apps/com.example@1.0.0.json", "record\n").ok);

    write_text(source / "nap.json", "second");
    result = store.install(source, "apps/com.example-1.0.0",
                           "registry/apps/com.example@1.0.0.json", "second-record\n", true);
    REQUIRE(result.ok);
    CHECK(read_text(temp.root / "apps/com.example-1.0.0/nap.json") == "second");
    CHECK(read_text(temp.root / "registry/apps/com.example@1.0.0.json") == "second-record\n");

    result = store.remove("apps/com.example-1.0.0", "registry/apps/com.example@1.0.0.json");
    CHECK(result.ok);
    CHECK_FALSE(std::filesystem::exists(temp.root / "apps/com.example-1.0.0"));
    CHECK_FALSE(std::filesystem::exists(temp.root / "registry/apps/com.example@1.0.0.json"));
    std::filesystem::remove_all(source, ec);
}

TEST_CASE("store rejects paths outside its managed layout") {
    TempStore temp;
    nah::store::Store store(temp.root);
    CHECK_FALSE(store.install(temp.root, "../outside", "registry/apps/a.json", "{}").ok);
    CHECK_FALSE(store.remove("apps/a", "registry/naks/a.json").ok);
}

TEST_CASE("store recovers an interrupted replacement") {
    TempStore temp;
    nah::store::Store store(temp.root);
    REQUIRE(store.initialize().ok);
    const std::string operation_id(32, 'a');
    const auto operation = temp.root / "staging" / operation_id;
    write_text(operation / "old-payload/nap.json", "old");
    write_text(operation / "old-record.json", "old-record\n");
    write_text(temp.root / "apps/com.example-1.0.0/nap.json", "new");
    write_text(temp.root / "registry/apps/com.example@1.0.0.json", "new-record\n");
    write_text(temp.root / "staging/transaction.json",
        "{\"version\":1,\"kind\":\"install\",\"phase\":\"payload_active\","
        "\"operation\":\"" + operation_id + "\","
        "\"payload\":\"apps/com.example-1.0.0\","
        "\"record\":\"registry/apps/com.example@1.0.0.json\"}\n");

    REQUIRE(store.recover().ok);
    CHECK(read_text(temp.root / "apps/com.example-1.0.0/nap.json") == "old");
    CHECK(read_text(temp.root / "registry/apps/com.example@1.0.0.json") == "old-record\n");
    CHECK_FALSE(std::filesystem::exists(temp.root / "staging/transaction.json"));
}
