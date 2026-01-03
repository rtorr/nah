#pragma once

#include <optional>
#include <string>

namespace nah {

struct SemVer {
    int major;
    int minor;
    int patch;
};

inline bool operator==(const SemVer& a, const SemVer& b) {
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch;
}

inline bool operator<(const SemVer& a, const SemVer& b) {
    if (a.major != b.major) return a.major < b.major;
    if (a.minor != b.minor) return a.minor < b.minor;
    return a.patch < b.patch;
}

enum class RequirementKind {
    Exact,
    Caret,
    Tilde,
    Wildcard,
    Bounded
};

struct SemVerRequirement {
    RequirementKind kind;
    SemVer lower;           // inclusive lower bound
    SemVer upper;           // exclusive upper bound for caret/tilde/bounded
    std::string selection_key; // MAJOR.MINOR derived from lower (for mapped mode)
};

// Parse a core SemVer version string (MAJOR.MINOR.PATCH, no pre-release/build).
std::optional<SemVer> parse_version(const std::string& str);

// Parse a SemVer requirement per SPEC supported forms.
std::optional<SemVerRequirement> parse_requirement(const std::string& str);

// Evaluate whether a version satisfies a requirement.
bool satisfies(const SemVer& version, const SemVerRequirement& req);

} // namespace nah

