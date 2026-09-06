#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
clean_main=true
clean_examples=false

case "${1:-}" in
    "") ;;
    --examples) clean_main=false; clean_examples=true ;;
    --all) clean_examples=true ;;
    -h|--help) echo "Usage: $0 [--examples|--all]"; exit 0 ;;
    *) echo "Usage: $0 [--examples|--all]" >&2; exit 2 ;;
esac

if [[ "$clean_main" == true ]]; then
    cmake -E remove_directory "$root_dir/build"
fi
if [[ "$clean_examples" == true ]]; then
    "$root_dir/examples/scripts/clean_all.sh" --all
fi
