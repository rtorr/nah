#!/usr/bin/env bash
# Common functions for NAH example scripts
# Source this file: source "$(dirname "$0")/common.sh"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[OK]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_header() { echo -e "\n${CYAN}=== $1 ===${NC}\n"; }

# Find the examples directory (works from any script location)
find_examples_dir() {
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd)"

    # If we're in scripts/, go up one level
    if [[ "$(basename "$script_dir")" == "scripts" ]]; then
        echo "$(dirname "$script_dir")"
    else
        echo "$script_dir"
    fi
}

# Find NAH CLI
# Sets: NAH_CLI
# Prefers project-built CLI over system PATH for development
find_nah_cli() {
    if [ -n "$NAH_CLI" ] && [ -x "$NAH_CLI" ]; then
        return 0
    fi

    # Check project build locations first (prefer local build for development)
    local examples_dir="$(find_examples_dir)"
    local locations=(
        "$examples_dir/../build/tools/nah/nah"
        "$examples_dir/../cmake-build-release/tools/nah/nah"
        "$examples_dir/../cmake-build-debug/tools/nah/nah"
    )

    for loc in "${locations[@]}"; do
        if [ -x "$loc" ]; then
            NAH_CLI="$loc"
            return 0
        fi
    done

    # Fall back to PATH
    if command -v nah &> /dev/null; then
        NAH_CLI="$(command -v nah)"
        return 0
    fi

    # Last resort: system locations
    for loc in "/usr/local/bin/nah" "/usr/bin/nah"; do
        if [ -x "$loc" ]; then
            NAH_CLI="$loc"
            return 0
        fi
    done

    return 1
}

# Build a CMake project
# Usage: build_cmake_project <dir> <name> [extra_cmake_args...]
# Note: Runs in a subshell to preserve working directory
build_cmake_project() {
    local dir="$1"
    local name="$2"
    shift 2
    local extra_args="$*"

    log_info "Building $name..."

    if [ ! -d "$dir" ]; then
        log_error "Directory not found: $dir"
        return 1
    fi

    (
        cd "$dir"

        if [ "${CLEAN:-0}" = "1" ] && [ -d "build" ]; then
            rm -rf build
        fi

        mkdir -p build
        cd build

        if ! cmake .. -DNAH_CLI="$NAH_CLI" $extra_args; then
            log_error "CMake configuration failed for $name"
            exit 1
        fi

        if ! cmake --build .; then
            log_error "Build failed for $name"
            exit 1
        fi
    ) || return 1

    log_success "Built $name"
    return 0
}

# Build a Conan project
# Usage: build_conan_project <dir> <name>
# Note: Runs in a subshell to preserve working directory
build_conan_project() {
    local dir="$1"
    local name="$2"

    if [ "${SKIP_CONAN:-0}" = "1" ]; then
        log_warn "Skipping $name (SKIP_CONAN=1)"
        return 1
    fi

    if ! command -v conan &> /dev/null; then
        log_warn "Skipping $name (Conan not found)"
        return 1
    fi

    log_info "Building $name with Conan..."

    (
        cd "$dir"

        if [ "${CLEAN:-0}" = "1" ] && [ -d "build" ]; then
            rm -rf build
        fi

        # Install Conan dependencies with full_deploy
        if ! conan install . --output-folder=build --build=missing \
            --deployer=full_deploy --deployer-folder=build/deploy 2>&1; then
            log_error "Conan install failed for $name"
            exit 1
        fi

        # Configure and build
        if ! cmake --preset conan-release -DNAH_CLI="$NAH_CLI"; then
            log_error "CMake configuration failed for $name"
            exit 1
        fi

        if ! cmake --build build/build/Release; then
            log_error "Build failed for $name"
            exit 1
        fi
    ) || return 1

    log_success "Built $name"
    return 0
}

# Install a NAK file
# Usage: install_nak <nak_file> <name>
install_nak() {
    local nak_file="$1"
    local name="$2"

    if [ -f "$nak_file" ]; then
        $NAH_CLI --root "$NAH_ROOT" install "$nak_file"
        log_success "Installed $name NAK"
        return 0
    else
        log_warn "$name NAK not found: $nak_file"
        return 1
    fi
}

# Install a NAP file
# Usage: install_nap <nap_file> <name>
install_nap() {
    local nap_file="$1"
    local name="$2"

    if [ -f "$nap_file" ]; then
        $NAH_CLI --root "$NAH_ROOT" install "$nap_file"
        log_success "Installed $name"
        return 0
    else
        log_warn "$name NAP not found: $nap_file"
        return 1
    fi
}
