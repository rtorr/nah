#include <doctest/doctest.h>
#include <nah/nak_record.hpp>

using nah::NakInstallRecord;
using nah::parse_nak_install_record;

TEST_CASE("nak install record valid schema and required fields") {
    const char* json = R"({
        "$schema": "nah.nak.install.v2",
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
    CHECK(rec.schema == "nah.nak.install.v2");
    CHECK(rec.nak.id == "com.example.nak");
    CHECK(rec.nak.version == "3.1.2");
    CHECK(rec.paths.root == "/nah/naks/com.example.nak/3.1.2");
}

TEST_CASE("nak install record missing schema invalid") {
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
    CHECK_FALSE(v.ok);
}

TEST_CASE("nak install record schema mismatch invalid") {
    const char* json = R"({
        "$schema": "nah.nak.install.v1",
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
    CHECK_FALSE(v.ok);
}

TEST_CASE("nak install record missing required fields invalid") {
    const char* json = R"({
        "$schema": "nah.nak.install.v2",
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
        "$schema": "nah.nak.install.v2",
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
        "$schema": "nah.nak.install.v2",
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
        "$schema": "nah.nak.install.v2",
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

TEST_CASE("nak install record loader section is optional") {
    // Per SPEC L436-446: loader is OPTIONAL
    const char* json = R"({
        "$schema": "nah.nak.install.v2",
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
    CHECK_FALSE(rec.loader.present);
}

TEST_CASE("nak install record with loader section parses correctly") {
    // Per SPEC L436-446: loader section format
    const char* json = R"({
        "$schema": "nah.nak.install.v2",
        "nak": {
            "id": "com.example.nak",
            "version": "3.1.2"
        },
        "paths": {
            "root": "/nah/naks/com.example.nak/3.1.2"
        },
        "loader": {
            "exec_path": "/nah/naks/com.example.nak/3.1.2/bin/loader",
            "args_template": ["${NAH_APP_ENTRY}", "--runtime"]
        }
    })";
    NakInstallRecord rec;
    auto v = parse_nak_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.loader.present);
    CHECK(rec.loader.exec_path == "/nah/naks/com.example.nak/3.1.2/bin/loader");
    CHECK(rec.loader.args_template.size() == 2);
}

TEST_CASE("nak install record execution section is optional") {
    // Per SPEC L446-449: execution is OPTIONAL
    const char* json = R"({
        "$schema": "nah.nak.install.v2",
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
        "$schema": "nah.nak.install.v2",
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
        "$schema": "nah.nak.install.v2",
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
        "$schema": "nah.nak.install.v2",
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
