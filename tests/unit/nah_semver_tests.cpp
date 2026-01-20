/**
 * Unit tests for nah_semver.h
 */

#include <nah/nah_semver.h>
#include <doctest/doctest.h>
#include <unordered_map>

using namespace nah::semver;

TEST_CASE("parse_version") {
    SUBCASE("basic versions") {
        auto v = parse_version("1.2.3");
        REQUIRE(v.has_value());
        CHECK(v->major() == 1);
        CHECK(v->minor() == 2);
        CHECK(v->patch() == 3);
    }

    SUBCASE("version with prerelease") {
        auto v = parse_version("1.0.0-alpha.1");
        REQUIRE(v.has_value());
        CHECK(v->major() == 1);
        CHECK(v->minor() == 0);
        CHECK(v->patch() == 0);
    }

    SUBCASE("version with build metadata") {
        auto v = parse_version("1.0.0+build.123");
        REQUIRE(v.has_value());
        CHECK(v->major() == 1);
    }

    SUBCASE("invalid versions") {
        CHECK(!parse_version("").has_value());
        CHECK(!parse_version("abc").has_value());
        CHECK(!parse_version("1").has_value());
        CHECK(!parse_version("1.2").has_value());
    }
}

TEST_CASE("parse_range - basic comparators") {
    SUBCASE("greater than or equal") {
        auto range = parse_range(">=1.0.0");
        REQUIRE(range.has_value());
        
        CHECK(satisfies(*parse_version("1.0.0"), *range));
        CHECK(satisfies(*parse_version("1.0.1"), *range));
        CHECK(satisfies(*parse_version("2.0.0"), *range));
        CHECK(!satisfies(*parse_version("0.9.9"), *range));
    }

    SUBCASE("less than") {
        auto range = parse_range("<2.0.0");
        REQUIRE(range.has_value());
        
        CHECK(satisfies(*parse_version("1.0.0"), *range));
        CHECK(satisfies(*parse_version("1.9.9"), *range));
        CHECK(!satisfies(*parse_version("2.0.0"), *range));
        CHECK(!satisfies(*parse_version("2.0.1"), *range));
    }

    SUBCASE("exact match") {
        auto range = parse_range("=1.2.3");
        REQUIRE(range.has_value());
        
        CHECK(satisfies(*parse_version("1.2.3"), *range));
        CHECK(!satisfies(*parse_version("1.2.4"), *range));
        CHECK(!satisfies(*parse_version("1.2.2"), *range));
    }

    SUBCASE("exact match without operator") {
        auto range = parse_range("1.2.3");
        REQUIRE(range.has_value());
        
        CHECK(satisfies(*parse_version("1.2.3"), *range));
        CHECK(!satisfies(*parse_version("1.2.4"), *range));
    }
}

TEST_CASE("parse_range - compound ranges") {
    SUBCASE("AND (space-separated)") {
        auto range = parse_range(">=1.0.0 <2.0.0");
        REQUIRE(range.has_value());
        
        CHECK(!satisfies(*parse_version("0.9.9"), *range));
        CHECK(satisfies(*parse_version("1.0.0"), *range));
        CHECK(satisfies(*parse_version("1.5.0"), *range));
        CHECK(satisfies(*parse_version("1.9.9"), *range));
        CHECK(!satisfies(*parse_version("2.0.0"), *range));
    }

    SUBCASE("OR (|| separated)") {
        auto range = parse_range(">=1.0.0 <2.0.0 || >=3.0.0 <4.0.0");
        REQUIRE(range.has_value());
        
        CHECK(satisfies(*parse_version("1.5.0"), *range));
        CHECK(!satisfies(*parse_version("2.5.0"), *range));
        CHECK(satisfies(*parse_version("3.5.0"), *range));
        CHECK(!satisfies(*parse_version("4.5.0"), *range));
    }
}

TEST_CASE("parse_range - caret ranges") {
    SUBCASE("^1.2.3 means >=1.2.3 <2.0.0") {
        auto range = parse_range("^1.2.3");
        REQUIRE(range.has_value());
        
        CHECK(!satisfies(*parse_version("1.2.2"), *range));
        CHECK(satisfies(*parse_version("1.2.3"), *range));
        CHECK(satisfies(*parse_version("1.9.9"), *range));
        CHECK(!satisfies(*parse_version("2.0.0"), *range));
    }

    SUBCASE("^0.2.3 means >=0.2.3 <0.3.0") {
        auto range = parse_range("^0.2.3");
        REQUIRE(range.has_value());
        
        CHECK(!satisfies(*parse_version("0.2.2"), *range));
        CHECK(satisfies(*parse_version("0.2.3"), *range));
        CHECK(satisfies(*parse_version("0.2.9"), *range));
        CHECK(!satisfies(*parse_version("0.3.0"), *range));
    }

    SUBCASE("^0.0.3 means exactly 0.0.3") {
        auto range = parse_range("^0.0.3");
        REQUIRE(range.has_value());
        
        CHECK(!satisfies(*parse_version("0.0.2"), *range));
        CHECK(satisfies(*parse_version("0.0.3"), *range));
        CHECK(!satisfies(*parse_version("0.0.4"), *range));
    }
}

TEST_CASE("parse_range - tilde ranges") {
    SUBCASE("~1.2.3 means >=1.2.3 <1.3.0") {
        auto range = parse_range("~1.2.3");
        REQUIRE(range.has_value());
        
        CHECK(!satisfies(*parse_version("1.2.2"), *range));
        CHECK(satisfies(*parse_version("1.2.3"), *range));
        CHECK(satisfies(*parse_version("1.2.9"), *range));
        CHECK(!satisfies(*parse_version("1.3.0"), *range));
    }
}

TEST_CASE("parse_range - X ranges") {
    SUBCASE("1.x means >=1.0.0 <2.0.0") {
        auto range = parse_range("1.x");
        REQUIRE(range.has_value());
        
        CHECK(!satisfies(*parse_version("0.9.9"), *range));
        CHECK(satisfies(*parse_version("1.0.0"), *range));
        CHECK(satisfies(*parse_version("1.9.9"), *range));
        CHECK(!satisfies(*parse_version("2.0.0"), *range));
    }

    SUBCASE("1.2.x means >=1.2.0 <1.3.0") {
        auto range = parse_range("1.2.x");
        REQUIRE(range.has_value());
        
        CHECK(!satisfies(*parse_version("1.1.9"), *range));
        CHECK(satisfies(*parse_version("1.2.0"), *range));
        CHECK(satisfies(*parse_version("1.2.9"), *range));
        CHECK(!satisfies(*parse_version("1.3.0"), *range));
    }

    SUBCASE("* means any version") {
        auto range = parse_range("*");
        REQUIRE(range.has_value());
        
        CHECK(satisfies(*parse_version("0.0.1"), *range));
        CHECK(satisfies(*parse_version("1.0.0"), *range));
        CHECK(satisfies(*parse_version("999.999.999"), *range));
    }
}

TEST_CASE("select_best") {
    std::vector<Version> versions;
    versions.push_back(*parse_version("1.0.0"));
    versions.push_back(*parse_version("1.2.0"));
    versions.push_back(*parse_version("1.5.0"));
    versions.push_back(*parse_version("2.0.0"));
    versions.push_back(*parse_version("2.1.0"));

    SUBCASE("selects highest matching") {
        auto range = parse_range(">=1.0.0 <2.0.0");
        REQUIRE(range.has_value());
        
        auto best = select_best(versions, *range);
        REQUIRE(best.has_value());
        CHECK(best->major() == 1);
        CHECK(best->minor() == 5);
        CHECK(best->patch() == 0);
    }

    SUBCASE("returns nullopt when no match") {
        auto range = parse_range(">=3.0.0");
        REQUIRE(range.has_value());
        
        auto best = select_best(versions, *range);
        CHECK(!best.has_value());
    }
}

TEST_CASE("VersionRange::min_version") {
    SUBCASE("simple range") {
        auto range = parse_range(">=1.2.3");
        REQUIRE(range.has_value());
        
        auto min = range->min_version();
        REQUIRE(min.has_value());
        CHECK(min->major() == 1);
        CHECK(min->minor() == 2);
        CHECK(min->patch() == 3);
    }

    SUBCASE("compound range uses lowest") {
        auto range = parse_range(">=2.0.0 || >=1.0.0");
        REQUIRE(range.has_value());
        
        auto min = range->min_version();
        REQUIRE(min.has_value());
        CHECK(min->major() == 1);
    }
}

TEST_CASE("VersionRange::selection_key") {
    auto range = parse_range(">=1.2.3");
    REQUIRE(range.has_value());
    
    CHECK(range->selection_key() == "1.2");
}

// Test with a mock runtime inventory structure
struct MockRuntime {
    struct { std::string id; std::string version; } nak;
};

TEST_CASE("select_nak_from_inventory") {
    std::unordered_map<std::string, MockRuntime> inventory;
    inventory["lua@5.3.0.json"] = {{"lua", "5.3.0"}};
    inventory["lua@5.4.0.json"] = {{"lua", "5.4.0"}};
    inventory["lua@5.4.6.json"] = {{"lua", "5.4.6"}};
    inventory["node@18.0.0.json"] = {{"node", "18.0.0"}};
    inventory["node@20.0.0.json"] = {{"node", "20.0.0"}};

    SUBCASE("selects highest matching version") {
        auto result = select_nak_from_inventory(inventory, "lua", ">=5.4.0");
        
        REQUIRE(result.found);
        CHECK(result.nak_id == "lua");
        CHECK(result.nak_version == "5.4.6");
        CHECK(result.record_ref == "lua@5.4.6.json");
        CHECK(result.candidates.size() == 2); // 5.4.0 and 5.4.6
    }

    SUBCASE("respects version constraint") {
        auto result = select_nak_from_inventory(inventory, "lua", ">=5.3.0 <5.4.0");
        
        REQUIRE(result.found);
        CHECK(result.nak_version == "5.3.0");
    }

    SUBCASE("returns error for no match") {
        auto result = select_nak_from_inventory(inventory, "lua", ">=6.0.0");
        
        CHECK(!result.found);
        CHECK(!result.error.empty());
    }

    SUBCASE("returns error for unknown NAK") {
        auto result = select_nak_from_inventory(inventory, "python", ">=3.0.0");
        
        CHECK(!result.found);
        CHECK(!result.error.empty());
    }

    SUBCASE("handles caret range") {
        auto result = select_nak_from_inventory(inventory, "node", "^18.0.0");
        
        REQUIRE(result.found);
        CHECK(result.nak_version == "18.0.0");
    }
}
