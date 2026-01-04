#pragma once

// NAH uses Semantic Versioning 2.0.0: https://semver.org/spec/v2.0.0.html
// Version parsing and comparison provided by cpp-semver library.
// Range parsing follows standard comparator syntax with || for unions.

#include <semver/semver.hpp>
#include <optional>
#include <string>
#include <vector>

namespace nah {

// Re-export semver::version as the canonical version type
using Version = semver::version;

// Comparator operators for range expressions
enum class Comparator {
    Eq,   // =X.Y.Z or X.Y.Z (exact match)
    Lt,   // <X.Y.Z
    Le,   // <=X.Y.Z
    Gt,   // >X.Y.Z
    Ge    // >=X.Y.Z
};

// A single comparator constraint (e.g., ">=1.0.0" or "<2.0.0")
struct Constraint {
    Comparator op;
    Version version;
};

// A comparator set is a set of constraints that must ALL be satisfied (AND)
// e.g., ">=1.0.0 <2.0.0" is two constraints ANDed together
using ComparatorSet = std::vector<Constraint>;

// A version range is a union of comparator sets (OR)
// e.g., ">=1.0.0 <2.0.0 || >=3.0.0" is two sets ORed together
struct VersionRange {
    std::vector<ComparatorSet> sets;
    
    // Get the minimum version from the range (used for mapped mode selection_key)
    std::optional<Version> min_version() const;
    
    // Get selection key as "MAJOR.MINOR" from min_version
    std::string selection_key() const;
};

// Parse a SemVer 2.0.0 version string.
// Supports full semver: MAJOR.MINOR.PATCH[-prerelease][+build]
// Returns nullopt on parse failure.
std::optional<Version> parse_version(const std::string& str);

// Parse a version range string.
// Supports: =, <, <=, >, >= comparators, space-separated AND, || for OR
// Examples:
//   "1.0.0"                    - exact match
//   ">=1.0.0"                  - minimum version
//   ">=1.0.0 <2.0.0"           - range (AND)
//   ">=1.0.0 <2.0.0 || >=3.0.0" - union (OR)
// Returns nullopt on parse failure.
std::optional<VersionRange> parse_range(const std::string& str);

// Evaluate whether a version satisfies a single constraint.
bool satisfies(const Version& version, const Constraint& constraint);

// Evaluate whether a version satisfies a comparator set (all constraints).
bool satisfies(const Version& version, const ComparatorSet& set);

// Evaluate whether a version satisfies a version range (any set).
bool satisfies(const Version& version, const VersionRange& range);

} // namespace nah

