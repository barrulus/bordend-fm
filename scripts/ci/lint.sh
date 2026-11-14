#!/usr/bin/env bash
# Shared by GitHub Actions and the local preflight runner.
set -euo pipefail
cd "$(dirname "$0")/../.."
export PYTHONDONTWRITEBYTECODE=1
git diff --check
git diff --cached --check
actionlint .github/workflows/ci.yml
shellcheck -x scripts/ci/*.sh
python3 -m unittest discover -s scripts/ci/tests -v
python3 scripts/check-docs.py
