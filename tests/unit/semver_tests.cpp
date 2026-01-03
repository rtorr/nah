#include <doctest/doctest.h>
#include <nah/semver.hpp>

using nah::SemVer;
using nah::RequirementKind;
using nah::parse_version;
using nah::parse_requirement;
using nah::satisfies;

TEST_CASE("parse core version accepts MAJOR.MINOR.PATCH") {
    auto v = parse_version("1.2.3");
    REQUIRE(v);
    CHECK(v->major == 1);
    CHECK(v->minor == 2);
    CHECK(v->patch == 3);
}

TEST_CASE("parse core version rejects pre-release and build metadata") {
    CHECK_FALSE(parse_version("1.2.3-alpha"));
    CHECK_FALSE(parse_version("1.2.3+build"));
}

TEST_CASE("exact requirement satisfaction") {
    auto req = parse_requirement("1.2.3");
    REQUIRE(req);
    CHECK(req->kind == RequirementKind::Exact);
    CHECK(satisfies(SemVer{1,2,3}, *req));
    CHECK_FALSE(satisfies(SemVer{1,2,4}, *req));
}

TEST_CASE("caret requirement major > 0") {
    auto req = parse_requirement("^1.2.3");
    REQUIRE(req);
    CHECK(req->kind == RequirementKind::Caret);
    CHECK(req->lower == SemVer{1,2,3});
    CHECK(req->upper == SemVer{2,0,0});
    CHECK(req->selection_key == "1.2");
    CHECK(satisfies(SemVer{1,2,3}, *req));
    CHECK(satisfies(SemVer{1,9,9}, *req));
    CHECK_FALSE(satisfies(SemVer{2,0,0}, *req));
}

TEST_CASE("caret requirement major == 0 minor > 0") {
    auto req = parse_requirement("^0.2.3");
    REQUIRE(req);
    CHECK(req->upper == SemVer{0,3,0});
    CHECK(satisfies(SemVer{0,2,3}, *req));
    CHECK(satisfies(SemVer{0,2,9}, *req));
    CHECK_FALSE(satisfies(SemVer{0,3,0}, *req));
}

TEST_CASE("caret requirement major == 0 minor == 0") {
    auto req = parse_requirement("^0.0.5");
    REQUIRE(req);
    CHECK(req->upper == SemVer{0,0,6});
    CHECK(satisfies(SemVer{0,0,5}, *req));
    CHECK_FALSE(satisfies(SemVer{0,0,6}, *req));
}

TEST_CASE("tilde requirement") {
    auto req = parse_requirement("~1.4.2");
    REQUIRE(req);
    CHECK(req->kind == RequirementKind::Tilde);
    CHECK(req->upper == SemVer{1,5,0});
    CHECK(satisfies(SemVer{1,4,2}, *req));
    CHECK(satisfies(SemVer{1,4,9}, *req));
    CHECK_FALSE(satisfies(SemVer{1,5,0}, *req));
}

TEST_CASE("wildcard requirement") {
    auto req = parse_requirement("1.7.*");
    REQUIRE(req);
    CHECK(req->kind == RequirementKind::Wildcard);
    CHECK(req->selection_key == "1.7");
    CHECK(satisfies(SemVer{1,7,0}, *req));
    CHECK(satisfies(SemVer{1,7,5}, *req));
    CHECK_FALSE(satisfies(SemVer{1,8,0}, *req));
}

TEST_CASE("bounded requirement") {
    auto req = parse_requirement(">=1.2.3 <2.0.0");
    REQUIRE(req);
    CHECK(req->kind == RequirementKind::Bounded);
    CHECK(satisfies(SemVer{1,2,3}, *req));
    CHECK(satisfies(SemVer{1,9,9}, *req));
    CHECK_FALSE(satisfies(SemVer{2,0,0}, *req));
}

TEST_CASE("invalid requirement strings are rejected") {
    CHECK_FALSE(parse_requirement(""));
    CHECK_FALSE(parse_requirement("foo"));
    CHECK_FALSE(parse_requirement("^1.2"));
    CHECK_FALSE(parse_requirement(">=1.2.3"));
    CHECK_FALSE(parse_requirement("1.2.*.3"));
}

// ============================================================================
// Whitespace Trimming Tests (per SPEC L258)
// ============================================================================

TEST_CASE("version parsing trims whitespace") {
    // Per SPEC L258: Whitespace trimming
    auto v1 = parse_version("  1.2.3  ");
    REQUIRE(v1);
    CHECK(v1->major == 1);
    CHECK(v1->minor == 2);
    CHECK(v1->patch == 3);
    
    auto v2 = parse_version("\t2.0.0\n");
    REQUIRE(v2);
    CHECK(v2->major == 2);
}

TEST_CASE("requirement parsing trims whitespace") {
    auto req1 = parse_requirement("  ^1.2.3  ");
    REQUIRE(req1);
    CHECK(req1->kind == RequirementKind::Caret);
    CHECK(req1->lower == SemVer{1,2,3});
    
    auto req2 = parse_requirement("\t~2.0.0\n");
    REQUIRE(req2);
    CHECK(req2->kind == RequirementKind::Tilde);
}

// ============================================================================
// min_version Derivation Tests (per SPEC L273-276)
// ============================================================================

TEST_CASE("requirement lower bound is min_version") {
    // Per SPEC L273-276: min_version derivation
    // The lower bound IS the minimum satisfying version
    
    // Exact: min_version = the version
    auto exact = parse_requirement("1.5.0");
    REQUIRE(exact);
    CHECK(exact->lower == SemVer{1,5,0});
    
    // Caret: min_version = specified version
    auto caret = parse_requirement("^2.3.4");
    REQUIRE(caret);
    CHECK(caret->lower == SemVer{2,3,4});
    
    // Tilde: min_version = specified version
    auto tilde = parse_requirement("~3.1.0");
    REQUIRE(tilde);
    CHECK(tilde->lower == SemVer{3,1,0});
    
    // Wildcard: min_version = X.Y.0
    auto wild = parse_requirement("4.2.*");
    REQUIRE(wild);
    CHECK(wild->lower == SemVer{4,2,0});
    
    // Bounded: min_version = lower bound
    auto bounded = parse_requirement(">=1.0.0 <2.0.0");
    REQUIRE(bounded);
    CHECK(bounded->lower == SemVer{1,0,0});
}
