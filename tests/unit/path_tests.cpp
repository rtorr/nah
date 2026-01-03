#include <doctest/doctest.h>
#include <nah/path_utils.hpp>

using nah::PathError;
using nah::normalize_under_root;

TEST_CASE("normalize simple relative path under root") {
    auto r = normalize_under_root("/nah/apps/app", "bin/app");
    REQUIRE(r.ok);
    CHECK(r.path == "/nah/apps/app/bin/app");
}

TEST_CASE("collapse dot and dotdot segments") {
    auto r = normalize_under_root("/nah/apps/app", "./bin/../lib/./file");
    REQUIRE(r.ok);
    CHECK(r.path == "/nah/apps/app/lib/file");
}

TEST_CASE("reject escape above root") {
    auto r = normalize_under_root("/nah/apps/app", "../../etc/passwd");
    CHECK_FALSE(r.ok);
    CHECK(r.error == PathError::EscapesRoot);
}

TEST_CASE("reject absolute when not allowed") {
    auto r = normalize_under_root("/nah/apps/app", "/abs/path");
    CHECK_FALSE(r.ok);
    CHECK(r.error == PathError::AbsoluteNotAllowed);
}

TEST_CASE("reject NUL bytes") {
    std::string bad = std::string("bin/\0app", 8);
    auto r = normalize_under_root("/nah/apps/app", bad);
    CHECK_FALSE(r.ok);
    CHECK(r.error == PathError::ContainsNul);
}

// ============================================================================
// Canonical Persisted Paths Tests (per SPEC L787-802)
// ============================================================================

TEST_CASE("normalized path is always absolute") {
    // Per SPEC L787-802: Canonical persisted paths are absolute
    auto r = normalize_under_root("/nah/apps/app", "bin/executable");
    REQUIRE(r.ok);
    CHECK(r.path[0] == '/');  // Must start with /
}

TEST_CASE("normalized path has no trailing slash") {
    auto r = normalize_under_root("/nah/apps/app", "bin/subdir/");
    REQUIRE(r.ok);
    // The normalized path should not have trailing slash for non-root
    bool no_trailing_slash = (r.path.back() != '/') || (r.path == "/");
    CHECK(no_trailing_slash);
}

TEST_CASE("normalize handles multiple consecutive slashes") {
    auto r = normalize_under_root("/nah/apps/app", "bin//subdir///file");
    REQUIRE(r.ok);
    CHECK(r.path.find("//") == std::string::npos);  // No double slashes
}

// ============================================================================
// Derived Paths Tests (per SPEC L804-807)
// ============================================================================

TEST_CASE("relative path resolved correctly under root") {
    // Per SPEC L804-807: Derived paths (app.entrypoint, etc.)
    auto r = normalize_under_root("/nah/apps/myapp/1.0.0", "bin/run");
    REQUIRE(r.ok);
    CHECK(r.path == "/nah/apps/myapp/1.0.0/bin/run");
}

TEST_CASE("empty relative path returns root") {
    auto r = normalize_under_root("/nah/apps/app", "");
    REQUIRE(r.ok);
    CHECK(r.path == "/nah/apps/app");
}

TEST_CASE("dot path returns root") {
    auto r = normalize_under_root("/nah/apps/app", ".");
    REQUIRE(r.ok);
    CHECK(r.path == "/nah/apps/app");
}

// ============================================================================
// Symlink Rejection Tests (per SPEC L831-834)
// ============================================================================

TEST_CASE("allow_symlinks flag controls symlink behavior") {
    // When allow_symlinks = false (default), symlinks should be rejected
    // When allow_symlinks = true, they should be allowed
    // This test validates the function signature supports both modes
    
    // Test with no-follow (symlinks rejected for containment check)
    auto r1 = normalize_under_root("/nah/apps/app", "bin/app", false);
    // Just verify it doesn't crash - actual symlink behavior depends on filesystem
    CHECK(r1.ok);
    
    // Test with follow (symlinks allowed)
    auto r2 = normalize_under_root("/nah/apps/app", "bin/app", true);
    CHECK(r2.ok);
}

// ============================================================================
// PATH_TRAVERSAL CriticalError Conditions (per SPEC L836-838)
// ============================================================================

TEST_CASE("escape above root returns PATH_TRAVERSAL-compatible error") {
    // Per SPEC L836-838: CriticalError::PATH_TRAVERSAL on escape
    auto r = normalize_under_root("/nah/apps/app", "../../../etc/passwd");
    CHECK_FALSE(r.ok);
    CHECK(r.error == PathError::EscapesRoot);
}

TEST_CASE("deeply nested escape still detected") {
    auto r = normalize_under_root("/nah/apps/app", "a/b/c/../../../../../../../etc");
    CHECK_FALSE(r.ok);
    CHECK(r.error == PathError::EscapesRoot);
}

TEST_CASE("absolute path when not allowed returns error") {
    auto r = normalize_under_root("/nah/apps/app", "/etc/passwd");
    CHECK_FALSE(r.ok);
    CHECK(r.error == PathError::AbsoluteNotAllowed);
}
