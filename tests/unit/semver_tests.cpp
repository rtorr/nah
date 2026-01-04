#include <doctest/doctest.h>
#include <nah/semver.hpp>

using nah::Version;
using nah::VersionRange;
using nah::Comparator;
using nah::parse_version;
using nah::parse_range;
using nah::satisfies;

// ============================================================================
// Version Parsing Tests (SemVer 2.0.0)
// ============================================================================

TEST_CASE("parse_version accepts MAJOR.MINOR.PATCH") {
    auto v = parse_version("1.2.3");
    REQUIRE(v);
    CHECK(v->major() == 1);
    CHECK(v->minor() == 2);
    CHECK(v->patch() == 3);
    CHECK_FALSE(v->is_prerelease());
}

TEST_CASE("parse_version accepts pre-release versions") {
    auto v = parse_version("1.0.0-alpha.1");
    REQUIRE(v);
    CHECK(v->major() == 1);
    CHECK(v->minor() == 0);
    CHECK(v->patch() == 0);
    CHECK(v->is_prerelease());
    CHECK(v->prerelease() == "alpha.1");
}

TEST_CASE("parse_version accepts build metadata") {
    auto v = parse_version("1.0.0+build.123");
    REQUIRE(v);
    CHECK(v->major() == 1);
    CHECK(v->build_meta() == "build.123");
}

TEST_CASE("parse_version accepts pre-release and build metadata") {
    auto v = parse_version("1.0.0-beta.2+build.456");
    REQUIRE(v);
    CHECK(v->is_prerelease());
    CHECK(v->prerelease() == "beta.2");
    CHECK(v->build_meta() == "build.456");
}

TEST_CASE("parse_version rejects invalid versions") {
    CHECK_FALSE(parse_version(""));
    CHECK_FALSE(parse_version("not.a.version"));
    CHECK_FALSE(parse_version("1.2"));
    CHECK_FALSE(parse_version("1"));
    CHECK_FALSE(parse_version("1.2.3.4"));
}

TEST_CASE("parse_version trims whitespace") {
    auto v = parse_version("  1.2.3  ");
    REQUIRE(v);
    CHECK(v->major() == 1);
    CHECK(v->minor() == 2);
    CHECK(v->patch() == 3);
}

// ============================================================================
// Version Comparison Tests (SemVer 2.0.0 precedence)
// ============================================================================

TEST_CASE("version comparison by major.minor.patch") {
    CHECK(*parse_version("1.0.0") < *parse_version("2.0.0"));
    CHECK(*parse_version("1.0.0") < *parse_version("1.1.0"));
    CHECK(*parse_version("1.0.0") < *parse_version("1.0.1"));
    CHECK(*parse_version("1.2.3") == *parse_version("1.2.3"));
}

TEST_CASE("pre-release versions have lower precedence than release") {
    // Per SemVer 2.0.0: 1.0.0-alpha < 1.0.0
    CHECK(*parse_version("1.0.0-alpha") < *parse_version("1.0.0"));
    CHECK(*parse_version("1.0.0-alpha.1") < *parse_version("1.0.0"));
    CHECK(*parse_version("1.0.0-rc.1") < *parse_version("1.0.0"));
}

TEST_CASE("pre-release version comparison") {
    // Per SemVer 2.0.0: alpha < alpha.1 < alpha.2 < beta
    CHECK(*parse_version("1.0.0-alpha") < *parse_version("1.0.0-alpha.1"));
    CHECK(*parse_version("1.0.0-alpha.1") < *parse_version("1.0.0-alpha.2"));
    CHECK(*parse_version("1.0.0-alpha.2") < *parse_version("1.0.0-beta"));
}

TEST_CASE("build metadata is ignored in comparison") {
    // Per SemVer 2.0.0: build metadata does not affect precedence
    CHECK(*parse_version("1.0.0+build1") == *parse_version("1.0.0+build2"));
    CHECK(*parse_version("1.0.0-alpha+build1") == *parse_version("1.0.0-alpha+build2"));
}

// ============================================================================
// Range Parsing Tests
// ============================================================================

TEST_CASE("parse_range accepts exact version") {
    auto r = parse_range("1.2.3");
    REQUIRE(r);
    CHECK(r->sets.size() == 1);
    CHECK(r->sets[0].size() == 1);
    CHECK(r->sets[0][0].op == Comparator::Eq);
}

TEST_CASE("parse_range accepts >= constraint") {
    auto r = parse_range(">=1.0.0");
    REQUIRE(r);
    CHECK(r->sets.size() == 1);
    CHECK(r->sets[0].size() == 1);
    CHECK(r->sets[0][0].op == Comparator::Ge);
}

TEST_CASE("parse_range accepts < constraint") {
    auto r = parse_range("<2.0.0");
    REQUIRE(r);
    CHECK(r->sets[0][0].op == Comparator::Lt);
}

TEST_CASE("parse_range accepts <= constraint") {
    auto r = parse_range("<=2.0.0");
    REQUIRE(r);
    CHECK(r->sets[0][0].op == Comparator::Le);
}

TEST_CASE("parse_range accepts > constraint") {
    auto r = parse_range(">1.0.0");
    REQUIRE(r);
    CHECK(r->sets[0][0].op == Comparator::Gt);
}

TEST_CASE("parse_range accepts = constraint") {
    auto r = parse_range("=1.2.3");
    REQUIRE(r);
    CHECK(r->sets[0][0].op == Comparator::Eq);
}

TEST_CASE("parse_range accepts space-separated AND constraints") {
    auto r = parse_range(">=1.0.0 <2.0.0");
    REQUIRE(r);
    CHECK(r->sets.size() == 1);
    CHECK(r->sets[0].size() == 2);
    CHECK(r->sets[0][0].op == Comparator::Ge);
    CHECK(r->sets[0][1].op == Comparator::Lt);
}

TEST_CASE("parse_range accepts || for OR") {
    auto r = parse_range(">=1.0.0 <2.0.0 || >=3.0.0");
    REQUIRE(r);
    CHECK(r->sets.size() == 2);
    CHECK(r->sets[0].size() == 2);  // >=1.0.0 <2.0.0
    CHECK(r->sets[1].size() == 1);  // >=3.0.0
}

TEST_CASE("parse_range rejects invalid ranges") {
    CHECK_FALSE(parse_range(""));
    CHECK_FALSE(parse_range(">="));
    CHECK_FALSE(parse_range(">= invalid"));
    CHECK_FALSE(parse_range(">=1.0.0 ||"));
}

TEST_CASE("parse_range trims whitespace") {
    auto r = parse_range("  >=1.0.0  <2.0.0  ");
    REQUIRE(r);
    CHECK(r->sets[0].size() == 2);
}

// ============================================================================
// Range Satisfaction Tests
// ============================================================================

TEST_CASE("exact version satisfaction") {
    auto r = parse_range("1.2.3");
    REQUIRE(r);
    CHECK(satisfies(*parse_version("1.2.3"), *r));
    CHECK_FALSE(satisfies(*parse_version("1.2.4"), *r));
    CHECK_FALSE(satisfies(*parse_version("1.2.2"), *r));
}

TEST_CASE(">= constraint satisfaction") {
    auto r = parse_range(">=1.0.0");
    REQUIRE(r);
    CHECK(satisfies(*parse_version("1.0.0"), *r));
    CHECK(satisfies(*parse_version("1.0.1"), *r));
    CHECK(satisfies(*parse_version("2.0.0"), *r));
    CHECK_FALSE(satisfies(*parse_version("0.9.9"), *r));
}

TEST_CASE("> constraint satisfaction") {
    auto r = parse_range(">1.0.0");
    REQUIRE(r);
    CHECK_FALSE(satisfies(*parse_version("1.0.0"), *r));
    CHECK(satisfies(*parse_version("1.0.1"), *r));
    CHECK(satisfies(*parse_version("2.0.0"), *r));
}

TEST_CASE("< constraint satisfaction") {
    auto r = parse_range("<2.0.0");
    REQUIRE(r);
    CHECK(satisfies(*parse_version("1.9.9"), *r));
    CHECK_FALSE(satisfies(*parse_version("2.0.0"), *r));
    CHECK_FALSE(satisfies(*parse_version("2.0.1"), *r));
}

TEST_CASE("<= constraint satisfaction") {
    auto r = parse_range("<=2.0.0");
    REQUIRE(r);
    CHECK(satisfies(*parse_version("1.9.9"), *r));
    CHECK(satisfies(*parse_version("2.0.0"), *r));
    CHECK_FALSE(satisfies(*parse_version("2.0.1"), *r));
}

TEST_CASE("range (AND) satisfaction") {
    auto r = parse_range(">=1.0.0 <2.0.0");
    REQUIRE(r);
    CHECK(satisfies(*parse_version("1.0.0"), *r));
    CHECK(satisfies(*parse_version("1.5.0"), *r));
    CHECK(satisfies(*parse_version("1.9.9"), *r));
    CHECK_FALSE(satisfies(*parse_version("0.9.9"), *r));
    CHECK_FALSE(satisfies(*parse_version("2.0.0"), *r));
}

TEST_CASE("union (OR) satisfaction") {
    auto r = parse_range(">=1.0.0 <2.0.0 || >=3.0.0 <4.0.0");
    REQUIRE(r);
    CHECK(satisfies(*parse_version("1.5.0"), *r));
    CHECK(satisfies(*parse_version("3.5.0"), *r));
    CHECK_FALSE(satisfies(*parse_version("2.5.0"), *r));
    CHECK_FALSE(satisfies(*parse_version("0.5.0"), *r));
    CHECK_FALSE(satisfies(*parse_version("4.0.0"), *r));
}

TEST_CASE("pre-release in range") {
    auto r = parse_range(">=1.0.0-alpha <1.0.0");
    REQUIRE(r);
    CHECK(satisfies(*parse_version("1.0.0-alpha"), *r));
    CHECK(satisfies(*parse_version("1.0.0-beta"), *r));
    CHECK(satisfies(*parse_version("1.0.0-rc.1"), *r));
    CHECK_FALSE(satisfies(*parse_version("1.0.0"), *r));
}

// ============================================================================
// selection_key Tests
// ============================================================================

TEST_CASE("selection_key returns MAJOR.MINOR from min_version") {
    auto r = parse_range(">=1.2.0 <2.0.0");
    REQUIRE(r);
    CHECK(r->selection_key() == "1.2");
}

TEST_CASE("selection_key for exact version") {
    auto r = parse_range("1.5.3");
    REQUIRE(r);
    CHECK(r->selection_key() == "1.5");
}

TEST_CASE("selection_key for OR uses lowest min_version") {
    auto r = parse_range(">=2.0.0 || >=1.0.0");
    REQUIRE(r);
    CHECK(r->selection_key() == "1.0");
}

TEST_CASE("selection_key empty for < only constraint") {
    auto r = parse_range("<2.0.0");
    REQUIRE(r);
    CHECK(r->selection_key() == "");
}
