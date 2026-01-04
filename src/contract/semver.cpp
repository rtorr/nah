#include "nah/semver.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace nah {

namespace {

std::string trim(const std::string& in) {
    size_t start = 0;
    while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) ++start;
    size_t end = in.size();
    while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) --end;
    return in.substr(start, end - start);
}

// Split string by delimiter, preserving empty parts
std::vector<std::string> split(const std::string& s, const std::string& delim) {
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

// Split string by whitespace into tokens
std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Parse a single constraint like ">=1.0.0", "<2.0.0", "=1.0.0", or "1.0.0"
std::optional<Constraint> parse_constraint(const std::string& str) {
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
        // No operator means exact match
        op = Comparator::Eq;
        version_str = s;
    }
    
    version_str = trim(version_str);
    if (version_str.empty()) return std::nullopt;
    
    try {
        auto version = semver::version::parse(version_str);
        return Constraint{op, version};
    } catch (const semver::semver_exception&) {
        return std::nullopt;
    }
}

// Parse a comparator set (space-separated constraints ANDed together)
std::optional<ComparatorSet> parse_comparator_set(const std::string& str) {
    auto tokens = tokenize(str);
    if (tokens.empty()) return std::nullopt;
    
    ComparatorSet set;
    for (const auto& token : tokens) {
        auto constraint = parse_constraint(token);
        if (!constraint) return std::nullopt;
        set.push_back(*constraint);
    }
    return set;
}

} // namespace

std::optional<Version> VersionRange::min_version() const {
    std::optional<Version> min;
    
    for (const auto& set : sets) {
        for (const auto& constraint : set) {
            // Only consider >= and = as defining a minimum
            if (constraint.op == Comparator::Ge || constraint.op == Comparator::Eq) {
                if (!min || constraint.version < *min) {
                    min = constraint.version;
                }
            }
            // > also defines a minimum (exclusive, but still a lower bound)
            if (constraint.op == Comparator::Gt) {
                if (!min || constraint.version < *min) {
                    min = constraint.version;
                }
            }
        }
    }
    
    return min;
}

std::string VersionRange::selection_key() const {
    auto min = min_version();
    if (!min) return "";
    return std::to_string(min->major()) + "." + std::to_string(min->minor());
}

std::optional<Version> parse_version(const std::string& str) {
    std::string s = trim(str);
    if (s.empty()) return std::nullopt;
    
    try {
        return semver::version::parse(s);
    } catch (const semver::semver_exception&) {
        return std::nullopt;
    }
}

std::optional<VersionRange> parse_range(const std::string& str) {
    std::string s = trim(str);
    if (s.empty()) return std::nullopt;
    
    // Split by || for OR
    auto or_parts = split(s, "||");
    
    VersionRange range;
    for (const auto& part : or_parts) {
        auto set = parse_comparator_set(trim(part));
        if (!set) return std::nullopt;
        range.sets.push_back(*set);
    }
    
    if (range.sets.empty()) return std::nullopt;
    return range;
}

bool satisfies(const Version& version, const Constraint& constraint) {
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

bool satisfies(const Version& version, const ComparatorSet& set) {
    // All constraints in a set must be satisfied (AND)
    for (const auto& constraint : set) {
        if (!satisfies(version, constraint)) {
            return false;
        }
    }
    return true;
}

bool satisfies(const Version& version, const VersionRange& range) {
    // Any set in the range must be satisfied (OR)
    for (const auto& set : range.sets) {
        if (satisfies(version, set)) {
            return true;
        }
    }
    return false;
}

} // namespace nah
