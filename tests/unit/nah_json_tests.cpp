/**
 * Unit tests for nah_json.h parsing functions
 */

#define NAH_JSON_IMPLEMENTATION
#include <nah/nah_json.h>
#include <doctest/doctest.h>
#include <sstream>

TEST_CASE("parse_app_declaration") {
    SUBCASE("valid minimal app declaration") {
        std::string json = R"({
            "id": "com.test.app",
            "version": "1.0.0",
            "entrypoint": "bin/app"
        })";

        auto result = nah::json::parse_app_declaration(json);
        REQUIRE(result.ok);
        CHECK(result.value.id == "com.test.app");
        CHECK(result.value.version == "1.0.0");
        CHECK(result.value.entrypoint_path == "bin/app");
    }

    SUBCASE("app with NAK requirement") {
        std::string json = R"({
            "id": "com.test.app",
            "version": "1.0.0",
            "entrypoint": "bin/app",
            "nak": {
                "id": "com.test.runtime",
                "version_req": ">=1.0.0 <2.0.0"
            }
        })";

        auto result = nah::json::parse_app_declaration(json);
        REQUIRE(result.ok);
        CHECK(result.value.nak_id == "com.test.runtime");
        CHECK(result.value.nak_version_req == ">=1.0.0 <2.0.0");
    }

    SUBCASE("app with environment variables") {
        std::string json = R"({
            "id": "com.test.app",
            "version": "1.0.0",
            "entrypoint": "bin/app",
            "env_vars": ["PATH_VAR=/some/path", "CONFIG=value"]
        })";

        auto result = nah::json::parse_app_declaration(json);
        REQUIRE(result.ok);
        REQUIRE(result.value.env_vars.size() == 2);
        CHECK(result.value.env_vars[0] == "PATH_VAR=/some/path");
        CHECK(result.value.env_vars[1] == "CONFIG=value");
    }

    SUBCASE("app with library directories") {
        std::string json = R"({
            "id": "com.test.app",
            "version": "1.0.0",
            "entrypoint": "bin/app",
            "lib_dirs": ["lib", "lib64", "vendor/lib"]
        })";

        auto result = nah::json::parse_app_declaration(json);
        REQUIRE(result.ok);
        REQUIRE(result.value.lib_dirs.size() == 3);
        CHECK(result.value.lib_dirs[0] == "lib");
        CHECK(result.value.lib_dirs[1] == "lib64");
        CHECK(result.value.lib_dirs[2] == "vendor/lib");
    }

    SUBCASE("app with permissions") {
        std::string json = R"({
            "id": "com.test.app",
            "version": "1.0.0",
            "entrypoint": "bin/app",
            "permissions": {
                "filesystem": ["read:/data"],
                "network": ["connect:*"]
            }
        })";

        auto result = nah::json::parse_app_declaration(json);
        REQUIRE(result.ok);
        REQUIRE(result.value.permissions_filesystem.size() == 1);
        CHECK(result.value.permissions_filesystem[0] == "read:/data");
        REQUIRE(result.value.permissions_network.size() == 1);
        CHECK(result.value.permissions_network[0] == "connect:*");
    }

    SUBCASE("app with metadata fields") {
        std::string json = R"({
            "id": "com.test.app",
            "version": "1.0.0",
            "entrypoint": "bin/app",
            "metadata": {
                "description": "Test application",
                "author": "Test Author",
                "license": "MIT",
                "homepage": "https://example.com"
            }
        })";

        auto result = nah::json::parse_app_declaration(json);
        REQUIRE(result.ok);
        CHECK(result.value.description == "Test application");
        CHECK(result.value.author == "Test Author");
        CHECK(result.value.license == "MIT");
        CHECK(result.value.homepage == "https://example.com");
    }

    SUBCASE("app with custom metadata fields") {
        std::string json = R"({
            "app": {
                "identity": {
                    "id": "com.test.app",
                    "version": "1.0.0"
                },
                "execution": {
                    "entrypoint": "bin/app"
                },
                "metadata": {
                    "description": "Test app",
                    "custom_field": "custom_value",
                    "sub_apps": [
                        {"id": "app1", "type": "screen"},
                        {"id": "app2", "type": "service"}
                    ],
                    "capabilities": ["audio", "network"]
                }
            }
        })";

        auto result = nah::json::parse_app_declaration(json);
        REQUIRE(result.ok);
        CHECK(result.value.id == "com.test.app");
        CHECK(result.value.description == "Test app");
    }

    SUBCASE("invalid JSON") {
        std::string json = "not valid json";
        auto result = nah::json::parse_app_declaration(json);
        CHECK(!result.ok);
    }

    SUBCASE("missing required fields") {
        std::string json = R"({
            "id": "com.test.app"
        })";

        auto result = nah::json::parse_app_declaration(json);
        CHECK(!result.ok);
    }
}

TEST_CASE("parse_host_environment") {
    SUBCASE("valid empty host environment") {
        std::string json = "{}";
        auto result = nah::json::parse_host_environment(json);
        REQUIRE(result.ok);
    }

    SUBCASE("host environment with environment variables") {
        std::string json = R"({
            "environment": {
                "CUSTOM_PATH": "/custom/path",
                "DEBUG": "1"
            }
        })";

        auto result = nah::json::parse_host_environment(json);
        REQUIRE(result.ok);
        CHECK(result.value.vars.at("CUSTOM_PATH").value == "/custom/path");
        CHECK(result.value.vars.at("DEBUG").value == "1");
    }

    SUBCASE("host environment with paths") {
        std::string json = R"({
            "paths": {
                "library_prepend": ["/custom/lib1"],
                "library_append": ["/custom/lib2"]
            }
        })";

        auto result = nah::json::parse_host_environment(json);
        REQUIRE(result.ok);
        REQUIRE(result.value.paths.library_prepend.size() == 1);
        CHECK(result.value.paths.library_prepend[0] == "/custom/lib1");
        REQUIRE(result.value.paths.library_append.size() == 1);
        CHECK(result.value.paths.library_append[0] == "/custom/lib2");
    }

    SUBCASE("host environment with override policy") {
        std::string json = R"({
            "overrides": {
                "allow_env_overrides": false,
                "allowed_env_keys": ["DEBUG", "LOG_LEVEL"]
            }
        })";

        auto result = nah::json::parse_host_environment(json);
        REQUIRE(result.ok);
        CHECK(result.value.overrides.allow_env_overrides == false);
        REQUIRE(result.value.overrides.allowed_env_keys.size() == 2);
        CHECK(result.value.overrides.allowed_env_keys[0] == "DEBUG");
        CHECK(result.value.overrides.allowed_env_keys[1] == "LOG_LEVEL");
    }
}

TEST_CASE("parse_install_record") {
    SUBCASE("valid install record") {
        std::string json = R"({
            "install": {
                "instance_id": "uuid-1234"
            },
            "app": {
                "id": "com.test.app",
                "version": "1.0.0",
                "nak_id": "com.test.runtime",
                "nak_version_req": ">=1.0.0"
            },
            "nak": {
                "id": "com.test.runtime",
                "version": "1.2.0",
                "record_ref": "runtime@1.2.0.json"
            },
            "paths": {
                "install_root": "/apps/test"
            },
            "provenance": {
                "package_hash": "sha256:abc123",
                "installed_at": "2024-01-01T00:00:00Z",
                "installed_by": "nah_cli"
            },
            "trust": {
                "state": "verified",
                "source": "local_install",
                "evaluated_at": "2024-01-01T00:00:00Z"
            }
        })";

        auto result = nah::json::parse_install_record(json);
        REQUIRE(result.ok);
        CHECK(result.value.install.instance_id == "uuid-1234");
        CHECK(result.value.app.id == "com.test.app");
        CHECK(result.value.app.version == "1.0.0");
        CHECK(result.value.nak.id == "com.test.runtime");
        CHECK(result.value.nak.version == "1.2.0");
        CHECK(result.value.paths.install_root == "/apps/test");
        CHECK(result.value.trust.state == nah::core::TrustState::Verified);
    }

    SUBCASE("install record with overrides") {
        std::string json = R"({
            "install": {"instance_id": "uuid-1234"},
            "app": {"id": "com.test.app", "version": "1.0.0"},
            "paths": {"install_root": "/apps/test"},
            "trust": {"state": "unknown"},
            "overrides": {
                "environment": {
                    "DEBUG": "true"
                },
                "arguments": {
                    "prepend": ["--verbose"],
                    "append": ["--quiet"]
                },
                "paths": {
                    "library_prepend": ["/custom/lib"]
                }
            }
        })";

        auto result = nah::json::parse_install_record(json);
        REQUIRE(result.ok);
        CHECK(result.value.overrides.environment.at("DEBUG").value == "true");
        CHECK(result.value.overrides.arguments.prepend.size() == 1);
        CHECK(result.value.overrides.arguments.append.size() == 1);
        CHECK(result.value.overrides.paths.library_prepend.size() == 1);
    }
}

TEST_CASE("parse_runtime_descriptor") {
    SUBCASE("valid runtime descriptor") {
        std::string json = R"({
            "nak": {
                "id": "com.test.runtime",
                "version": "1.2.0"
            },
            "paths": {
                "root": "/naks/runtime"
            },
            "loaders": {
                "default": {
                    "exec_path": "/naks/runtime/bin/runtime",
                    "args_template": ["--exec"]
                }
            }
        })";

        auto result = nah::json::parse_runtime_descriptor(json);
        REQUIRE(result.ok);
        CHECK(result.value.nak.id == "com.test.runtime");
        CHECK(result.value.nak.version == "1.2.0");
        CHECK(result.value.paths.root == "/naks/runtime");
        CHECK(result.value.loaders.size() == 1);
        CHECK(result.value.loaders.at("default").exec_path == "/naks/runtime/bin/runtime");
    }

    SUBCASE("runtime with multiple loaders") {
        std::string json = R"({
            "nak": {
                "id": "com.test.runtime",
                "version": "1.2.0"
            },
            "paths": {
                "root": "/naks/runtime"
            },
            "loaders": {
                "default": {
                    "exec_path": "/naks/runtime/bin/runtime"
                },
                "debug": {
                    "exec_path": "/naks/runtime/bin/runtime-debug",
                    "args_template": ["--debug"]
                }
            }
        })";

        auto result = nah::json::parse_runtime_descriptor(json);
        REQUIRE(result.ok);
        CHECK(result.value.loaders.size() == 2);
        CHECK(result.value.loaders.count("default") == 1);
        CHECK(result.value.loaders.count("debug") == 1);
    }
}

TEST_CASE("parse_launch_contract") {
    SUBCASE("valid launch contract") {
        std::string json = R"({
            "app": {
                "id": "com.test.app",
                "version": "1.0.0",
                "root": "/apps/test",
                "entrypoint": "/apps/test/bin/app"
            },
            "execution": {
                "binary": "/apps/test/bin/app",
                "arguments": ["--config", "test"],
                "cwd": "/apps/test"
            },
            "environment": {
                "PATH": "/usr/bin:/bin",
                "APP_HOME": "/apps/test"
            },
            "trust": {
                "state": "verified"
            }
        })";

        auto result = nah::json::parse_launch_contract(json);
        REQUIRE(result.ok);
        CHECK(result.value.app.id == "com.test.app");
        CHECK(result.value.execution.binary == "/apps/test/bin/app");
        CHECK(result.value.execution.arguments.size() == 2);
        CHECK(result.value.environment.size() == 2);
        CHECK(result.value.trust.state == nah::core::TrustState::Verified);
    }
}

TEST_CASE("JSON error messages") {
    SUBCASE("malformed JSON") {
        std::string json = "{invalid json}";
        auto result = nah::json::parse_app_declaration(json);
        CHECK(!result.ok);
        CHECK(!result.error.empty());
    }

    SUBCASE("wrong type for field") {
        std::string json = R"({
            "id": 123,
            "version": "1.0.0",
            "entrypoint": "bin/app"
        })";  // id should be a string

        auto result = nah::json::parse_app_declaration(json);
        CHECK(!result.ok);
    }
}

// Helper function test removed - parse_error is not part of public API

TEST_CASE("trust state parsing") {
    SUBCASE("parse valid trust states") {
        CHECK(nah::core::parse_trust_state("unknown") == nah::core::TrustState::Unknown);
        CHECK(nah::core::parse_trust_state("verified") == nah::core::TrustState::Verified);
        CHECK(nah::core::parse_trust_state("unverified") == nah::core::TrustState::Unverified);
        CHECK(nah::core::parse_trust_state("failed") == nah::core::TrustState::Failed);
    }

    SUBCASE("parse invalid trust state returns nullopt") {
        CHECK(!nah::core::parse_trust_state("invalid").has_value());
        CHECK(!nah::core::parse_trust_state("").has_value());
    }
}

TEST_CASE("warning key parsing") {
    SUBCASE("parse valid warning keys") {
        CHECK(nah::core::parse_warning_key("invalid_manifest") == nah::core::Warning::invalid_manifest);
        CHECK(nah::core::parse_warning_key("nak_not_found") == nah::core::Warning::nak_not_found);
    }

    SUBCASE("parse invalid warning key returns nullopt") {
        CHECK(!nah::core::parse_warning_key("invalid").has_value());
        CHECK(!nah::core::parse_warning_key("").has_value());
        CHECK(!nah::core::parse_warning_key("not_a_warning").has_value());
    }
}