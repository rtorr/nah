#!/bin/bash
# Run NAH tests
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

usage() {
    echo "Usage: $0 [OPTIONS] [FILTER]"
    echo ""
    echo "Options:"
    echo "  --unit          Run unit tests only (default)"
    echo "  --integration   Run integration tests only"
    echo "  --all           Run all tests"
    echo "  --verbose       Verbose output"
    echo "  --list          List available tests"
    echo "  -h, --help      Show this help"
    echo ""
    echo "Filter:"
    echo "  Test name filter passed to doctest (e.g., 'Manifest*')"
}

TEST_TYPE="unit"
VERBOSE=""
LIST=false
FILTER=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --unit)
            TEST_TYPE="unit"
            shift
            ;;
        --integration)
            TEST_TYPE="integration"
            shift
            ;;
        --all)
            TEST_TYPE="all"
            shift
            ;;
        --verbose)
            VERBOSE="-s"
            shift
            ;;
        --list)
            LIST=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
        *)
            FILTER="$1"
            shift
            ;;
    esac
done

if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found. Run scripts/build.sh first."
    exit 1
fi

UNIT_TEST="$BUILD_DIR/tests/unit/nah-tests"
INTEGRATION_TEST="$BUILD_DIR/tests/integration/nah-integration-tests"

run_unit() {
    if [ ! -x "$UNIT_TEST" ]; then
        echo "Unit tests not found. Run scripts/build.sh first."
        exit 1
    fi
    echo "Running unit tests..."
    if [ "$LIST" = true ]; then
        "$UNIT_TEST" --list-test-cases
    elif [ -n "$FILTER" ]; then
        "$UNIT_TEST" $VERBOSE --test-case="$FILTER"
    else
        "$UNIT_TEST" $VERBOSE
    fi
}

run_integration() {
    if [ ! -x "$INTEGRATION_TEST" ]; then
        echo "Integration tests not found. Run scripts/build.sh first."
        exit 1
    fi
    echo "Running integration tests..."
    if [ "$LIST" = true ]; then
        "$INTEGRATION_TEST" --list-test-cases
    elif [ -n "$FILTER" ]; then
        "$INTEGRATION_TEST" $VERBOSE --test-case="$FILTER"
    else
        "$INTEGRATION_TEST" $VERBOSE
    fi
}

case $TEST_TYPE in
    unit)
        run_unit
        ;;
    integration)
        run_integration
        ;;
    all)
        run_unit
        echo ""
        run_integration
        ;;
esac
