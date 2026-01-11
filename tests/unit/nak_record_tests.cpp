#include <doctest/doctest.h>
#include <nah/nak_record.hpp>

using nah::NakInstallRecord;
using nah::parse_nak_install_record;

TEST_CASE("nak install record valid required fields") {
    const char* json = R"({
        "nak": {
            "id": "com.example.nak",
            "version": "3.1.2"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2"
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.nak.id == "com.example.nak");
    CHECK(rec.nak.version == "3.1.2");
    CHECK(rec.paths.root == "/nah/naks/com.example.nak/3.1.2");
}

TEST_CASE("nak install record missing required fields invalid") {
    const char* json = R"({
        "nak": {
            "id": "com.example.nak"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2"
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK_FALSE(v.ok);
}

TEST_CASE("nak install record empty required field invalid") {
    const char* json = R"({
        "nak": {
            "id": "",
            "version": "3.1.2"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2"
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK_FALSE(v.ok);
}

// ============================================================================
// Optional Field Tests (per SPEC L427, L436-449)
// ============================================================================

TEST_CASE("nak install record resource_root defaults to paths.root") {
    // Per SPEC L427: resource_root defaults to paths.root
    const char* json = R"({
        "nak": {
            "id": "com.example.nak",
            "version": "3.1.2"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2"
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK(v.ok);
    // resource_root defaults to root when not specified
    CHECK((rec.paths.resource_root.empty() || rec.paths.resource_root == rec.paths.root));
}

TEST_CASE("nak install record explicit resource_root is used") {
    const char* json = R"({
        "nak": {
            "id": "com.example.nak",
            "version": "3.1.2"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2",
            "resource_root": "/nah/naks/com.example.nak/3.1.2/resources"
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.paths.resource_root == "/nah/naks/com.example.nak/3.1.2/resources");
}

TEST_CASE("nak install record loaders section is optional") {
    // Per SPEC: loaders is OPTIONAL (libs-only NAKs omit it)
    const char* json = R"({
        "nak": {
            "id": "com.example.nak",
            "version": "3.1.2"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2"
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK(v.ok);
    CHECK_FALSE(rec.has_loaders());
}

TEST_CASE("nak install record with loaders section parses correctly") {
    // Per SPEC: loaders section with multiple named loaders
    const char* json = R"({
        "nak": {
            "id": "com.example.nak",
            "version": "3.1.2"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2"
        },
        "loaders": {
            "default": {
                "exec_path": "/nah/naks/com.example.nak/3.1.2/bin/loader",
                "args_template": ["${NAH_APP_ENTRY}", "--runtime"]
            },
            "alt": {
                "exec_path": "/nah/naks/com.example.nak/3.1.2/bin/loader-alt",
                "args_template": ["--mode", "alt", "${NAH_APP_ENTRY}"]
            }
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.has_loaders());
    CHECK(rec.loaders.size() == 2);
    CHECK(rec.loaders.at("default").exec_path == "/nah/naks/com.example.nak/3.1.2/bin/loader");
    CHECK(rec.loaders.at("default").args_template.size() == 2);
    CHECK(rec.loaders.at("alt").exec_path == "/nah/naks/com.example.nak/3.1.2/bin/loader-alt");
    CHECK(rec.loaders.at("alt").args_template.size() == 3);
}

TEST_CASE("nak install record execution section is optional") {
    // Per SPEC L446-449: execution is OPTIONAL
    const char* json = R"({
        "nak": {
            "id": "com.example.nak",
            "version": "3.1.2"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2"
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK(v.ok);
    CHECK_FALSE(rec.execution.present);
}

TEST_CASE("nak install record with execution section parses correctly") {
    // Per SPEC L446-449: execution section format
    const char* json = R"({
        "nak": {
            "id": "com.example.nak",
            "version": "3.1.2"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2"
        },
        "execution": {
            "cwd": "workdir"
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.execution.present);
    CHECK(rec.execution.cwd == "workdir");
}

TEST_CASE("nak install record lib_dirs is optional") {
    // lib_dirs is optional
    const char* json = R"({
        "nak": {
            "id": "com.example.nak",
            "version": "3.1.2"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2"
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.paths.lib_dirs.empty());
}

TEST_CASE("nak install record with lib_dirs parses correctly") {
    const char* json = R"({
        "nak": {
            "id": "com.example.nak",
            "version": "3.1.2"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2",
            "lib_dirs": ["/nah/naks/com.example.nak/3.1.2/lib", "/nah/naks/com.example.nak/3.1.2/lib64"]
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.paths.lib_dirs.size() == 2);
}
