#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
assume_yes=false
version=""

usage() {
    echo "Usage: $0 [-y|--yes] <version>"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -y|--yes) assume_yes=true; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) usage >&2; exit 2 ;;
        *)
            [[ -z "$version" ]] || { usage >&2; exit 2; }
            version="$1"
            shift
            ;;
    esac
done

[[ "$version" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z.-]+)?(\+[0-9A-Za-z.-]+)?$ ]] || {
    echo "Version must be SemVer: MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]" >&2
    exit 2
}

[[ -z "$(git -C "$root_dir" status --porcelain)" ]] || {
    echo "Working tree must be clean" >&2
    exit 1
}
git -C "$root_dir" rev-parse "v$version" >/dev/null 2>&1 && {
    echo "Tag v$version already exists" >&2
    exit 1
}

check_dir="$(mktemp -d)"
trap 'cmake -E remove_directory "$check_dir"' EXIT
cmake -S "$root_dir" -B "$check_dir" -DCMAKE_BUILD_TYPE=Release -DNAH_ENABLE_TESTS=ON
cmake --build "$check_dir" --parallel
ctest --test-dir "$check_dir" --output-on-failure

if [[ "$assume_yes" != true ]]; then
    read -r -p "Create and push release v$version? [y/N] " reply
    [[ "$reply" =~ ^[Yy]$ ]] || exit 0
fi

printf '%s\n' "$version" > "$root_dir/VERSION"
git -C "$root_dir" add VERSION
git -C "$root_dir" commit -m "Release v$version"
git -C "$root_dir" tag -a "v$version" -m "Release v$version"
git -C "$root_dir" push origin main
git -C "$root_dir" push origin "v$version"
