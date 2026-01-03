#pragma once

#include <string>

namespace nah {

enum class PathError {
    None,
    ContainsNul,
    AbsoluteNotAllowed,
    EscapesRoot,
};

struct PathResult {
    bool ok;
    std::string path;  // normalized absolute path when ok
    PathError error;
};

// Normalize a path relative to a root without following symlinks (string-based).
// - Rejects NUL bytes
// - Rejects absolute relative_path when allow_absolute is false
// - Collapses "." and ".." segments
// - Fails if resulting path would escape root
PathResult normalize_under_root(const std::string& root,
                                const std::string& relative_path,
                                bool allow_absolute = false);

} // namespace nah

