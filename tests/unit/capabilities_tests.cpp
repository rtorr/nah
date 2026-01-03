#include <doctest/doctest.h>
#include <nah/capabilities.hpp>

using namespace nah;

TEST_CASE("derive_capability returns correct capability for filesystem operations") {
    auto cap = derive_capability("read", "/path/to/file");
    REQUIRE(cap.has_value());
    CHECK(cap->operation == "read");
    CHECK(cap->resource == "/path/to/file");
    CHECK(cap->key == "fs.read./path/to/file");
}

TEST_CASE("derive_capability handles write operation") {
    auto cap = derive_capability("write", "/var/data");
    REQUIRE(cap.has_value());
    CHECK(cap->operation == "write");
    CHECK(cap->resource == "/var/data");
    CHECK(cap->key == "fs.write./var/data");
}

TEST_CASE("derive_capability handles execute operation") {
    auto cap = derive_capability("execute", "/usr/bin/tool");
    REQUIRE(cap.has_value());
    CHECK(cap->operation == "execute");
    CHECK(cap->resource == "/usr/bin/tool");
    CHECK(cap->key == "fs.execute./usr/bin/tool");
}

TEST_CASE("derive_capability handles network connect operation") {
    auto cap = derive_capability("connect", "https://api.example.com");
    REQUIRE(cap.has_value());
    CHECK(cap->operation == "connect");
    CHECK(cap->resource == "https://api.example.com");
    CHECK(cap->key == "net.connect.https://api.example.com");
}

TEST_CASE("derive_capability handles network listen operation") {
    auto cap = derive_capability("listen", "tcp://0.0.0.0:8080");
    REQUIRE(cap.has_value());
    CHECK(cap->operation == "listen");
    CHECK(cap->resource == "tcp://0.0.0.0:8080");
    CHECK(cap->key == "net.listen.tcp://0.0.0.0:8080");
}

TEST_CASE("derive_capability handles network bind operation") {
    auto cap = derive_capability("bind", "udp://localhost:5353");
    REQUIRE(cap.has_value());
    CHECK(cap->operation == "bind");
    CHECK(cap->resource == "udp://localhost:5353");
    CHECK(cap->key == "net.bind.udp://localhost:5353");
}

TEST_CASE("derive_capability returns nullopt for unknown operation") {
    CHECK_FALSE(derive_capability("unknown", "/path").has_value());
    CHECK_FALSE(derive_capability("", "/path").has_value());
    CHECK_FALSE(derive_capability("delete", "/path").has_value());
}

TEST_CASE("derive_enforcement maps capability to enforcement ID") {
    HostProfile profile;
    profile.capabilities["fs.read./data"] = "sandbox.allow.read";
    profile.capabilities["net.connect.*"] = "firewall.allow.egress";
    
    auto enforcement = derive_enforcement("fs.read./data", profile);
    REQUIRE(enforcement.has_value());
    CHECK(*enforcement == "sandbox.allow.read");
}

TEST_CASE("derive_enforcement returns nullopt for unmapped capability") {
    HostProfile profile;
    profile.capabilities["fs.read./data"] = "sandbox.allow.read";
    
    auto enforcement = derive_enforcement("fs.write./other", profile);
    CHECK_FALSE(enforcement.has_value());
}

TEST_CASE("derive_enforcement handles wildcard mappings") {
    HostProfile profile;
    profile.capabilities["net.connect.*"] = "firewall.allow.all";
    
    // Exact match takes precedence
    auto exact = derive_enforcement("net.connect.*", profile);
    REQUIRE(exact.has_value());
    CHECK(*exact == "firewall.allow.all");
}

TEST_CASE("parse_permission_string handles filesystem permissions") {
    auto perm = parse_permission_string("fs:read:/home/user");
    REQUIRE(perm.has_value());
    CHECK(perm->type == "fs");
    CHECK(perm->operation == "read");
    CHECK(perm->resource == "/home/user");
}

TEST_CASE("parse_permission_string handles network permissions") {
    auto perm = parse_permission_string("net:connect:https://example.com");
    REQUIRE(perm.has_value());
    CHECK(perm->type == "net");
    CHECK(perm->operation == "connect");
    CHECK(perm->resource == "https://example.com");
}

TEST_CASE("parse_permission_string rejects invalid format") {
    CHECK_FALSE(parse_permission_string("invalid").has_value());
    CHECK_FALSE(parse_permission_string("fs:read").has_value());
    CHECK_FALSE(parse_permission_string("").has_value());
    CHECK_FALSE(parse_permission_string("::").has_value());
}

TEST_CASE("derive_capabilities_from_permissions converts permission list") {
    std::vector<std::string> permissions = {
        "fs:read:/data",
        "fs:write:/tmp",
        "net:connect:https://api.example.com"
    };
    
    auto caps = derive_capabilities_from_permissions(permissions);
    
    CHECK(caps.size() == 3);
    
    bool found_read = false, found_write = false, found_connect = false;
    for (const auto& cap : caps) {
        if (cap.key == "fs.read./data") found_read = true;
        if (cap.key == "fs.write./tmp") found_write = true;
        if (cap.key == "net.connect.https://api.example.com") found_connect = true;
    }
    
    CHECK(found_read);
    CHECK(found_write);
    CHECK(found_connect);
}

TEST_CASE("derive_capabilities_from_permissions skips invalid permissions") {
    std::vector<std::string> permissions = {
        "fs:read:/valid",
        "invalid_format",
        "net:connect:https://example.com"
    };
    
    auto caps = derive_capabilities_from_permissions(permissions);
    
    CHECK(caps.size() == 2);
}
