#!/bin/bash
# Build the NAH project
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --clean       Clean before building"
    echo "  --release     Build in Release mode (default: Debug)"
    echo "  --jobs N      Parallel jobs (default: auto)"
    echo "  --examples    Also build examples"
    echo "  -h, --help    Show this help"
}

BUILD_TYPE="Debug"
CLEAN=false
JOBS=""
BUILD_EXAMPLES=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN=true
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --jobs)
            JOBS="-j$2"
            shift 2
            ;;
        --examples)
            BUILD_EXAMPLES=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

if [ -z "$JOBS" ]; then
    if command -v nproc &> /dev/null; then
        JOBS="-j$(nproc)"
    elif command -v sysctl &> /dev/null; then
        JOBS="-j$(sysctl -n hw.ncpu)"
    else
        JOBS="-j4"
    fi
fi

if [ "$CLEAN" = true ] && [ -d "$BUILD_DIR" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring (${BUILD_TYPE})..."
cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "Building..."
make $JOBS

echo ""
echo "Build complete!"
echo "  Binary: $BUILD_DIR/tools/nah/nah"
echo "  Tests:  $BUILD_DIR/tests/unit/nah-tests"

if [ "$BUILD_EXAMPLES" = true ]; then
    echo ""
    echo "Building examples..."
    "$PROJECT_ROOT/examples/host/scripts/setup_demo.sh"
fi
