#!/bin/bash
# Clean NAH build artifacts
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --all        Clean everything including examples"
    echo "  --examples   Clean only examples"
    echo "  -h, --help   Show this help"
}

CLEAN_MAIN=true
CLEAN_EXAMPLES=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --all)
            CLEAN_MAIN=true
            CLEAN_EXAMPLES=true
            shift
            ;;
        --examples)
            CLEAN_MAIN=false
            CLEAN_EXAMPLES=true
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

if [ "$CLEAN_MAIN" = true ]; then
    echo "Cleaning main build..."
    rm -rf "$PROJECT_ROOT/build"
    rm -f "$PROJECT_ROOT/compile_commands.json"
fi

if [ "$CLEAN_EXAMPLES" = true ]; then
    echo "Cleaning examples..."
    rm -rf "$PROJECT_ROOT/examples/demo_nah_root"
    rm -rf "$PROJECT_ROOT/examples/sdk/build"
    rm -rf "$PROJECT_ROOT/examples/apps/app_a/build"
    rm -rf "$PROJECT_ROOT/examples/apps/app_b/build"
    rm -rf "$PROJECT_ROOT/examples/apps/app_c/build"
    rm -rf "$PROJECT_ROOT/examples/host/build"
    rm -f "$PROJECT_ROOT/examples/compile_commands.json"
fi

echo "Clean complete."
