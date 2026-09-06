#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$root_dir/build"
test_regex=""
verbose=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --unit) test_regex='^nah-tests$'; shift ;;
        --integration) test_regex='^(integration_tests|cli_lifecycle)$'; shift ;;
        --all) test_regex=""; shift ;;
        --verbose) verbose=true; shift ;;
        -h|--help) echo "Usage: $0 [--unit|--integration|--all] [--verbose]"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

cmake -S "$root_dir" -B "$build_dir" -DNAH_ENABLE_TESTS=ON
cmake --build "$build_dir" --parallel

ctest_args=(--test-dir "$build_dir" --output-on-failure)
if [[ -n "$test_regex" ]]; then ctest_args+=(-R "$test_regex"); fi
if [[ "$verbose" == true ]]; then ctest_args+=(-V); fi
ctest "${ctest_args[@]}"
