#pragma once

/**
 * @file semver.hpp
 * @brief Semantic Versioning 2.0.0 support
 * 
 * NAH uses SemVer 2.0.0 (https://semver.org/spec/v2.0.0.html) for all version
 * handling. This header provides:
 * - Version parsing and comparison
 * - Version range parsing (>=, <, etc.)
 * - Range satisfaction checking
 * 
 * @example
 * ```cpp
 * #include <nah/semver.hpp>
 * 
 * auto version = nah::parse_version("1.2.3");
 * auto range = nah::parse_range(">=1.0.0 <2.0.0");
 * 
 * if (version && range && nah::satisfies(*version, *range)) {
 *     // Version satisfies the range
 * }
 * ```
 */

// cpp-semver requires <cstdint> but doesn't include it (GCC strictness)
#include <cstdint>
#include <semver/semver.hpp>
#include <optional>
#include <string>
#include <vector>

namespace nah {

/// Semantic version type (MAJOR.MINOR.PATCH[-prerelease][+build])
using Version = semver::version;

/// Comparator operators for range expressions
enum class Comparator {
    Eq,   ///< =X.Y.Z or X.Y.Z (exact match)
    Lt,   ///< <X.Y.Z
    Le,   ///< <=X.Y.Z
    Gt,   ///< >X.Y.Z
    Ge    ///< >=X.Y.Z
};

/// A single comparator constraint (e.g., ">=1.0.0" or "<2.0.0")
struct Constraint {
    Comparator op;
    Version version;
};

/// A comparator set is constraints that must ALL be satisfied (AND)
/// e.g., ">=1.0.0 <2.0.0" is two constraints ANDed together
using ComparatorSet = std::vector<Constraint>;

/**
 * @brief A version range is a union of comparator sets (OR)
 * 
 * e.g., ">=1.0.0 <2.0.0 || >=3.0.0" is two sets ORed together
 */
struct VersionRange {
    std::vector<ComparatorSet> sets;
    
    /// Get the minimum version from the range
    std::optional<Version> min_version() const;
    
    /// Get selection key as "MAJOR.MINOR" from min_version
    std::string selection_key() const;
};

/**
 * @brief Parse a SemVer 2.0.0 version string
 * @param str Version string (e.g., "1.2.3", "1.0.0-alpha+build")
 * @return Parsed version or nullopt on failure
 */
std::optional<Version> parse_version(const std::string& str);

/**
 * @brief Parse a version range string
 * @param str Range string (e.g., ">=1.0.0 <2.0.0", "1.0.0 || >=2.0.0")
 * @return Parsed range or nullopt on failure
 * 
 * Supports: =, <, <=, >, >= comparators, space-separated AND, || for OR
 */
std::optional<VersionRange> parse_range(const std::string& str);

/// Check if a version satisfies a single constraint
bool satisfies(const Version& version, const Constraint& constraint);

/// Check if a version satisfies a comparator set (all constraints)
bool satisfies(const Version& version, const ComparatorSet& set);

/// Check if a version satisfies a version range (any set)
bool satisfies(const Version& version, const VersionRange& range);

} // namespace nah

