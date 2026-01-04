#include "nah/path_utils.hpp"
#include "nah/platform.hpp"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace nah {

namespace {

bool contains_nul(const std::string& s) {
    return s.find('\0') != std::string::npos;
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::string current;
    std::istringstream ss(s);
    while (std::getline(ss, current, delim)) {
        parts.push_back(current);
    }
    return parts;
}

std::string join_components(const std::string& root, const std::vector<std::string>& comps) {
    std::filesystem::path p(root);
    for (const auto& c : comps) {
        p /= c;
    }
    // Always use forward slashes for portable paths
    return to_portable_path(p.lexically_normal().string());
}

} // namespace

PathResult normalize_under_root(const std::string& root,
                                const std::string& relative_path,
                                bool allow_absolute) {
    if (contains_nul(root) || contains_nul(relative_path)) {
        return {false, {}, PathError::ContainsNul};
    }

    std::vector<std::string> components;

    // Determine if input is absolute
    if (!relative_path.empty() && (relative_path[0] == '/' || relative_path[0] == '\\')) {
        if (!allow_absolute) {
            return {false, {}, PathError::AbsoluteNotAllowed};
        }
        // Strip leading separator for normalization
        auto trimmed = relative_path;
        while (!trimmed.empty() && (trimmed[0] == '/' || trimmed[0] == '\\')) {
            trimmed.erase(trimmed.begin());
        }
        for (auto& part : split(trimmed, '/')) {
            if (!part.empty()) components.push_back(part);
        }
    } else {
        for (auto& part : split(relative_path, '/')) {
            components.push_back(part);
        }
    }

    std::vector<std::string> normalized;
    for (const auto& part : components) {
        if (part.empty() || part == ".") {
            continue;
        }
        if (part == "..") {
            if (normalized.empty()) {
                return {false, {}, PathError::EscapesRoot};
            }
            normalized.pop_back();
        } else {
            normalized.push_back(part);
        }
    }

    std::string out = join_components(root, normalized);
    // Ensure containment: lexically compare without touching filesystem.
    std::filesystem::path root_path(root);
    std::filesystem::path out_path(out);
    auto lex_root = root_path.lexically_normal();
    auto lex_out = out_path.lexically_normal();
    auto root_it = lex_root.begin();
    auto out_it = lex_out.begin();
    for (; root_it != lex_root.end() && out_it != lex_out.end(); ++root_it, ++out_it) {
        if (*root_it != *out_it) {
            return {false, {}, PathError::EscapesRoot};
        }
    }
    if (root_it != lex_root.end()) {
        return {false, {}, PathError::EscapesRoot};
    }

    return {true, out, PathError::None};
}

} // namespace nah

