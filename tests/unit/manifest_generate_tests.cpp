#include <doctest/doctest.h>
#include <nah/manifest_generate.hpp>
#include <nah/manifest.hpp>

#include <string>
#include <vector>

// ============================================================================
// Tests: Manifest Input Parsing
// ============================================================================

TEST_CASE("parse_manifest_input: valid minimal input") {
    const char* json = R"({
        "app": {
            "id": "com.example.myapp",
            "version": "1.0.0",
            "nak_id": "com.example.runtime",
            "nak_version_req": ">=2.0.0",
            "entrypoint": "bundle.js"
        }
    })";
    
    auto result = nah::parse_manifest_input(json);
    CHECK(result.ok);
    CHECK(result.error.empty());
    CHECK(result.input.id == "com.example.myapp");
    CHECK(result.input.version == "1.0.0");
    CHECK(result.input.nak_id == "com.example.runtime");
    CHECK(result.input.nak_version_req == ">=2.0.0");
    CHECK(result.input.entrypoint == "bundle.js");
}

TEST_CASE("parse_manifest_input: valid full input") {
    const char* json = R"({
        "app": {
            "id": "com.example.myapp",
            "version": "1.0.0",
            "nak_id": "com.example.runtime",
            "nak_version_req": ">=2.0.0",
            "entrypoint": "bundle.js",
            "entrypoint_args": ["--mode", "production"],
            "description": "My Application",
            "author": "Developer",
            "license": "MIT",
            "homepage": "https://example.com",
            "lib_dirs": ["lib", "vendor/lib"],
            "asset_dirs": ["assets"],
            "exports": [
                {
                    "id": "config",
                    "path": "share/config.json",
                    "type": "application/json"
                },
                {
                    "id": "splash",
                    "path": "assets/splash.png"
                }
            ],
            "environment": {
                "NODE_ENV": "production",
                "LOG_LEVEL": "info"
            },
            "permissions": {
                "filesystem": ["read:app://assets/*"],
                "network": ["connect:https://api.example.com:443"]
            }
        }
    })";
    
    auto result = nah::parse_manifest_input(json);
    CHECK(result.ok);
    CHECK(result.input.id == "com.example.myapp");
    CHECK(result.input.entrypoint_args.size() == 2);
    CHECK(result.input.entrypoint_args[0] == "--mode");
    CHECK(result.input.entrypoint_args[1] == "production");
    CHECK(result.input.description == "My Application");
    CHECK(result.input.lib_dirs.size() == 2);
    CHECK(result.input.asset_dirs.size() == 1);
    CHECK(result.input.exports.size() == 2);
    CHECK(result.input.exports[0].id == "config");
    CHECK(result.input.exports[0].type == "application/json");
    CHECK(result.input.exports[1].id == "splash");
    CHECK(result.input.exports[1].type.empty());
    CHECK(result.input.environment.size() == 2);
    CHECK(result.input.permissions_filesystem.size() == 1);
    CHECK(result.input.permissions_network.size() == 1);
}

TEST_CASE("parse_manifest_input: missing required field fails") {
    // Missing id
    const char* json1 = R"({
        "app": {
            "version": "1.0.0",
            "nak_id": "com.example.runtime",
            "nak_version_req": ">=2.0.0",
            "entrypoint": "bundle.js"
        }
    })";
    auto r1 = nah::parse_manifest_input(json1);
    CHECK_FALSE(r1.ok);
    CHECK(r1.error.find("id") != std::string::npos);
    
    // Missing entrypoint
    const char* json2 = R"({
        "app": {
            "id": "com.example.myapp",
            "version": "1.0.0",
            "nak_id": "com.example.runtime",
            "nak_version_req": ">=2.0.0"
        }
    })";
    auto r2 = nah::parse_manifest_input(json2);
    CHECK_FALSE(r2.ok);
    CHECK(r2.error.find("entrypoint") != std::string::npos);
}

TEST_CASE("parse_manifest_input: absolute entrypoint path fails") {
    const char* json = R"({
        "app": {
            "id": "com.example.myapp",
            "version": "1.0.0",
            "nak_id": "com.example.runtime",
            "nak_version_req": ">=2.0.0",
            "entrypoint": "/absolute/path/bundle.js"
        }
    })";
    
    auto result = nah::parse_manifest_input(json);
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("relative") != std::string::npos);
}

TEST_CASE("parse_manifest_input: path traversal in entrypoint fails") {
    const char* json = R"({
        "app": {
            "id": "com.example.myapp",
            "version": "1.0.0",
            "nak_id": "com.example.runtime",
            "nak_version_req": ">=2.0.0",
            "entrypoint": "../escape/bundle.js"
        }
    })";
    
    auto result = nah::parse_manifest_input(json);
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("..") != std::string::npos);
}

TEST_CASE("parse_manifest_input: absolute lib_dir fails") {
    const char* json = R"({
        "app": {
            "id": "com.example.myapp",
            "version": "1.0.0",
            "nak_id": "com.example.runtime",
            "nak_version_req": ">=2.0.0",
            "entrypoint": "bundle.js",
            "lib_dirs": ["/absolute/lib"]
        }
    })";
    
    auto result = nah::parse_manifest_input(json);
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("relative") != std::string::npos);
}

TEST_CASE("parse_manifest_input: invalid filesystem permission format fails") {
    const char* json = R"({
        "app": {
            "id": "com.example.myapp",
            "version": "1.0.0",
            "nak_id": "com.example.runtime",
            "nak_version_req": ">=2.0.0",
            "entrypoint": "bundle.js",
            "permissions": {
                "filesystem": ["invalid-no-colon"]
            }
        }
    })";
    
    auto result = nah::parse_manifest_input(json);
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("permission") != std::string::npos);
}

TEST_CASE("parse_manifest_input: invalid filesystem operation fails") {
    const char* json = R"({
        "app": {
            "id": "com.example.myapp",
            "version": "1.0.0",
            "nak_id": "com.example.runtime",
            "nak_version_req": ">=2.0.0",
            "entrypoint": "bundle.js",
            "permissions": {
                "filesystem": ["delete:app://files/*"]
            }
        }
    })";
    
    auto result = nah::parse_manifest_input(json);
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("operation") != std::string::npos);
}

TEST_CASE("parse_manifest_input: invalid network operation fails") {
    const char* json = R"({
        "app": {
            "id": "com.example.myapp",
            "version": "1.0.0",
            "nak_id": "com.example.runtime",
            "nak_version_req": ">=2.0.0",
            "entrypoint": "bundle.js",
            "permissions": {
                "network": ["broadcast:udp://0.0.0.0:1234"]
            }
        }
    })";
    
    auto result = nah::parse_manifest_input(json);
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("operation") != std::string::npos);
}

TEST_CASE("parse_manifest_input: invalid JSON syntax fails") {
    const char* json = R"({
        "app": {
            "id": "broken
    })";
    
    auto result = nah::parse_manifest_input(json);
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("parse") != std::string::npos);
}

// ============================================================================
// Tests: TLV Generation from Input
// ============================================================================

TEST_CASE("build_manifest_from_input: produces valid TLV") {
    nah::ManifestInput input;
    input.id = "com.example.myapp";
    input.version = "1.0.0";
    input.nak_id = "com.example.runtime";
    input.nak_version_req = ">=2.0.0";
    input.entrypoint = "bundle.js";
    
    auto manifest_bytes = nah::build_manifest_from_input(input);
    
    // Should have valid header
    CHECK(manifest_bytes.size() >= 16);
    
    // Check magic
    uint32_t magic = static_cast<uint32_t>(manifest_bytes[0]) | 
                     (static_cast<uint32_t>(manifest_bytes[1]) << 8) | 
                     (static_cast<uint32_t>(manifest_bytes[2]) << 16) | 
                     (static_cast<uint32_t>(manifest_bytes[3]) << 24);
    CHECK(magic == 0x4D48414E);  // "NAHM"
    
    // Parse and verify
    auto parsed = nah::parse_manifest(manifest_bytes);
    CHECK(parsed.ok);
    CHECK(parsed.manifest.id == "com.example.myapp");
    CHECK(parsed.manifest.version == "1.0.0");
    CHECK(parsed.manifest.nak_id == "com.example.runtime");
    CHECK(parsed.manifest.nak_version_req.has_value());
    CHECK(parsed.manifest.entrypoint_path == "bundle.js");
}

TEST_CASE("build_manifest_from_input: includes all optional fields") {
    nah::ManifestInput input;
    input.id = "com.example.myapp";
    input.version = "1.0.0";
    input.nak_id = "com.example.runtime";
    input.nak_version_req = ">=2.0.0";
    input.entrypoint = "bundle.js";
    input.entrypoint_args = {"--mode", "production"};
    input.description = "Test App";
    input.author = "Developer";
    input.lib_dirs = {"lib"};
    input.environment["NODE_ENV"] = "production";
    input.permissions_filesystem = {"read:app://assets/*"};
    input.permissions_network = {"connect:https://api.example.com:443"};
    
    nah::ManifestInput::AssetExport exp;
    exp.id = "config";
    exp.path = "share/config.json";
    exp.type = "application/json";
    input.exports.push_back(exp);
    
    auto manifest_bytes = nah::build_manifest_from_input(input);
    auto parsed = nah::parse_manifest(manifest_bytes);
    REQUIRE(parsed.ok);
    
    CHECK(parsed.manifest.id == "com.example.myapp");
    CHECK(parsed.manifest.entrypoint_args.size() == 2);
    CHECK(parsed.manifest.description == "Test App");
    CHECK(parsed.manifest.author == "Developer");
    CHECK(parsed.manifest.lib_dirs.size() == 1);
    CHECK(parsed.manifest.lib_dirs[0] == "lib");
    CHECK(parsed.manifest.env_vars.size() == 1);
    CHECK(parsed.manifest.permissions_filesystem.size() == 1);
    CHECK(parsed.manifest.permissions_network.size() == 1);
    CHECK(parsed.manifest.asset_exports.size() == 1);
}

// ============================================================================
// Tests: End-to-End (JSON -> TLV -> Parse)
// ============================================================================

TEST_CASE("end-to-end: JSON input to parsed manifest") {
    const char* json = R"({
        "app": {
            "id": "com.example.bundle-app",
            "version": "2.0.0",
            "nak_id": "com.mycompany.rn-runtime",
            "nak_version_req": ">=3.0.0 <4.0.0",
            "entrypoint": "dist/bundle.js",
            "entrypoint_args": ["--config", "prod.json"],
            "description": "A bundle application",
            "exports": [
                {
                    "id": "splash",
                    "path": "assets/splash.png",
                    "type": "image/png"
                }
            ],
            "environment": {
                "NODE_ENV": "production"
            }
        }
    })";
    
    // Parse JSON
    auto parse_result = nah::parse_manifest_input(json);
    REQUIRE(parse_result.ok);
    
    // Build TLV
    auto manifest_bytes = nah::build_manifest_from_input(parse_result.input);
    
    // Parse TLV back
    auto parsed = nah::parse_manifest(manifest_bytes);
    REQUIRE(parsed.ok);
    auto& manifest = parsed.manifest;
    
    CHECK(manifest.id == "com.example.bundle-app");
    CHECK(manifest.version == "2.0.0");
    CHECK(manifest.nak_id == "com.mycompany.rn-runtime");
    CHECK(manifest.nak_version_req.has_value());
    CHECK(manifest.entrypoint_path == "dist/bundle.js");
    CHECK(manifest.entrypoint_args.size() == 2);
    CHECK(manifest.description == "A bundle application");
    CHECK(manifest.asset_exports.size() == 1);
    CHECK(manifest.asset_exports[0].id == "splash");
    CHECK(manifest.env_vars.size() == 1);
}

TEST_CASE("end-to-end: bundle app with no permissions") {
    // This is the typical case for bundle apps - no permissions declared
    // because the NAK runtime is the sandbox
    const char* json = R"({
        "app": {
            "id": "com.example.my-rn-app",
            "version": "1.0.0",
            "nak_id": "com.mycompany.rn-runtime",
            "nak_version_req": ">=2.0.0",
            "entrypoint": "bundle.js"
        }
    })";
    
    auto parse_result = nah::parse_manifest_input(json);
    REQUIRE(parse_result.ok);
    
    auto manifest_bytes = nah::build_manifest_from_input(parse_result.input);
    auto parsed = nah::parse_manifest(manifest_bytes);
    REQUIRE(parsed.ok);
    auto& manifest = parsed.manifest;
    
    CHECK(manifest.id == "com.example.my-rn-app");
    CHECK(manifest.permissions_filesystem.empty());
    CHECK(manifest.permissions_network.empty());
}
