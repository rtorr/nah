#!/usr/bin/env bash
# Run NAH-managed apps
#
# This script demonstrates how a host uses NAH:
# 1. Get the launch contract from NAH
# 2. Set up environment variables
# 3. Set library paths
# 4. Execute the binary
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

# Get list of installed apps (just the app id@version, not the path)
get_installed_apps() {
    $NAH_CLI --root "$NAH_ROOT" app list 2>/dev/null | grep -oE '^[^ ]+' || true
}

# Show contract for an app
show_contract() {
    local app_id="$1"
    echo ""
    echo "=== Contract: $app_id ==="
    echo ""
    $NAH_CLI --root "$NAH_ROOT" contract show "$app_id" 2>&1 || {
        log_warn "Could not show contract for $app_id"
    }
}

# Run an app via the launch contract (this is what a host does)
run_app() {
    local app_id="$1"
    echo ""
    echo "=== Running: $app_id ==="
    echo ""

    # Get contract as JSON
    local contract_json
    contract_json=$($NAH_CLI --root "$NAH_ROOT" --json contract show "$app_id" 2>&1) || {
        log_error "Failed to get contract for $app_id"
        echo "$contract_json"
        return 1
    }

    # Check for critical error
    local critical_error
    critical_error=$(echo "$contract_json" | jq -r '.critical_error // empty')
    if [ -n "$critical_error" ] && [ "$critical_error" != "null" ]; then
        log_error "Critical error: $critical_error"
        return 1
    fi

    # Extract contract fields
    local binary cwd lib_path_key
    binary=$(echo "$contract_json" | jq -r '.execution.binary')
    cwd=$(echo "$contract_json" | jq -r '.execution.cwd')
    lib_path_key=$(echo "$contract_json" | jq -r '.execution.library_path_env_key')

    # Build library path
    local lib_paths
    lib_paths=$(echo "$contract_json" | jq -r '.execution.library_paths | join(":")')

    # Build arguments array
    local args=()
    while IFS= read -r arg; do
        args+=("$arg")
    done < <(echo "$contract_json" | jq -r '.execution.arguments[]')

    # Extract and set environment variables
    local env_vars=()
    while IFS='=' read -r key value; do
        if [ -n "$key" ]; then
            env_vars+=("$key=$value")
        fi
    done < <(echo "$contract_json" | jq -r '.environment | to_entries | .[] | "\(.key)=\(.value)"')

    # Show what we're about to do
    log_info "Binary: $binary"
    log_info "CWD: $cwd"
    log_info "Args: ${args[*]}"
    log_info "$lib_path_key: $lib_paths"
    echo ""

    # Execute the app
    # Set environment, library path, change to cwd, and exec
    (
        cd "$cwd"
        for env_var in "${env_vars[@]}"; do
            export "${env_var?}"
        done
        export "$lib_path_key=$lib_paths"
        exec "$binary" "${args[@]}"
    ) || {
        log_warn "App exited with non-zero status: $?"
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
    for app in $APPS; do
        echo "  $app"
    done
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
