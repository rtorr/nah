#!/usr/bin/env bash
#
# Host Setup Script
# =================
# Reads host.toml and sets up the NAH root with all declared NAKs and apps.
#
# Usage:
#   ./setup.sh [--clean] [--skip-optional]
#
# This script:
#   1. Reads host.toml to find declared NAKs and apps
#   2. Builds any NAKs/apps that need building
#   3. Installs them into the NAH root
#   4. Copies host profiles
#   5. Sets the default profile

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
log_warn()    { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error()   { echo -e "${RED}[ERROR]${NC} $1"; }

# =============================================================================
# Parse host.toml (basic TOML parsing with grep/sed)
# =============================================================================

get_toml_value() {
    local file="$1" key="$2"
    grep "^${key}[[:space:]]*=" "$file" 2>/dev/null | sed 's/.*=[[:space:]]*"\{0,1\}\([^"]*\)"\{0,1\}/\1/' | head -1
}

get_toml_array_blocks() {
    local file="$1" block="$2"
    # Count occurrences of [[block]]
    grep -c "^\[\[${block}\]\]" "$file" 2>/dev/null || echo 0
}

get_block_field() {
    local file="$1" block="$2" index="$3" field="$4"
    # Extract the nth block and get the field
    awk -v block="$block" -v idx="$index" -v field="$field" '
        BEGIN { count=0; in_block=0 }
        /^\[\['"$block"'\]\]/ { count++; in_block=(count==idx) }
        /^\[/ && !/^\[\['"$block"'\]\]/ { in_block=0 }
        in_block && $0 ~ "^"field"[[:space:]]*=" {
            gsub(/.*=[[:space:]]*"?/, "")
            gsub(/".*/, "")
            print
            exit
        }
    ' "$file"
}

# =============================================================================
# Find NAH CLI
# =============================================================================

find_nah_cli() {
    if [ -n "$NAH_CLI" ] && [ -x "$NAH_CLI" ]; then
        return 0
    fi

    # Check common locations
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
# Build helpers
# =============================================================================

build_cmake_project() {
    local src_dir="$1" name="$2"
    local build_dir="$src_dir/build"

    if [ ! -f "$src_dir/CMakeLists.txt" ]; then
        log_error "No CMakeLists.txt in $src_dir"
        return 1
    fi

    log_info "Building $name..."
    mkdir -p "$build_dir"
    cmake -S "$src_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
    cmake --build "$build_dir" --parallel >/dev/null 2>&1
    log_success "Built $name"
}

build_conan_project() {
    local src_dir="$1" name="$2"

    if ! command -v conan &>/dev/null; then
        log_warn "Conan not available, skipping $name"
        return 1
    fi

    log_info "Building $name with Conan..."
    cd "$src_dir"
    conan install . --output-folder=build --build=missing \
        --deployer=full_deploy --deployer-folder=build/deploy >/dev/null 2>&1
    cmake --preset conan-release >/dev/null 2>&1
    cmake --build build/build/Release --parallel >/dev/null 2>&1
    cd - >/dev/null
    log_success "Built $name"
}

# =============================================================================
# Main
# =============================================================================

CLEAN=0
SKIP_OPTIONAL=0

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean) CLEAN=1; shift ;;
        --skip-optional) SKIP_OPTIONAL=1; shift ;;
        --help|-h)
            echo "Usage: $0 [--clean] [--skip-optional]"
            echo ""
            echo "Options:"
            echo "  --clean          Remove existing NAH root before setup"
            echo "  --skip-optional  Skip optional NAKs and apps"
            exit 0
            ;;
        *) shift ;;
    esac
done

# Verify host.toml exists
if [ ! -f "$HOST_MANIFEST" ]; then
    log_error "host.toml not found at $HOST_MANIFEST"
    exit 1
fi

# Find NAH CLI
if ! find_nah_cli; then
    log_error "NAH CLI not found. Build NAH first or set NAH_CLI."
    exit 1
fi

# Read host configuration
HOST_NAME=$(get_toml_value "$HOST_MANIFEST" "name")
NAH_ROOT_REL=$(get_toml_value "$HOST_MANIFEST" "nah_root")
DEFAULT_PROFILE=$(get_toml_value "$HOST_MANIFEST" "default_profile")

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
    cp "$SCRIPT_DIR/profiles"/*.toml "$NAH_ROOT/host/profiles/" 2>/dev/null || true
    log_success "Copied host profiles"
fi

# Set default profile
if [ -n "$DEFAULT_PROFILE" ] && [ -f "$NAH_ROOT/host/profiles/${DEFAULT_PROFILE}.toml" ]; then
    ln -sf "profiles/${DEFAULT_PROFILE}.toml" "$NAH_ROOT/host/profile.current"
    log_success "Set default profile: $DEFAULT_PROFILE"
fi

# =============================================================================
# Install NAKs
# =============================================================================

echo ""
log_info "Installing NAKs..."

NAK_COUNT=$(get_toml_array_blocks "$HOST_MANIFEST" "naks")
for i in $(seq 1 "$NAK_COUNT"); do
    NAK_ID=$(get_block_field "$HOST_MANIFEST" "naks" "$i" "id")
    NAK_VERSION=$(get_block_field "$HOST_MANIFEST" "naks" "$i" "version")
    NAK_SOURCE=$(get_block_field "$HOST_MANIFEST" "naks" "$i" "source")
    NAK_DESC=$(get_block_field "$HOST_MANIFEST" "naks" "$i" "description")
    NAK_OPTIONAL=$(get_block_field "$HOST_MANIFEST" "naks" "$i" "optional")

    if [ "$SKIP_OPTIONAL" = "1" ] && [ "$NAK_OPTIONAL" = "true" ]; then
        log_warn "Skipping optional NAK: $NAK_ID"
        continue
    fi

    # Resolve source path
    if [[ "$NAK_SOURCE" != /* ]]; then
        NAK_SOURCE="$SCRIPT_DIR/${NAK_SOURCE#./}"
    fi

    # Find or build NAK package
    NAK_FILE=""

    # Check for pre-built NAK
    if [ -f "$NAK_SOURCE/build/${NAK_ID}-${NAK_VERSION}.nak" ]; then
        NAK_FILE="$NAK_SOURCE/build/${NAK_ID}-${NAK_VERSION}.nak"
    elif [ -f "$NAK_SOURCE/build/build/Release/${NAK_ID}-${NAK_VERSION}.nak" ]; then
        NAK_FILE="$NAK_SOURCE/build/build/Release/${NAK_ID}-${NAK_VERSION}.nak"
    else
        # Need to build
        if [ -f "$NAK_SOURCE/conanfile.py" ]; then
            if build_conan_project "$NAK_SOURCE" "$NAK_DESC"; then
                NAK_FILE="$NAK_SOURCE/build/build/Release/${NAK_ID}-${NAK_VERSION}.nak"
            elif [ "$NAK_OPTIONAL" = "true" ]; then
                log_warn "Skipping optional NAK: $NAK_ID (build failed)"
                continue
            else
                log_error "Failed to build required NAK: $NAK_ID"
                exit 1
            fi
        elif [ -f "$NAK_SOURCE/CMakeLists.txt" ]; then
            build_cmake_project "$NAK_SOURCE" "$NAK_DESC"
            cmake --build "$NAK_SOURCE/build" --target package_nak >/dev/null 2>&1 || true
            NAK_FILE="$NAK_SOURCE/build/${NAK_ID}-${NAK_VERSION}.nak"
        fi
    fi

    if [ -f "$NAK_FILE" ]; then
        "$NAH_CLI" --root "$NAH_ROOT" nak install "$NAK_FILE" >/dev/null 2>&1
        log_success "Installed NAK: $NAK_ID@$NAK_VERSION"
    elif [ "$NAK_OPTIONAL" = "true" ]; then
        log_warn "Skipping optional NAK: $NAK_ID (not found)"
    else
        log_error "NAK package not found: $NAK_ID"
        exit 1
    fi
done

# =============================================================================
# Install Apps
# =============================================================================

echo ""
log_info "Installing apps..."

APP_COUNT=$(get_toml_array_blocks "$HOST_MANIFEST" "apps")
for i in $(seq 1 "$APP_COUNT"); do
    APP_ID=$(get_block_field "$HOST_MANIFEST" "apps" "$i" "id")
    APP_VERSION=$(get_block_field "$HOST_MANIFEST" "apps" "$i" "version")
    APP_SOURCE=$(get_block_field "$HOST_MANIFEST" "apps" "$i" "source")
    APP_DESC=$(get_block_field "$HOST_MANIFEST" "apps" "$i" "description")
    APP_NAK=$(get_block_field "$HOST_MANIFEST" "apps" "$i" "nak")
    APP_OPTIONAL=$(get_block_field "$HOST_MANIFEST" "apps" "$i" "optional")

    if [ "$SKIP_OPTIONAL" = "1" ] && [ "$APP_OPTIONAL" = "true" ]; then
        log_warn "Skipping optional app: $APP_ID"
        continue
    fi

    # Check if required NAK is installed
    if [ -n "$APP_NAK" ]; then
        if [ ! -d "$NAH_ROOT/naks/$APP_NAK" ]; then
            if [ "$APP_OPTIONAL" = "true" ]; then
                log_warn "Skipping $APP_ID (NAK $APP_NAK not installed)"
                continue
            else
                log_error "Required NAK not installed: $APP_NAK"
                exit 1
            fi
        fi
    fi

    # Resolve source path
    if [[ "$APP_SOURCE" != /* ]]; then
        APP_SOURCE="$SCRIPT_DIR/${APP_SOURCE#./}"
    fi

    # Find or build app package
    APP_FILE=""

    if [ -f "$APP_SOURCE/build/${APP_ID}-${APP_VERSION}.nap" ]; then
        APP_FILE="$APP_SOURCE/build/${APP_ID}-${APP_VERSION}.nap"
    else
        # Build the app
        if [ -f "$APP_SOURCE/CMakeLists.txt" ]; then
            build_cmake_project "$APP_SOURCE" "$APP_DESC"
            cmake --build "$APP_SOURCE/build" --target package_nap >/dev/null 2>&1 || true
            APP_FILE="$APP_SOURCE/build/${APP_ID}-${APP_VERSION}.nap"
        fi
    fi

    if [ -f "$APP_FILE" ]; then
        "$NAH_CLI" --root "$NAH_ROOT" app install "$APP_FILE" >/dev/null 2>&1
        log_success "Installed app: $APP_ID@$APP_VERSION"
    elif [ "$APP_OPTIONAL" = "true" ]; then
        log_warn "Skipping optional app: $APP_ID (not found)"
    else
        log_error "App package not found: $APP_ID"
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
