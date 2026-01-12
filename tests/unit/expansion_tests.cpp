#include <doctest/doctest.h>
#include <nah/expansion.hpp>

using namespace nah;

TEST_CASE("expand_placeholders performs basic substitution") {
    std::unordered_map<std::string, std::string> env;
    env["FOO"] = "bar";
    env["BAZ"] = "qux";
    
    std::vector<std::string> missing;
    auto result = expand_placeholders("hello {FOO} world {BAZ}", env, missing);
    
    CHECK(result == "hello bar world qux");
    CHECK(missing.empty());
}

TEST_CASE("expand_placeholders leaves unknown placeholders with warning") {
    std::unordered_map<std::string, std::string> env;
    env["FOO"] = "bar";
    
    std::vector<std::string> missing;
    auto result = expand_placeholders("hello {FOO} {UNKNOWN}", env, missing);
    
    CHECK(result == "hello bar {UNKNOWN}");
    REQUIRE(missing.size() == 1);
    CHECK(missing[0] == "UNKNOWN");
}

TEST_CASE("expand_placeholders handles empty env") {
    std::unordered_map<std::string, std::string> env;
    std::vector<std::string> missing;
    
    auto result = expand_placeholders("{A} {B}", env, missing);
    
    CHECK(result == "{A} {B}");
    CHECK(missing.size() == 2);
}

TEST_CASE("expand_placeholders handles string with no placeholders") {
    std::unordered_map<std::string, std::string> env;
    env["FOO"] = "bar";
    std::vector<std::string> missing;
    
    auto result = expand_placeholders("no placeholders here", env, missing);
    
    CHECK(result == "no placeholders here");
    CHECK(missing.empty());
}

TEST_CASE("expand_placeholders handles adjacent placeholders") {
    std::unordered_map<std::string, std::string> env;
    env["A"] = "1";
    env["B"] = "2";
    std::vector<std::string> missing;
    
    auto result = expand_placeholders("{A}{B}", env, missing);
    
    CHECK(result == "12");
    CHECK(missing.empty());
}

TEST_CASE("expand_placeholders handles placeholder at start") {
    std::unordered_map<std::string, std::string> env;
    env["PREFIX"] = "/usr";
    std::vector<std::string> missing;
    
    auto result = expand_placeholders("{PREFIX}/lib", env, missing);
    
    CHECK(result == "/usr/lib");
}

TEST_CASE("expand_placeholders handles placeholder at end") {
    std::unordered_map<std::string, std::string> env;
    env["SUFFIX"] = ".so";
    std::vector<std::string> missing;
    
    auto result = expand_placeholders("libfoo{SUFFIX}", env, missing);
    
    CHECK(result == "libfoo.so");
}

TEST_CASE("expand_placeholders handles empty placeholder name") {
    std::unordered_map<std::string, std::string> env;
    env[""] = "empty";
    std::vector<std::string> missing;
    
    auto result = expand_placeholders("test {} text", env, missing);
    
    // Empty placeholder name should be left as-is
    CHECK(result == "test {} text");
}

TEST_CASE("expand_placeholders handles nested braces") {
    std::unordered_map<std::string, std::string> env;
    env["FOO"] = "bar";
    std::vector<std::string> missing;
    
    auto result = expand_placeholders("{{FOO}}", env, missing);
    
    // First { is literal, {FOO} is expanded
    CHECK(result == "{bar}");
}

TEST_CASE("expand_placeholders_with_limits respects size limit") {
    std::unordered_map<std::string, std::string> env;
    std::string long_value(1000, 'x');
    env["LONG"] = long_value;
    
    std::vector<std::string> missing;
    auto result = expand_placeholders_with_limits(
        "{LONG}{LONG}{LONG}", env, missing, 100, 128);
    
    CHECK(result.truncated);
    CHECK(result.value.size() <= 100);
}

TEST_CASE("expand_placeholders_with_limits respects placeholder count limit") {
    std::unordered_map<std::string, std::string> env;
    for (int i = 0; i < 200; ++i) {
        env["VAR" + std::to_string(i)] = "v";
    }
    
    std::string input;
    for (int i = 0; i < 200; ++i) {
        input += "{VAR" + std::to_string(i) + "}";
    }
    
    std::vector<std::string> missing;
    auto result = expand_placeholders_with_limits(
        input, env, missing, 64 * 1024, 50);
    
    CHECK(result.limit_exceeded);
}

TEST_CASE("expand_placeholders handles NAH-specific variables") {
    std::unordered_map<std::string, std::string> env;
    env["NAH_APP_ROOT"] = "/nah/apps/myapp-1.0.0";
    env["NAH_APP_ENTRY"] = "bin/myapp";
    env["NAH_NAK_ROOT"] = "/nah/naks/mynak/1.0.0";
    
    std::vector<std::string> missing;
    auto result = expand_placeholders(
        "--root={NAH_APP_ROOT} --entry={NAH_APP_ENTRY}", env, missing);
    
    CHECK(result == "--root=/nah/apps/myapp-1.0.0 --entry=bin/myapp");
    CHECK(missing.empty());
}

TEST_CASE("expand_vector expands all strings in vector") {
    std::unordered_map<std::string, std::string> env;
    env["ROOT"] = "/app";
    env["LIB"] = "lib";
    
    std::vector<std::string> input = {"{ROOT}/bin", "{ROOT}/{LIB}", "{ROOT}/share"};
    std::vector<std::string> missing;
    
    auto result = expand_vector(input, env, missing);
    
    REQUIRE(result.size() == 3);
    CHECK(result[0] == "/app/bin");
    CHECK(result[1] == "/app/lib");
    CHECK(result[2] == "/app/share");
    CHECK(missing.empty());
}

TEST_CASE("expand_placeholders supports $NAME shell syntax") {
    std::unordered_map<std::string, std::string> env;
    env["NAH_APP_ROOT"] = "/nah/apps/myapp";
    env["PATH"] = "/usr/bin";
    
    std::vector<std::string> missing;
    auto result = expand_placeholders("$NAH_APP_ROOT/bin:$PATH", env, missing);
    
    CHECK(result == "/nah/apps/myapp/bin:/usr/bin");
    CHECK(missing.empty());
}

TEST_CASE("expand_placeholders supports ${NAME} shell syntax") {
    std::unordered_map<std::string, std::string> env;
    env["NAH_APP_ROOT"] = "/nah/apps/myapp";
    env["SUFFIX"] = ".cache";
    
    std::vector<std::string> missing;
    auto result = expand_placeholders("${NAH_APP_ROOT}/.devbox${SUFFIX}", env, missing);
    
    CHECK(result == "/nah/apps/myapp/.devbox.cache");
    CHECK(missing.empty());
}

TEST_CASE("expand_placeholders handles mixed syntax") {
    std::unordered_map<std::string, std::string> env;
    env["A"] = "alpha";
    env["B"] = "beta";
    env["C"] = "gamma";
    
    std::vector<std::string> missing;
    auto result = expand_placeholders("{A}:$B:${C}", env, missing);
    
    CHECK(result == "alpha:beta:gamma");
    CHECK(missing.empty());
}

TEST_CASE("expand_placeholders handles lone $ literally") {
    std::unordered_map<std::string, std::string> env;
    env["FOO"] = "bar";
    
    std::vector<std::string> missing;
    auto result = expand_placeholders("cost: $5 and {FOO}", env, missing);
    
    CHECK(result == "cost: $5 and bar");
    CHECK(missing.empty());
}
