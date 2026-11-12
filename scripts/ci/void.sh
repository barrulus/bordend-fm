#!/usr/bin/env bash
# Run from a committed checkout. Output must be outside that checkout.
set -euo pipefail
root=$(git rev-parse --show-toplevel)
cd "$root"
source scripts/ci/void-env
output=${1:?usage: scripts/ci/void.sh OUTPUT_DIRECTORY}
mkdir -p "$output"
output=$(realpath "$output")
[[ "$output" != "$root" && "$output" != "$root/"* ]]
if [[ -n $(git status --porcelain --untracked-files=normal) ]]; then
    echo 'Commit the checkout first: the package must match the reported Git revision.' >&2
    exit 1
fi
mkdir -p "$output/input"
git archive --format=tar --prefix=nordend-checkout/ HEAD | gzip -n > "$output/input/source.tar.gz"
python3 - "$output/input" <<'PY'
import hashlib, json, re, subprocess, sys
from pathlib import Path
out = Path(sys.argv[1])
version = re.search(r'project\(nordend VERSION ([0-9.]+)', Path('CMakeLists.txt').read_text())[1]
metadata = {
    'commit': subprocess.check_output(['git','rev-parse','HEAD'], text=True).strip(),
    'timestamp': subprocess.check_output(['git','show','-s','--format=%cd','--date=format-local:%Y%m%d%H%M%S','HEAD'], text=True, env={**__import__('os').environ, 'TZ':'UTC'}).strip(),
    'version': version,
    'sha256': hashlib.sha256((out/'source.tar.gz').read_bytes()).hexdigest(),
}
(out/'source.json').write_text(json.dumps(metadata, indent=2)+'\n')
PY
version=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["version"])' "$output/input/source.json")
docker run --rm --platform linux/amd64 \
    -v "$root:/source:ro" -v "$output/input:/input:ro" -v "$output:/output" \
    -e CI_JOBS="${CI_JOBS:-2}" "$VOID_IMAGE" bash /source/scripts/ci/void-build.sh
# Deliberately fresh: no compiler, headers or build dependencies from xbps-src.
docker run --rm --platform linux/amd64 \
    -v "$root:/source:ro" -v "$output/packages:/packages:ro" \
    "$VOID_IMAGE" bash /source/scripts/ci/void-install.sh "$version"
