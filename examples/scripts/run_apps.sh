#!/usr/bin/env bash
# Run NAH-managed apps
#
# Usage:
#   ./scripts/run_apps.sh [app_id]     # Run specific app or all apps
#   ./scripts/run_apps.sh --contract   # Show contracts only (don't run)
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
CONTRACT_ONLY=0
SPECIFIC_APP=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --contract) CONTRACT_ONLY=1; shift ;;
        --root) NAH_ROOT="$2"; shift 2 ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS] [app_id]"
            echo ""
            echo "Options:"
            echo "  --contract    Show launch contracts only (don't run)"
            echo "  --root <path> NAH root directory"
            echo ""
            echo "Examples:"
            echo "  $0                        # Run all apps"
            echo "  $0 com.example.app        # Run specific app"
            echo "  $0 --contract             # Show all contracts"
            exit 0
            ;;
        com.*) SPECIFIC_APP="$1"; shift ;;
        *) shift ;;
    esac
done

# Find NAH CLI
find_nah_cli || {
    log_error "NAH CLI not found. Set NAH_CLI environment variable."
    exit 1
}

echo "=============================================="
echo "  NAH App Runner"
echo "=============================================="
echo ""
log_info "NAH CLI: $NAH_CLI"
log_info "NAH Root: $NAH_ROOT"

# Check NAH root exists
if [ ! -d "$NAH_ROOT" ]; then
    log_error "NAH root not found. Run setup_host.sh first."
    exit 1
fi

# Get list of installed apps
get_installed_apps() {
    $NAH_CLI --root "$NAH_ROOT" app list 2>/dev/null | grep -E '^com\.' || true
}

# Show contract for an app
show_contract() {
    local app_id="$1"
    log_header "Contract: $app_id"
    $NAH_CLI --root "$NAH_ROOT" contract show "$app_id" 2>&1 || {
        log_warn "Could not show contract for $app_id"
    }
}

# Run an app via NAH
run_app() {
    local app_id="$1"
    log_header "Running: $app_id"

    # Show what NAH will do
    echo "Launch contract:"
    $NAH_CLI --root "$NAH_ROOT" contract show "$app_id" 2>&1 | head -20 || true
    echo ""

    # Actually run the app
    log_info "Launching via NAH..."
    $NAH_CLI --root "$NAH_ROOT" run "$app_id" 2>&1 || {
        log_warn "App exited with non-zero status"
    }
    echo ""
}

# Main logic
if [ -n "$SPECIFIC_APP" ]; then
    # Run/show specific app
    if [ $CONTRACT_ONLY -eq 1 ]; then
        show_contract "$SPECIFIC_APP"
    else
        run_app "$SPECIFIC_APP"
    fi
else
    # Run/show all apps
    APPS=$(get_installed_apps)

    if [ -z "$APPS" ]; then
        log_error "No apps installed. Run setup_host.sh first."
        exit 1
    fi

    echo ""
    log_info "Installed apps:"
    echo "$APPS"
    echo ""

    for app_id in $APPS; do
        if [ $CONTRACT_ONLY -eq 1 ]; then
            show_contract "$app_id"
        else
            run_app "$app_id"
        fi
    done
fi

echo ""
log_success "Done!"
