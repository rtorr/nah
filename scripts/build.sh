#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$root_dir/build"
build_type=Debug
jobs=""
build_examples=false

usage() {
    echo "Usage: $0 [--clean] [--release] [--jobs N] [--examples]"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) cmake -E remove_directory "$build_dir"; shift ;;
        --release) build_type=Release; shift ;;
        --jobs) jobs="$2"; shift 2 ;;
        --examples) build_examples=true; shift ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done

cmake -S "$root_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DNAH_ENABLE_TESTS=ON

build_args=(--build "$build_dir" --parallel)
if [[ -n "$jobs" ]]; then build_args+=("$jobs"); fi
cmake "${build_args[@]}"

if [[ "$build_examples" == true ]]; then
    NAH_CLI="$build_dir/tools/nah/nah" "$root_dir/examples/scripts/build_all.sh"
fi
