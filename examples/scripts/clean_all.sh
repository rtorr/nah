#!/usr/bin/env bash
# Clean all build artifacts from examples
#
# Usage:
#   ./scripts/clean_all.sh [--all]
#
# Options:
#   --all   Also remove demo_nah_root

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXAMPLES_DIR="$(dirname "$SCRIPT_DIR")"

BLUE='\033[0;34m'
GREEN='\033[0;32m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[OK]${NC} $1"; }

CLEAN_ROOT=0
for arg in "$@"; do
    case $arg in
        --all) CLEAN_ROOT=1 ;;
    esac
done

echo "=============================================="
echo "  NAH Examples Clean Script"
echo "=============================================="
echo ""

cd "$EXAMPLES_DIR"

# Clean SDK
if [ -d "sdk/build" ]; then
    log_info "Cleaning sdk/build..."
    rm -rf sdk/build
fi

# Clean Conan SDK
if [ -d "conan-sdk/build" ]; then
    log_info "Cleaning conan-sdk/build..."
    rm -rf conan-sdk/build
fi
if [ -f "conan-sdk/CMakeUserPresets.json" ]; then
    rm -f conan-sdk/CMakeUserPresets.json
fi

# Clean apps
for app_dir in apps/*/; do
    if [ -d "${app_dir}build" ]; then
        log_info "Cleaning ${app_dir}build..."
        rm -rf "${app_dir}build"
    fi
done

# Clean host examples
if [ -d "host/build" ]; then
    log_info "Cleaning host/build..."
    rm -rf host/build
fi

# Clean demo_nah_root if requested
if [ $CLEAN_ROOT -eq 1 ] && [ -d "demo_nah_root" ]; then
    log_info "Cleaning demo_nah_root..."
    rm -rf demo_nah_root
fi

echo ""
log_success "Clean complete!"
