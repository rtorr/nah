#!/usr/bin/env bash
#
# Host Setup Script
# =================
# Uses `nah host install` to set up the NAH root from host.json.
#
# Usage:
#   ./setup.sh [--clean]
#
# Prerequisites:
#   - NAK and NAP packages must already be built
#   - NAH CLI must be available

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# =============================================================================
# Find NAH CLI
# =============================================================================

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

# =============================================================================
# Main
# =============================================================================

CLEAN_FLAG=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean) CLEAN_FLAG="--clean"; shift ;;
        --help|-h)
            echo "Usage: $0 [--clean]"
            echo ""
            echo "Options:"
            echo "  --clean    Remove existing NAH root before setup"
            echo ""
            echo "Prerequisites:"
            echo "  NAK and NAP packages must already be built."
            echo "  Run examples/scripts/build_all.sh first."
            exit 0
            ;;
        *) shift ;;
    esac
done

# Find NAH CLI
if ! find_nah_cli; then
    echo "Error: NAH CLI not found. Build NAH first or set NAH_CLI."
    exit 1
fi

echo "Using NAH CLI: $NAH_CLI"
echo ""

# Run nah host install
exec "$NAH_CLI" host install "$SCRIPT_DIR" $CLEAN_FLAG
