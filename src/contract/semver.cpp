#include "nah/semver.hpp"

#include <cctype>
#include <sstream>
#include <vector>

namespace nah {

namespace {

std::string trim(const std::string& in) {
    size_t start = 0;
    while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) ++start;
    size_t end = in.size();
    while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) --end;
    return in.substr(start, end - start);
}

std::optional<int> to_int(const std::string& s) {
    if (s.empty()) return std::nullopt;
    int val = 0;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return std::nullopt;
        val = val * 10 + (c - '0');
    }
    return val;
}

std::optional<SemVer> parse_core_version(const std::string& s) {
    // Reject pre-release/build metadata
    if (s.find_first_of("+-") != std::string::npos) return std::nullopt;
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string segment;
    while (std::getline(ss, segment, '.')) {
        parts.push_back(segment);
    }
    if (parts.size() != 3) return std::nullopt;
    auto mj = to_int(parts[0]);
    auto mn = to_int(parts[1]);
    auto pt = to_int(parts[2]);
    if (!mj || !mn || !pt) return std::nullopt;
    return SemVer{*mj, *mn, *pt};
}

SemVer make_upper_for_caret(const SemVer& v) {
    if (v.major > 0) return SemVer{v.major + 1, 0, 0};
    if (v.major == 0 && v.minor > 0) return SemVer{0, v.minor + 1, 0};
    return SemVer{0, 0, v.patch + 1};
}

} // namespace

std::optional<SemVer> parse_version(const std::string& str) {
    return parse_core_version(trim(str));
}

std::optional<SemVerRequirement> parse_requirement(const std::string& input) {
    const std::string s = trim(input);
    if (s.empty()) return std::nullopt;

    // bounded >=X.Y.Z <A.B.C
    if (s.rfind(">=", 0) == 0) {
        auto rest = s.substr(2);
        auto lt_pos = rest.find('<');
        if (lt_pos == std::string::npos) return std::nullopt;
        std::string lower_str = trim(rest.substr(0, lt_pos));
        std::string upper_str = trim(rest.substr(lt_pos + 1));
        auto lower = parse_core_version(lower_str);
        auto upper = parse_core_version(upper_str);
        if (!lower || !upper) return std::nullopt;
        return SemVerRequirement{RequirementKind::Bounded, *lower, *upper,
                                 std::to_string(lower->major) + "." + std::to_string(lower->minor)};
    }

    // caret
    if (s.rfind("^", 0) == 0) {
        auto v = parse_core_version(s.substr(1));
        if (!v) return std::nullopt;
        SemVer upper = make_upper_for_caret(*v);
        return SemVerRequirement{RequirementKind::Caret, *v, upper,
                                 std::to_string(v->major) + "." + std::to_string(v->minor)};
    }

    // tilde
    if (s.rfind("~", 0) == 0) {
        auto v = parse_core_version(s.substr(1));
        if (!v) return std::nullopt;
        SemVer upper{v->major, v->minor + 1, 0};
        return SemVerRequirement{RequirementKind::Tilde, *v, upper,
                                 std::to_string(v->major) + "." + std::to_string(v->minor)};
    }

    // wildcard X.Y.* or X.Y.x
    if (s.size() >= 4) {
        bool is_star = s.size() > 0 && s.back() == '*';
        bool is_x = s.size() > 1 && s.compare(s.size() - 2, 2, ".x") == 0;
        if (is_star || is_x) {
            std::string base = s.substr(0, s.size() - (is_star ? 1 : 2));
            if (!base.empty() && base.back() == '.') base.pop_back();
            auto v = parse_core_version(base + ".0");
            if (!v) return std::nullopt;
            return SemVerRequirement{RequirementKind::Wildcard, *v, SemVer{v->major, v->minor + 1, 0},
                                     std::to_string(v->major) + "." + std::to_string(v->minor)};
        }
    }

    // exact X.Y.Z
    if (auto v = parse_core_version(s)) {
        return SemVerRequirement{RequirementKind::Exact, *v, *v,
                                 std::to_string(v->major) + "." + std::to_string(v->minor)};
    }

    return std::nullopt;
}

bool satisfies(const SemVer& version, const SemVerRequirement& req) {
    switch (req.kind) {
        case RequirementKind::Exact:
            return version == req.lower;
        case RequirementKind::Wildcard:
            return version.major == req.lower.major && version.minor == req.lower.minor;
        case RequirementKind::Caret:
        case RequirementKind::Tilde:
        case RequirementKind::Bounded:
            return !(version < req.lower) && (version < req.upper);
        default:
            return false;
    }
}

} // namespace nah
