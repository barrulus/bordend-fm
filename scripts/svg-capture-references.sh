#!/usr/bin/env bash
# Stage 0 only. Run once: nix shell nixpkgs#resvg --command bash scripts/svg-capture-references.sh
set -euo pipefail
cd "$(dirname "$0")/.."
exec python3 scripts/svg_capture_references.py
