#!/usr/bin/env bash
#
# NAH Release Script
# ==================
# Updates version across the codebase and creates a git tag.
#
# Usage:
#   ./scripts/release.sh <version>
#   ./scripts/release.sh 1.2.0
#   ./scripts/release.sh 2.0.0-beta.1
#
# This script:
#   1. Validates the version format (SemVer 2.0.0)
#   2. Updates VERSION file
#   3. Updates CMakeLists.txt
#   4. Updates conanfile.py
#   5. Commits changes
#   6. Creates and pushes a git tag
#
# The tag push triggers GitHub Actions to build and publish the release.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Validate SemVer format
validate_version() {
    local version="$1"
    # SemVer 2.0.0 regex (simplified but covers most cases)
    if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9]+(\.[a-zA-Z0-9]+)*)?(\+[a-zA-Z0-9]+(\.[a-zA-Z0-9]+)*)?$ ]]; then
        error "Invalid version format: $version\nExpected SemVer 2.0.0 format: MAJOR.MINOR.PATCH[-prerelease][+build]"
    fi
}

# Get current version
get_current_version() {
    cat "$ROOT_DIR/VERSION" | tr -d '[:space:]'
}

# Update VERSION file
update_version_file() {
    local version="$1"
    echo "$version" > "$ROOT_DIR/VERSION"
    info "Updated VERSION file"
}

# Update CMakeLists.txt
update_cmake() {
    local version="$1"
    local cmake_file="$ROOT_DIR/CMakeLists.txt"

    # Extract just MAJOR.MINOR.PATCH for CMake (no prerelease/build metadata)
    local cmake_version="${version%%-*}"  # Remove prerelease
    cmake_version="${cmake_version%%+*}"  # Remove build metadata

    sed -i.bak "s/^project(nah VERSION [0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*/project(nah VERSION $cmake_version/" "$cmake_file"
    rm -f "$cmake_file.bak"
    info "Updated CMakeLists.txt (version: $cmake_version)"
}

# Update conanfile.py
update_conan() {
    local version="$1"
    local conan_file="$ROOT_DIR/conanfile.py"

    sed -i.bak "s/^    version = \"[^\"]*\"/    version = \"$version\"/" "$conan_file"
    rm -f "$conan_file.bak"
    info "Updated conanfile.py"
}

# Update package.json
update_npm() {
    local version="$1"
    local npm_file="$ROOT_DIR/package.json"

    if [[ -f "$npm_file" ]]; then
        sed -i.bak "s/\"version\": \"[^\"]*\"/\"version\": \"$version\"/" "$npm_file"
        rm -f "$npm_file.bak"
        info "Updated package.json"
    fi
}

# Check for uncommitted changes
check_clean_working_tree() {
    if ! git -C "$ROOT_DIR" diff --quiet HEAD 2>/dev/null; then
        error "Working tree has uncommitted changes. Please commit or stash them first."
    fi
}

# Check if tag already exists
check_tag_exists() {
    local version="$1"
    if git -C "$ROOT_DIR" rev-parse "v$version" >/dev/null 2>&1; then
        error "Tag v$version already exists"
    fi
}

# Main
main() {
    local skip_confirm=false
    local new_version=""

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -y|--yes)
                skip_confirm=true
                shift
                ;;
            -*)
                echo "Unknown option: $1"
                exit 1
                ;;
            *)
                new_version="$1"
                shift
                ;;
        esac
    done

    if [[ -z "$new_version" ]]; then
        echo "Usage: $0 [-y|--yes] <version>"
        echo "Example: $0 1.2.0"
        echo "         $0 --yes 2.0.0-beta.1"
        exit 1
    fi

    local current_version
    current_version=$(get_current_version)

    info "NAH Release Script"
    echo "  Current version: $current_version"
    echo "  New version:     $new_version"
    echo

    # Validations
    validate_version "$new_version"
    check_clean_working_tree
    check_tag_exists "$new_version"

    # Confirm
    if [[ "$skip_confirm" != "true" ]]; then
        read -p "Proceed with release v$new_version? [y/N] " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            info "Aborted"
            exit 0
        fi
    fi

    # Update files
    update_version_file "$new_version"
    update_cmake "$new_version"
    update_conan "$new_version"
    update_npm "$new_version"

    # Commit
    info "Committing version bump..."
    git -C "$ROOT_DIR" add VERSION CMakeLists.txt conanfile.py package.json
    git -C "$ROOT_DIR" commit -m "Release v$new_version"

    # Tag
    info "Creating tag v$new_version..."
    git -C "$ROOT_DIR" tag -a "v$new_version" -m "Release v$new_version"

    # Push
    info "Pushing to origin..."
    git -C "$ROOT_DIR" push origin main
    git -C "$ROOT_DIR" push origin "v$new_version"

    echo
    info "Release v$new_version complete!"
    echo
    echo "GitHub Actions will now:"
    echo "  1. Build binaries for Linux, macOS, and Windows"
    echo "  2. Create a GitHub Release with the binaries"
    echo "  3. Auto-generate release notes"
    echo
    echo "Monitor progress at: https://github.com/rtorr/nah/actions"
}

main "$@"
