#!/usr/bin/env bash
#
# Run Apps
# ========
# Run installed applications or show their launch contracts.
#
# Usage:
#   ./run.sh [options] [app_id]
#
# Options:
#   --contract    Show launch contract instead of running
#   --profile     Use specific profile
#   --list        List installed apps

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOST_MANIFEST="$SCRIPT_DIR/host.toml"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info()    { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[OK]${NC} $1"; }
log_error()   { echo -e "${RED}[ERROR]${NC} $1"; }

# Read NAH root from host.toml
get_toml_value() {
    grep "^${2}[[:space:]]*=" "$1" 2>/dev/null | sed 's/.*=[[:space:]]*"\{0,1\}\([^"]*\)"\{0,1\}/\1/' | head -1
}

NAH_ROOT_REL=$(get_toml_value "$HOST_MANIFEST" "nah_root")
if [[ "$NAH_ROOT_REL" == /* ]]; then
    NAH_ROOT="$NAH_ROOT_REL"
else
    NAH_ROOT="$SCRIPT_DIR/${NAH_ROOT_REL#./}"
fi

# Find NAH CLI
find_nah_cli() {
    if [ -n "$NAH_CLI" ] && [ -x "$NAH_CLI" ]; then
        return 0
    fi
    for path in \
        "$SCRIPT_DIR/../../build/tools/nah/nah" \
        "/usr/local/bin/nah" \
        "/usr/bin/nah" \
        "$(command -v nah 2>/dev/null)"
    do
        if [ -x "$path" ]; then
            NAH_CLI="$path"
            return 0
        fi
    done
    return 1
}

if ! find_nah_cli; then
    log_error "NAH CLI not found"
    exit 1
fi

# Parse arguments
SHOW_CONTRACT=0
PROFILE=""
LIST_APPS=0
APP_ID=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --contract|-c) SHOW_CONTRACT=1; shift ;;
        --profile|-p) PROFILE="$2"; shift 2 ;;
        --list|-l) LIST_APPS=1; shift ;;
        --help|-h)
            echo "Usage: $0 [options] [app_id]"
            echo ""
            echo "Options:"
            echo "  --contract, -c    Show launch contract instead of running"
            echo "  --profile, -p     Use specific profile"
            echo "  --list, -l        List installed apps"
            exit 0
            ;;
        -*) log_error "Unknown option: $1"; exit 1 ;;
        *) APP_ID="$1"; shift ;;
    esac
done

# Verify NAH root exists
if [ ! -d "$NAH_ROOT" ]; then
    log_error "NAH root not found: $NAH_ROOT"
    log_info "Run ./setup.sh first"
    exit 1
fi

# List apps
if [ "$LIST_APPS" = "1" ]; then
    echo "Installed Applications:"
    echo ""
    for app_dir in "$NAH_ROOT"/apps/*/; do
        if [ -d "$app_dir" ]; then
            app_name=$(basename "$app_dir")
            echo "  $app_name"
        fi
    done
    exit 0
fi

# Build profile args
PROFILE_ARGS=""
if [ -n "$PROFILE" ]; then
    PROFILE_ARGS="--profile $PROFILE"
fi

# Get list of apps to run
if [ -n "$APP_ID" ]; then
    APPS=("$APP_ID")
else
    APPS=()
    for app_dir in "$NAH_ROOT"/apps/*/; do
        if [ -d "$app_dir" ]; then
            APPS+=("$(basename "$app_dir")")
        fi
    done
fi

if [ ${#APPS[@]} -eq 0 ]; then
    log_error "No apps installed"
    exit 1
fi

# Run or show contract for each app
for app in "${APPS[@]}"; do
    # Parse app id and version from directory name (format: id-version)
    app_id="${app%-*}"
    app_version="${app##*-}"

    # Handle apps without version in name
    if [ "$app_id" = "$app_version" ]; then
        app_version=""
    fi

    echo ""
    echo "=============================================="
    echo "  $app"
    echo "=============================================="
    echo ""

    if [ "$SHOW_CONTRACT" = "1" ]; then
        "$NAH_CLI" --root "$NAH_ROOT" contract show \
            --app "$app_id" \
            ${app_version:+--version "$app_version"} \
            $PROFILE_ARGS 2>&1 || true
    else
        "$NAH_CLI" --root "$NAH_ROOT" app launch \
            "$app_id" \
            ${app_version:+--version "$app_version"} \
            $PROFILE_ARGS 2>&1 || true
    fi
done
