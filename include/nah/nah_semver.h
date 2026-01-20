/**
 * NAH Semver - Semantic Versioning 2.0.0 Support
 * 
 * Header-only wrapper around cpp-semver providing:
 * - Version parsing and comparison
 * - Version range parsing (>=, <, ^, ~, etc.)
 * - Range satisfaction checking
 * 
 * Usage:
 *   #include <nah/nah_semver.h>
 *   
 *   auto version = nah::semver::parse_version("1.2.3");
 *   auto range = nah::semver::parse_range(">=1.0.0 <2.0.0");
 *   
 *   if (version && range && nah::semver::satisfies(*version, *range)) {
 *       // Version satisfies the range
 *   }
 * 
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NAH_SEMVER_H
#define NAH_SEMVER_H

#ifdef __cplusplus

// cpp-semver requires <cstdint> but may not include it on all compilers
#include <cstdint>
#include <semver/semver.hpp>

#include <optional>
#include <string>
#include <vector>
#include <cctype>
#include <sstream>
#include <algorithm>

namespace nah {
namespace semver {

// ============================================================================
// Types
// ============================================================================

/// Semantic version type (MAJOR.MINOR.PATCH[-prerelease][+build])
using Version = ::semver::version;

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
 * A version range is a union of comparator sets (OR)
 * e.g., ">=1.0.0 <2.0.0 || >=3.0.0" is two sets ORed together
 */
struct VersionRange {
    std::vector<ComparatorSet> sets;

    /// Get the minimum version from the range (for NAK selection)
    std::optional<Version> min_version() const;

    /// Get selection key as "MAJOR.MINOR" from min_version
    std::string selection_key() const;
};

// ============================================================================
// API Functions
// ============================================================================

/**
 * Parse a SemVer 2.0.0 version string
 * @param str Version string (e.g., "1.2.3", "1.0.0-alpha+build")
 * @return Parsed version or nullopt on failure
 */
inline std::optional<Version> parse_version(const std::string& str);

/**
 * Parse a version range string
 * @param str Range string (e.g., ">=1.0.0 <2.0.0", "^1.2.0", "~1.2.3")
 * @return Parsed range or nullopt on failure
 *
 * Supports:
 * - Comparators: =, <, <=, >, >=
 * - Caret ranges: ^1.2.3 (>=1.2.3 <2.0.0)
 * - Tilde ranges: ~1.2.3 (>=1.2.3 <1.3.0)
 * - X-ranges: 1.x, 1.2.x (any matching version)
 * - Space-separated AND: ">=1.0.0 <2.0.0"
 * - OR with ||: ">=1.0.0 <2.0.0 || >=3.0.0"
 */
inline std::optional<VersionRange> parse_range(const std::string& str);

/// Check if a version satisfies a single constraint
inline bool satisfies(const Version& version, const Constraint& constraint);

/// Check if a version satisfies a comparator set (all constraints)
inline bool satisfies(const Version& version, const ComparatorSet& set);

/// Check if a version satisfies a version range (any set)
inline bool satisfies(const Version& version, const VersionRange& range);

/**
 * Select the best matching version from a list
 * @param versions Available versions
 * @param range Version requirement
 * @return Best (highest) matching version or nullopt if none match
 */
inline std::optional<Version> select_best(
    const std::vector<Version>& versions,
    const VersionRange& range);

// ============================================================================
// Implementation
// ============================================================================

namespace detail {

inline std::string trim(const std::string& in) {
    size_t start = 0;
    while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) ++start;
    size_t end = in.size();
    while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) --end;
    return in.substr(start, end - start);
}

inline std::vector<std::string> split(const std::string& s, const std::string& delim) {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t pos;
    while ((pos = s.find(delim, start)) != std::string::npos) {
        parts.push_back(s.substr(start, pos - start));
        start = pos + delim.length();
    }
    parts.push_back(s.substr(start));
    return parts;
}

inline std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Expand caret range: ^1.2.3 -> >=1.2.3 <2.0.0
inline std::optional<ComparatorSet> expand_caret(const std::string& version_str) {
    try {
        auto v = ::semver::version::parse(version_str);
        ComparatorSet set;
        set.push_back({Comparator::Ge, v});
        
        // ^0.0.x -> =0.0.x (exact)
        // ^0.x -> >=0.x.0 <0.(x+1).0
        // ^x.y.z -> >=x.y.z <(x+1).0.0
        Version upper;
        if (v.major() == 0) {
            if (v.minor() == 0) {
                // ^0.0.x means exactly 0.0.x
                set.clear();
                set.push_back({Comparator::Eq, v});
                return set;
            }
            // ^0.x means <0.(x+1).0
            upper = Version(0, v.minor() + 1, 0);
        } else {
            upper = Version(v.major() + 1, 0, 0);
        }
        set.push_back({Comparator::Lt, upper});
        return set;
    } catch (...) {
        return std::nullopt;
    }
}

// Expand tilde range: ~1.2.3 -> >=1.2.3 <1.3.0
inline std::optional<ComparatorSet> expand_tilde(const std::string& version_str) {
    try {
        auto v = ::semver::version::parse(version_str);
        ComparatorSet set;
        set.push_back({Comparator::Ge, v});
        set.push_back({Comparator::Lt, Version(v.major(), v.minor() + 1, 0)});
        return set;
    } catch (...) {
        return std::nullopt;
    }
}

// Parse X-range: 1.x, 1.2.x, * 
inline std::optional<ComparatorSet> expand_x_range(const std::string& str) {
    std::string s = trim(str);
    
    // * or x means any version
    if (s == "*" || s == "x" || s == "X") {
        return ComparatorSet{}; // Empty set = match anything
    }
    
    // Check for x/X in version
    auto parts = split(s, ".");
    if (parts.empty()) return std::nullopt;
    
    try {
        if (parts.size() == 1 || (parts.size() >= 2 && (parts[1] == "x" || parts[1] == "X" || parts[1] == "*"))) {
            // 1.x or 1.* -> >=1.0.0 <2.0.0
            uint64_t major = static_cast<uint64_t>(std::stoi(parts[0]));
            ComparatorSet set;
            set.push_back({Comparator::Ge, Version(major, 0, 0)});
            set.push_back({Comparator::Lt, Version(major + 1, 0, 0)});
            return set;
        }
        
        if (parts.size() >= 3 && (parts[2] == "x" || parts[2] == "X" || parts[2] == "*")) {
            // 1.2.x or 1.2.* -> >=1.2.0 <1.3.0
            uint64_t major = static_cast<uint64_t>(std::stoi(parts[0]));
            uint64_t minor = static_cast<uint64_t>(std::stoi(parts[1]));
            ComparatorSet set;
            set.push_back({Comparator::Ge, Version(major, minor, 0)});
            set.push_back({Comparator::Lt, Version(major, minor + 1, 0)});
            return set;
        }
    } catch (...) {
        return std::nullopt;
    }
    
    return std::nullopt;
}

inline std::optional<Constraint> parse_constraint(const std::string& str) {
    std::string s = trim(str);
    if (s.empty()) return std::nullopt;
    
    Comparator op = Comparator::Eq;
    std::string version_str;
    
    if (s.rfind(">=", 0) == 0) {
        op = Comparator::Ge;
        version_str = s.substr(2);
    } else if (s.rfind("<=", 0) == 0) {
        op = Comparator::Le;
        version_str = s.substr(2);
    } else if (s.rfind(">", 0) == 0) {
        op = Comparator::Gt;
        version_str = s.substr(1);
    } else if (s.rfind("<", 0) == 0) {
        op = Comparator::Lt;
        version_str = s.substr(1);
    } else if (s.rfind("=", 0) == 0) {
        op = Comparator::Eq;
        version_str = s.substr(1);
    } else {
        op = Comparator::Eq;
        version_str = s;
    }
    
    version_str = trim(version_str);
    if (version_str.empty()) return std::nullopt;
    
    try {
        auto version = ::semver::version::parse(version_str);
        return Constraint{op, version};
    } catch (...) {
        return std::nullopt;
    }
}

inline std::optional<ComparatorSet> parse_comparator_set(const std::string& str) {
    std::string s = trim(str);
    if (s.empty()) return ComparatorSet{}; // Empty = match all
    
    // Check for caret range
    if (s[0] == '^') {
        return expand_caret(s.substr(1));
    }
    
    // Check for tilde range
    if (s[0] == '~') {
        return expand_tilde(s.substr(1));
    }
    
    // Check for X-range
    if (s.find('x') != std::string::npos || s.find('X') != std::string::npos || 
        s.find('*') != std::string::npos || s == "*") {
        auto expanded = expand_x_range(s);
        if (expanded) return expanded;
    }
    
    // Parse as space-separated constraints
    auto tokens = tokenize(s);
    if (tokens.empty()) return ComparatorSet{};
    
    ComparatorSet set;
    for (const auto& token : tokens) {
        // Skip if token is a caret/tilde (handle nested)
        if (token[0] == '^') {
            auto expanded = expand_caret(token.substr(1));
            if (!expanded) return std::nullopt;
            for (const auto& c : *expanded) set.push_back(c);
        } else if (token[0] == '~') {
            auto expanded = expand_tilde(token.substr(1));
            if (!expanded) return std::nullopt;
            for (const auto& c : *expanded) set.push_back(c);
        } else {
            auto constraint = parse_constraint(token);
            if (!constraint) return std::nullopt;
            set.push_back(*constraint);
        }
    }
    return set;
}

} // namespace detail

// ============================================================================
// VersionRange methods
// ============================================================================

inline std::optional<Version> VersionRange::min_version() const {
    std::optional<Version> min;
    
    for (const auto& set : sets) {
        for (const auto& constraint : set) {
            if (constraint.op == Comparator::Ge || constraint.op == Comparator::Eq ||
                constraint.op == Comparator::Gt) {
                if (!min || constraint.version < *min) {
                    min = constraint.version;
                }
            }
        }
    }
    
    return min;
}

inline std::string VersionRange::selection_key() const {
    auto min = min_version();
    if (!min) return "";
    return std::to_string(min->major()) + "." + std::to_string(min->minor());
}

// ============================================================================
// API Implementation
// ============================================================================

inline std::optional<Version> parse_version(const std::string& str) {
    std::string s = detail::trim(str);
    if (s.empty()) return std::nullopt;
    
    try {
        return ::semver::version::parse(s);
    } catch (...) {
        return std::nullopt;
    }
}

inline std::optional<VersionRange> parse_range(const std::string& str) {
    std::string s = detail::trim(str);
    if (s.empty()) return std::nullopt;
    
    // Split by || for OR
    auto or_parts = detail::split(s, "||");
    
    VersionRange range;
    for (const auto& part : or_parts) {
        auto set = detail::parse_comparator_set(detail::trim(part));
        if (!set) return std::nullopt;
        range.sets.push_back(*set);
    }
    
    if (range.sets.empty()) return std::nullopt;
    return range;
}

inline bool satisfies(const Version& version, const Constraint& constraint) {
    switch (constraint.op) {
        case Comparator::Eq:
            return version == constraint.version;
        case Comparator::Lt:
            return version < constraint.version;
        case Comparator::Le:
            return version <= constraint.version;
        case Comparator::Gt:
            return version > constraint.version;
        case Comparator::Ge:
            return version >= constraint.version;
    }
    return false;
}

inline bool satisfies(const Version& version, const ComparatorSet& set) {
    // Empty set matches everything
    if (set.empty()) return true;
    
    // All constraints in a set must be satisfied (AND)
    for (const auto& constraint : set) {
        if (!satisfies(version, constraint)) {
            return false;
        }
    }
    return true;
}

inline bool satisfies(const Version& version, const VersionRange& range) {
    // Empty range matches nothing
    if (range.sets.empty()) return false;
    
    // Any set in the range must be satisfied (OR)
    for (const auto& set : range.sets) {
        if (satisfies(version, set)) {
            return true;
        }
    }
    return false;
}

inline std::optional<Version> select_best(
    const std::vector<Version>& versions,
    const VersionRange& range)
{
    std::optional<Version> best;
    
    for (const auto& v : versions) {
        if (satisfies(v, range)) {
            if (!best || v > *best) {
                best = v;
            }
        }
    }
    
    return best;
}

// ============================================================================
// NAK Selection Helper
// ============================================================================

/**
 * Select the best matching NAK from inventory for a given requirement.
 * 
 * This is used at install time to determine which NAK version to pin.
 * 
 * @param inventory Available NAK runtimes (from RuntimeInventory.runtimes)
 * @param nak_id Required NAK ID (e.g., "lua", "node")
 * @param version_req Version requirement string (e.g., ">=5.4.0", "^20.0.0")
 * @return Selection result with best matching version or error
 * 
 * Example:
 *   // inventory maps "lua@5.4.6.json" -> RuntimeDescriptor{nak.id="lua", nak.version="5.4.6"}
 *   auto result = select_nak_for_install(inventory, "lua", ">=5.4.0");
 *   if (result.found) {
 *       install_record.nak.id = result.nak_id;
 *       install_record.nak.version = result.nak_version;
 *       install_record.nak.record_ref = result.record_ref;
 *   }
 */
struct NakSelectionResult {
    bool found = false;              ///< Was a matching NAK found?
    std::string nak_id;              ///< The NAK ID
    std::string nak_version;         ///< Selected version (e.g., "5.4.6")
    std::string record_ref;          ///< Reference key (e.g., "lua@5.4.6.json")
    std::string selection_reason;    ///< Why this version was selected
    std::vector<std::string> candidates;  ///< All versions that matched
    std::string error;               ///< Error message if not found
};

/**
 * Select best NAK from a map of record_ref -> (id, version) pairs.
 * 
 * @tparam RuntimeMap Map type with .first=record_ref, .second having .nak.id and .nak.version
 */
template<typename RuntimeMap>
inline NakSelectionResult select_nak_from_inventory(
    const RuntimeMap& runtimes,
    const std::string& nak_id,
    const std::string& version_req)
{
    NakSelectionResult result;
    result.nak_id = nak_id;
    
    // Parse version requirement
    auto range = parse_range(version_req);
    if (!range) {
        result.error = "Invalid version requirement: " + version_req;
        return result;
    }
    
    // Find all matching versions
    std::vector<std::pair<Version, std::string>> matches; // (version, record_ref)
    
    for (const auto& [record_ref, runtime] : runtimes) {
        // Check if this runtime matches the NAK ID
        if (runtime.nak.id != nak_id) {
            continue;
        }
        
        // Parse the runtime version
        auto version = parse_version(runtime.nak.version);
        if (!version) {
            continue; // Skip invalid versions
        }
        
        // Check if it satisfies the requirement
        if (satisfies(*version, *range)) {
            matches.push_back({*version, record_ref});
            result.candidates.push_back(runtime.nak.version);
        }
    }
    
    if (matches.empty()) {
        result.error = "No NAK found matching " + nak_id + " " + version_req;
        return result;
    }
    
    // Select the highest matching version
    auto best = std::max_element(matches.begin(), matches.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    
    result.found = true;
    result.nak_version = best->first.str();
    result.record_ref = best->second;
    result.selection_reason = "highest_matching_version";
    
    return result;
}

} // namespace semver
} // namespace nah

#endif // __cplusplus

#endif // NAH_SEMVER_H
