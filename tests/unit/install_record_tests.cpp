#include <doctest/doctest.h>
#include <nah/install_record.hpp>

using nah::parse_app_install_record;
using nah::AppInstallRecord;

TEST_CASE("app install record valid required fields") {
    const char* json = R"({
        "install": {
            "instance_id": "uuid-123"
        },
        "paths": {
            "install_root": "/nah/apps/app-1.0"
        }
    })";
    AppInstallRecord rec;
    auto v = parse_app_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.install.instance_id == "uuid-123");
    CHECK(rec.paths.install_root == "/nah/apps/app-1.0");
}

TEST_CASE("app install record missing required fields invalid") {
    const char* json = R"({
        "install": {},
        "paths": {
            "install_root": "/nah/apps/app-1.0"
        }
    })";
    AppInstallRecord rec;
    auto v = parse_app_install_record(json, rec);
    CHECK_FALSE(v.ok);
}

TEST_CASE("app install record empty required field invalid") {
    const char* json = R"({
        "install": {
            "instance_id": ""
        },
        "paths": {
            "install_root": "/nah/apps/app-1.0"
        }
    })";
    AppInstallRecord rec;
    auto v = parse_app_install_record(json, rec);
    CHECK_FALSE(v.ok);
}

// ============================================================================
// Optional Field Tests (per SPEC L377)
// ============================================================================

TEST_CASE("app install record nak.record_ref MAY be absent") {
    // Per SPEC L377: nak.record_ref MAY be absent
    const char* json = R"({
        "install": {
            "instance_id": "uuid-123"
        },
        "paths": {
            "install_root": "/nah/apps/app-1.0"
        }
    })";
    AppInstallRecord rec;
    auto v = parse_app_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.nak.record_ref.empty());
}

TEST_CASE("app install record with nak section but no record_ref is valid") {
    const char* json = R"({
        "install": {
            "instance_id": "uuid-123"
        },
        "paths": {
            "install_root": "/nah/apps/app-1.0"
        },
        "nak": {
            "id": "com.example.nak",
            "version": "3.0.0"
        }
    })";
    AppInstallRecord rec;
    auto v = parse_app_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.nak.id == "com.example.nak");
    CHECK(rec.nak.version == "3.0.0");
    CHECK(rec.nak.record_ref.empty());
}

TEST_CASE("app install record with full nak section parses correctly") {
    const char* json = R"({
        "install": {
            "instance_id": "uuid-123"
        },
        "paths": {
            "install_root": "/nah/apps/app-1.0"
        },
        "nak": {
            "id": "com.example.nak",
            "version": "3.0.0",
            "record_ref": "com.example.nak@3.0.0.json"
        }
    })";
    AppInstallRecord rec;
    auto v = parse_app_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.nak.id == "com.example.nak");
    CHECK(rec.nak.version == "3.0.0");
    CHECK(rec.nak.record_ref == "com.example.nak@3.0.0.json");
}

TEST_CASE("app install record trust section is optional") {
    const char* json = R"({
        "install": {
            "instance_id": "uuid-123"
        },
        "paths": {
            "install_root": "/nah/apps/app-1.0"
        }
    })";
    AppInstallRecord rec;
    auto v = parse_app_install_record(json, rec);
    CHECK(v.ok);
    // Trust defaults to Unknown when section is absent
}

TEST_CASE("app install record with trust section parses correctly") {
    const char* json = R"({
        "install": {
            "instance_id": "uuid-123"
        },
        "paths": {
            "install_root": "/nah/apps/app-1.0"
        },
        "trust": {
            "state": "verified",
            "source": "test-host",
            "evaluated_at": "2025-01-01T00:00:00Z"
        }
    })";
    AppInstallRecord rec;
    auto v = parse_app_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.trust.source == "test-host");
}

TEST_CASE("app install record overrides section is optional") {
    const char* json = R"({
        "install": {
            "instance_id": "uuid-123"
        },
        "paths": {
            "install_root": "/nah/apps/app-1.0"
        }
    })";
    AppInstallRecord rec;
    auto v = parse_app_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.overrides.environment.empty());
}

TEST_CASE("app install record with overrides section parses correctly") {
    const char* json = R"({
        "install": {
            "instance_id": "uuid-123"
        },
        "paths": {
            "install_root": "/nah/apps/app-1.0"
        },
        "overrides": {
            "environment": {
                "MY_VAR": "my_value"
            }
        }
    })";
    AppInstallRecord rec;
    auto v = parse_app_install_record(json, rec);
    CHECK(v.ok);
    CHECK(rec.overrides.environment.size() == 1);
    CHECK(rec.overrides.environment.at("MY_VAR") == "my_value");
}
