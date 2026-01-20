#!/bin/bash
# Format NAH source code
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --check      Check formatting without modifying files"
    echo "  -h, --help   Show this help"
}

CHECK_ONLY=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --check)
            CHECK_ONLY=true
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

if ! command -v clang-format &> /dev/null; then
    echo "clang-format not found. Please install it."
    exit 1
fi

cd "$PROJECT_ROOT"

# Find all C/C++ source files (excluding build directories and third-party)
FILES=$(find src include tools tests -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.c" -o -name "*.h" \) \
    ! -path "*/build/*" \
    ! -path "*/third_party/*" \
    2>/dev/null)

if [ -z "$FILES" ]; then
    echo "No source files found."
    exit 0
fi

FILE_COUNT=$(echo "$FILES" | wc -l | tr -d ' ')

if [ "$CHECK_ONLY" = true ]; then
    echo "Checking formatting of $FILE_COUNT files..."
    FAILED=false
    for f in $FILES; do
        if ! clang-format --dry-run --Werror "$f" 2>/dev/null; then
            echo "  Needs formatting: $f"
            FAILED=true
        fi
    done
    if [ "$FAILED" = true ]; then
        echo ""
        echo "Some files need formatting. Run scripts/format.sh to fix."
        exit 1
    fi
    echo "All files formatted correctly."
else
    echo "Formatting $FILE_COUNT files..."
    for f in $FILES; do
        clang-format -i "$f"
    done
    echo "Done."
fi
