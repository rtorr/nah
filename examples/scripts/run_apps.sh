#!/usr/bin/env bash
# Run installed apps using NAH
#
# This script runs installed applications through the NAH runtime system.
# It demonstrates how NAH manages application execution with proper environment
# setup, library paths, and runtime resolution.
#
# Usage:
#   ./scripts/run_apps.sh [options] [app_id]
#
# Options:
#   --contract      Show contract details instead of running
#   --root <path>   NAH root directory
#
# Examples:
#   ./scripts/run_apps.sh                      # Run all apps
#   ./scripts/run_apps.sh com.example.app      # Run specific app
#   ./scripts/run_apps.sh --contract           # Show all contracts

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

# Default options
SHOW_CONTRACT=false
SPECIFIC_APP=""

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --contract) SHOW_CONTRACT=true; shift ;;
        --root) NAH_ROOT="$2"; shift 2 ;;
        --help|-h)
            echo "Usage: $0 [options] [app_id]"
            echo ""
            echo "Options:"
            echo "  --contract    Show contract details instead of running"
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
APPS=$($NAH_CLI --root "$NAH_ROOT" list --apps | grep -v "^Apps:" | grep -v "^No apps" | sed 's/^  //' | cut -d' ' -f1 || true)

if [ -z "$APPS" ]; then
    log_warning "No apps installed"
    exit 0
fi

log_info "Installed apps:"
for app in $APPS; do
    echo "  $app"
done

# Function to run a single app
run_app() {
    local app_id="$1"
    echo ""
    echo "=== Running: $app_id ==="
    echo ""

    # Simply use nah run to execute the app
    $NAH_CLI --root "$NAH_ROOT" run "$app_id"
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        log_info "App completed successfully"
    else
        log_error "App failed with exit code: $exit_code"
    fi

    return $exit_code
}

# Function to show contract for an app
show_contract() {
    local app_id="$1"
    echo ""
    echo "=== Contract for: $app_id ==="
    echo ""

    $NAH_CLI --root "$NAH_ROOT" --json show "$app_id" | python3 -m json.tool
}

# Main execution
if [ -n "$SPECIFIC_APP" ]; then
    # Run specific app
    if $SHOW_CONTRACT; then
        show_contract "$SPECIFIC_APP"
    else
        run_app "$SPECIFIC_APP"
    fi
else
    # Run all apps
    for app in $APPS; do
        if $SHOW_CONTRACT; then
            show_contract "$app"
        else
            run_app "$app" || log_warning "Failed to run $app"
        fi
    done
fi

echo ""
echo "=============================================="
echo "[OK] Done!"
echo "=============================================="