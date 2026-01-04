#!/usr/bin/env bash
# Build all NAH examples (NAKs and apps)
#
# Usage:
#   ./scripts/build_all.sh [--clean]
#
# Prerequisites:
#   - NAH CLI must be installed or NAH_CLI environment variable set
#   - For conan-sdk: Conan 2.x must be installed
#
# Environment variables:
#   NAH_CLI     - Path to nah CLI (default: searches PATH, then common locations)
#   SKIP_CONAN  - Set to 1 to skip conan-sdk build
#   CLEAN       - Set to 1 to clean before building

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

EXAMPLES_DIR="$(find_examples_dir)"

# Parse arguments
for arg in "$@"; do
    case $arg in
        --clean) export CLEAN=1 ;;
        --help|-h)
            echo "Usage: $0 [--clean]"
            echo "  --clean  Remove build directories before building"
            exit 0
            ;;
    esac
done

# Find NAH CLI
find_nah_cli || {
    log_error "NAH CLI not found. Set NAH_CLI environment variable or ensure 'nah' is in PATH."
    exit 1
}
log_info "Using NAH CLI: $NAH_CLI"

echo "=============================================="
echo "  NAH Examples Build Script"
echo "=============================================="
echo ""

# 1. Build SDK NAK
build_cmake_project "$EXAMPLES_DIR/sdk" "Framework SDK (NAK)" || exit 1
(cd "$EXAMPLES_DIR/sdk/build" && cmake --build . --target package_nak)

# 2. Build Conan SDK NAK
CONAN_SDK_BUILT=0
if build_conan_project "$EXAMPLES_DIR/conan-sdk" "Game Engine SDK (NAK)"; then
    (cd "$EXAMPLES_DIR/conan-sdk" && cmake --build build/build/Release --target package_nak)
    CONAN_SDK_BUILT=1
fi

# 3. Build apps
for app_dir in "$EXAMPLES_DIR"/apps/app "$EXAMPLES_DIR"/apps/app_c; do
    if [ -d "$app_dir" ]; then
        app_name=$(basename "$app_dir")
        build_cmake_project "$app_dir" "App: $app_name" || exit 1
        (cd "$app_dir/build" && cmake --build . --target package_nap 2>/dev/null || true)
    fi
done

# 4. Build game-app (only if conan-sdk was built)
if [ "$CONAN_SDK_BUILT" = "1" ]; then
    build_cmake_project "$EXAMPLES_DIR/apps/game-app" "App: game-app" \
        "-DGAMEENGINE_SDK_DIR=$EXAMPLES_DIR/conan-sdk/build/build/Release" || exit 1
    (cd "$EXAMPLES_DIR/apps/game-app/build" && cmake --build . --target package_nap 2>/dev/null || true)
else
    log_warn "Skipping game-app (conan-sdk not built)"
fi

echo ""
echo "=============================================="
log_success "All examples built successfully!"
echo "=============================================="
echo ""
echo "Next steps:"
echo "  1. Run: ./scripts/setup_host.sh    # Install NAKs and apps"
echo "  2. Run: ./scripts/run_apps.sh      # Launch apps via NAH"
