#!/usr/bin/env bash
# Set up a NAH host with NAKs and apps installed
#
# Usage:
#   ./scripts/setup_host.sh [--clean] [--root <path>]
#
# Prerequisites:
#   - Run build_all.sh first to build NAKs and apps
#   - NAH CLI must be available
#
# Environment variables:
#   NAH_CLI   - Path to nah CLI
#   NAH_ROOT  - NAH root directory (default: demo_nah_root)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

EXAMPLES_DIR="$(find_examples_dir)"

# Defaults
export NAH_ROOT="${NAH_ROOT:-$EXAMPLES_DIR/demo_nah_root}"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --clean) CLEAN_ROOT=1; shift ;;
        --root) NAH_ROOT="$2"; shift 2 ;;
        --help|-h)
            echo "Usage: $0 [--clean] [--root <path>]"
            echo "  --clean       Remove existing NAH root before setup"
            echo "  --root <path> NAH root directory (default: demo_nah_root)"
            exit 0
            ;;
        *) shift ;;
    esac
done

# Find NAH CLI
find_nah_cli || {
    log_error "NAH CLI not found. Set NAH_CLI environment variable."
    exit 1
}

echo "=============================================="
echo "  NAH Host Setup Script"
echo "=============================================="
echo ""
log_info "NAH CLI: $NAH_CLI"
log_info "NAH Root: $NAH_ROOT"
echo ""

# Clean if requested
if [ "${CLEAN_ROOT:-0}" = "1" ] && [ -d "$NAH_ROOT" ]; then
    log_info "Cleaning existing NAH root..."
    rm -rf "$NAH_ROOT"
fi

# Create NAH root structure
log_info "Creating NAH root structure..."
mkdir -p "$NAH_ROOT"/{apps,naks,host/profiles,registry/{installs,naks}}

# Copy host profiles
if [ -d "$EXAMPLES_DIR/host/profiles" ]; then
    cp -r "$EXAMPLES_DIR/host/profiles"/* "$NAH_ROOT/host/profiles/" 2>/dev/null || true
    log_success "Copied host profiles"
fi

# Install NAKs
echo ""
log_info "Installing NAKs..."

install_nak "$EXAMPLES_DIR/sdk/build/com.example.sdk-1.2.3.nak" "Framework SDK" || true
install_nak "$EXAMPLES_DIR/conan-sdk/build/build/Release/com.example.gameengine-1.0.0.nak" "Game Engine SDK" || true

# Install apps
echo ""
log_info "Installing apps..."

install_nap "$EXAMPLES_DIR/apps/app/build/com.example.app-1.0.0.nap" "app" || true
install_nap "$EXAMPLES_DIR/apps/app_c/build/com.example.app_c-1.0.0.nap" "app_c" || true
install_nap "$EXAMPLES_DIR/apps/game-app/build/com.example.mygame-1.0.0.nap" "game-app" || true

# Summary
echo ""
echo "=============================================="
log_success "NAH Host setup complete!"
echo "=============================================="
echo ""
echo "Installed NAKs:"
$NAH_CLI --root "$NAH_ROOT" nak list 2>/dev/null || echo "  (use 'nah nak list' to view)"
echo ""
echo "Installed Apps:"
$NAH_CLI --root "$NAH_ROOT" app list 2>/dev/null || echo "  (use 'nah app list' to view)"
echo ""
echo "Next: ./scripts/run_apps.sh"
