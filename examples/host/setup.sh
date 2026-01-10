#!/usr/bin/env bash
#
# Host Setup Script
# =================
# Reads host.json and installs pre-built NAKs and apps into the NAH root.
#
# Usage:
#   ./setup.sh [--clean]
#
# Prerequisites:
#   - NAK and NAP packages must already be built
#   - NAH CLI must be available

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOST_MANIFEST="$SCRIPT_DIR/host.json"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info()    { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[OK]${NC} $1"; }
log_warn()    { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error()   { echo -e "${RED}[ERROR]${NC} $1"; }

# =============================================================================
# Parse host.json using basic grep/sed (no jq dependency)
# =============================================================================

get_json_value() {
    local file="$1" key="$2"
    grep -o "\"${key}\"[[:space:]]*:[[:space:]]*\"[^\"]*\"" "$file" 2>/dev/null | sed 's/.*:[[:space:]]*"\([^"]*\)".*/\1/' | head -1
}

# Get count of array elements
get_json_array_count() {
    local file="$1" array="$2"
    # Count occurrences of "id" field within array blocks
    python3 -c "
import json
with open('$file') as f:
    data = json.load(f)
print(len(data.get('$array', [])))
" 2>/dev/null || echo 0
}

# Get field from array element by index (0-based)
get_json_array_field() {
    local file="$1" array="$2" index="$3" field="$4"
    python3 -c "
import json
with open('$file') as f:
    data = json.load(f)
arr = data.get('$array', [])
if $index < len(arr):
    val = arr[$index].get('$field', '')
    print(val if val is not None else '')
" 2>/dev/null || echo ""
}

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

CLEAN=0

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean) CLEAN=1; shift ;;
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

# Verify host.json exists
if [ ! -f "$HOST_MANIFEST" ]; then
    log_error "host.json not found at $HOST_MANIFEST"
    exit 1
fi

# Find NAH CLI
if ! find_nah_cli; then
    log_error "NAH CLI not found. Build NAH first or set NAH_CLI."
    exit 1
fi

# Read host configuration
HOST_NAME=$(get_json_value "$HOST_MANIFEST" "name")
NAH_ROOT_REL=$(get_json_value "$HOST_MANIFEST" "nah_root")
DEFAULT_PROFILE=$(get_json_value "$HOST_MANIFEST" "default_profile")

# Resolve NAH root path
if [[ "$NAH_ROOT_REL" == /* ]]; then
    NAH_ROOT="$NAH_ROOT_REL"
else
    NAH_ROOT="$SCRIPT_DIR/${NAH_ROOT_REL#./}"
fi

echo ""
echo "=============================================="
echo "  ${HOST_NAME:-Host} Setup"
echo "=============================================="
echo ""
log_info "Host manifest: $HOST_MANIFEST"
log_info "NAH CLI: $NAH_CLI"
log_info "NAH root: $NAH_ROOT"
echo ""

# Clean if requested
if [ "$CLEAN" = "1" ] && [ -d "$NAH_ROOT" ]; then
    log_info "Cleaning existing NAH root..."
    rm -rf "$NAH_ROOT"
fi

# Create NAH root structure
log_info "Creating NAH root structure..."
mkdir -p "$NAH_ROOT"/{apps,naks,host/profiles,registry/{installs,naks}}

# Copy profiles
if [ -d "$SCRIPT_DIR/profiles" ]; then
    cp "$SCRIPT_DIR/profiles"/*.json "$NAH_ROOT/host/profiles/" 2>/dev/null || true
    log_success "Copied host profiles"
fi

# Set default profile
if [ -n "$DEFAULT_PROFILE" ] && [ -f "$NAH_ROOT/host/profiles/${DEFAULT_PROFILE}.json" ]; then
    ln -sf "profiles/${DEFAULT_PROFILE}.json" "$NAH_ROOT/host/profile.current"
    log_success "Set default profile: $DEFAULT_PROFILE"
fi

# =============================================================================
# Install NAKs
# =============================================================================

echo ""
log_info "Installing NAKs..."

NAK_COUNT=$(get_json_array_count "$HOST_MANIFEST" "naks")
for i in $(seq 0 $((NAK_COUNT - 1))); do
    NAK_ID=$(get_json_array_field "$HOST_MANIFEST" "naks" "$i" "id")
    NAK_VERSION=$(get_json_array_field "$HOST_MANIFEST" "naks" "$i" "version")
    NAK_PACKAGE=$(get_json_array_field "$HOST_MANIFEST" "naks" "$i" "package")
    NAK_OPTIONAL=$(get_json_array_field "$HOST_MANIFEST" "naks" "$i" "optional")

    # Resolve package path
    if [[ "$NAK_PACKAGE" != /* ]]; then
        NAK_PACKAGE="$SCRIPT_DIR/${NAK_PACKAGE#./}"
    fi

    if [ -f "$NAK_PACKAGE" ]; then
        "$NAH_CLI" --root "$NAH_ROOT" nak install "$NAK_PACKAGE" >/dev/null 2>&1
        log_success "Installed NAK: $NAK_ID@$NAK_VERSION"
    elif [ "$NAK_OPTIONAL" = "True" ] || [ "$NAK_OPTIONAL" = "true" ]; then
        log_warn "Skipping optional NAK: $NAK_ID (package not found)"
    else
        log_error "NAK package not found: $NAK_PACKAGE"
        exit 1
    fi
done

# =============================================================================
# Install Apps
# =============================================================================

echo ""
log_info "Installing apps..."

APP_COUNT=$(get_json_array_count "$HOST_MANIFEST" "apps")
for i in $(seq 0 $((APP_COUNT - 1))); do
    APP_ID=$(get_json_array_field "$HOST_MANIFEST" "apps" "$i" "id")
    APP_VERSION=$(get_json_array_field "$HOST_MANIFEST" "apps" "$i" "version")
    APP_PACKAGE=$(get_json_array_field "$HOST_MANIFEST" "apps" "$i" "package")
    APP_NAK=$(get_json_array_field "$HOST_MANIFEST" "apps" "$i" "nak")
    APP_OPTIONAL=$(get_json_array_field "$HOST_MANIFEST" "apps" "$i" "optional")

    # Check if required NAK is installed
    if [ -n "$APP_NAK" ] && [ ! -d "$NAH_ROOT/naks/$APP_NAK" ]; then
        if [ "$APP_OPTIONAL" = "True" ] || [ "$APP_OPTIONAL" = "true" ]; then
            log_warn "Skipping optional app: $APP_ID (NAK $APP_NAK not installed)"
            continue
        else
            log_error "Required NAK not installed: $APP_NAK"
            exit 1
        fi
    fi

    # Resolve package path
    if [[ "$APP_PACKAGE" != /* ]]; then
        APP_PACKAGE="$SCRIPT_DIR/${APP_PACKAGE#./}"
    fi

    if [ -f "$APP_PACKAGE" ]; then
        "$NAH_CLI" --root "$NAH_ROOT" app install "$APP_PACKAGE" >/dev/null 2>&1
        log_success "Installed app: $APP_ID@$APP_VERSION"
    elif [ "$APP_OPTIONAL" = "True" ] || [ "$APP_OPTIONAL" = "true" ]; then
        log_warn "Skipping optional app: $APP_ID (package not found)"
    else
        log_error "App package not found: $APP_PACKAGE"
        exit 1
    fi
done

# =============================================================================
# Summary
# =============================================================================

echo ""
echo "=============================================="
log_success "Host setup complete!"
echo "=============================================="
echo ""
echo "NAH Root: $NAH_ROOT"
echo ""
echo "Installed NAKs:"
ls -1 "$NAH_ROOT/naks/" 2>/dev/null | while read nak; do
    versions=$(ls -1 "$NAH_ROOT/naks/$nak" 2>/dev/null | tr '\n' ', ' | sed 's/,$//')
    echo "  $nak ($versions)"
done
echo ""
echo "Installed Apps:"
ls -1 "$NAH_ROOT/apps/" 2>/dev/null | sed 's/^/  /'
echo ""
echo "Next steps:"
echo "  ./run.sh                    # Run all apps"
echo "  ./run.sh --contract         # Show launch contracts"
echo "  ./run.sh com.example.app    # Run specific app"
