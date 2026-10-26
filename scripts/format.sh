#!/usr/bin/env bash
# Formats every first-party C++ source with clang-format.
#   scripts/format.sh           rewrite files in place
#   scripts/format.sh --check   exit non-zero if anything is unformatted
#
# Override the binary with CLANG_FORMAT= (e.g. clang-format-21).
set -euo pipefail
cd "$(dirname "$0")/.."

clang_format="${CLANG_FORMAT:-clang-format}"
if ! command -v "$clang_format" >/dev/null 2>&1; then
    echo "error: '$clang_format' not found (install clang-format, or set CLANG_FORMAT)" >&2
    exit 127
fi

mapfile -t files < <(git ls-files -- src tests | grep -E '\.(cpp|hpp|h)$')
[[ ${#files[@]} -gt 0 ]] || { echo "no sources found"; exit 0; }

if [[ "${1:-}" == "--check" ]]; then
    "$clang_format" --dry-run --Werror "${files[@]}"
    echo "format: ${#files[@]} files clean"
else
    "$clang_format" -i "${files[@]}"
    echo "format: ${#files[@]} files processed"
fi
