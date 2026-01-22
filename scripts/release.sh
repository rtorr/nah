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
#   2. Runs all tests (unit + integration)
#   3. Updates VERSION file
#   4. Updates CMakeLists.txt
#   5. Updates conanfile.py
#   6. Reconfigures CMake (so local builds use new version)
#   7. Commits changes
#   8. Creates and pushes a git tag
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
    # CMakeLists.txt reads from VERSION file, so nothing to update
    info "CMakeLists.txt reads version from VERSION file (no update needed)"
}

# Update conanfile.py
update_conan() {
    # conanfile.py reads from VERSION file, so nothing to update
    info "conanfile.py reads version from VERSION file (no update needed)"
}

# Update package.json
update_npm() {
    # Package.json no longer exists - NAH is not published to npm
    return 0
}

# Reconfigure CMake to pick up new version
reconfigure_cmake() {
    local build_dir="$ROOT_DIR/build"
    if [[ -d "$build_dir" ]]; then
        info "Reconfiguring CMake build..."
        cmake -B "$build_dir" -S "$ROOT_DIR" >/dev/null 2>&1 && \
            info "CMake reconfigured (local builds will use new version)" || \
            warn "CMake reconfigure failed (run 'cmake -B build' manually)"
    fi
}

# Run all tests
run_tests() {
    local build_dir="$ROOT_DIR/build"
    
    if [[ ! -d "$build_dir" ]]; then
        error "Build directory not found. Run 'cmake -B build && make' first."
    fi
    
    info "Building project..."
    if ! make -C "$build_dir" -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) >/dev/null 2>&1; then
        error "Build failed. Fix build errors before releasing."
    fi
    
    info "Running unit tests..."
    if [[ -f "$build_dir/tests/unit/nah-tests" ]]; then
        if ! "$build_dir/tests/unit/nah-tests" --no-colors; then
            error "Unit tests failed. Fix failing tests before releasing."
        fi
        info "✓ Unit tests passed"
    else
        warn "Unit test binary not found (skipping)"
    fi
    
    info "Running integration tests..."
    if [[ -f "$build_dir/tests/integration/integration_tests" ]]; then
        # Integration tests must run from build directory (they look for ./nah)
        if ! (cd "$build_dir/tools/nah" && ../../tests/integration/integration_tests --no-colors --quiet); then
            error "Integration tests failed. Fix failing tests before releasing."
        fi
        info "✓ Integration tests passed"
    else
        warn "Integration test binary not found (skipping)"
    fi
    
    info "✓ All tests passed"
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
    
    # Run tests before making any changes
    info "Running tests before release..."
    run_tests

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
    reconfigure_cmake

    # Commit
    info "Committing version bump..."
    git -C "$ROOT_DIR" add VERSION
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
