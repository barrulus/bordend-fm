#!/usr/bin/env bash
# Run from a committed checkout. Output must be outside that checkout.
set -euo pipefail
root=$(git rev-parse --show-toplevel)
cd "$root"
# shellcheck source=scripts/ci/void-env
source scripts/ci/void-env
bash scripts/ci/export-source.sh "${1:?usage: scripts/ci/void.sh OUTPUT_DIRECTORY}"
output=$(realpath "$1")
version=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["version"])' "$output/input/source.json")
docker run --rm --platform linux/amd64 \
    -v "$root:/source:ro" -v "$output/input:/input:ro" -v "$output:/output" \
    -e CI_JOBS="${CI_JOBS:-2}" "$VOID_BUILD_IMAGE" bash /source/scripts/ci/void-build.sh
# Deliberately fresh: no compiler, headers or build dependencies from xbps-src.
docker run --rm --platform linux/amd64 \
    -v "$root:/source:ro" -v "$output/packages:/packages:ro" \
    "$VOID_IMAGE" sh -c 'xbps-install -Syu xbps && xbps-install -y bash && exec bash "$@"' sh /source/scripts/ci/void-install.sh "$version"
