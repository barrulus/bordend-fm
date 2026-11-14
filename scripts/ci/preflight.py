#!/usr/bin/env python3
"""Run the CI checks locally, preserving logs and the tested commit identity."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TARGETS = ("lint", "nix", "void", "deb", "rpm", "arch", "tumbleweed")


def commands(target, output):
    if target == "lint":
        return [["nix", "shell", "--inputs-from", ".", "nixpkgs#actionlint",
                 "nixpkgs#shellcheck", "nixpkgs#python3", "--command",
                 "bash", "scripts/ci/lint.sh"]]
    if target == "nix":
        return [["nix", "flake", "check", "--print-build-logs"],
                ["nix", "build", "--no-link", "--print-build-logs"]]
    if target == "void":
        return [["bash", "scripts/ci/void.sh", str(output / "void-package")]]
    return [["bash", "scripts/ci/native.sh", target, str(output / f"{target}-package")]]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("targets", nargs="*", metavar="TARGET",
                        help="lint, nix, void, deb, rpm, arch, tumbleweed, or all (default)")
    parser.add_argument("--output", type=Path,
                        help="new or empty directory outside the checkout for logs/packages")
    args = parser.parse_args(argv)
    targets = args.targets or ["all"]
    if any(t not in (*TARGETS, "all") for t in targets):
        parser.error("unknown target; choose " + ", ".join((*TARGETS, "all")))
    targets = list(TARGETS) if "all" in targets else list(dict.fromkeys(targets))
    os.chdir(ROOT)
    os.environ["PYTHONDONTWRITEBYTECODE"] = "1"
    dirty = bool(subprocess.check_output(
        ["git", "status", "--porcelain", "--untracked-files=normal"], text=True).strip())
    if dirty and targets != ["lint"]:
        parser.error("commit all intended changes first: build checks must test the reported commit; "
                     "use 'lint' while editing")
    required = {"nix"} if any(t in ("lint", "nix") for t in targets) else set()
    containers = any(t not in ("lint", "nix") for t in targets)
    if containers:
        required.update(("docker", "bash", "python3", "tar", "gzip", "realpath"))
    for tool in sorted(required):
        if not shutil.which(tool):
            parser.error(f"required tool not found: {tool}")
    if containers and subprocess.run(["docker", "info"], stdout=subprocess.DEVNULL).returncode:
        parser.error("Docker daemon is unavailable; no package checks were run")
    output = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix="rummage-preflight-"))
    if output == ROOT or ROOT in output.parents:
        parser.error("output must be outside the checkout")
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        parser.error("output must be a new or empty directory")
    output.mkdir(parents=True, exist_ok=True)
    report = {
        "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        "dirty": dirty, "requested": targets, "results": {},
    }

    def save():
        (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n")

    save()
    print(f"Preflight {report['commit']}{' (working tree)' if dirty else ''}; logs: {output}", flush=True)
    for target in targets:
        print(f"\n=== {target} ===", flush=True)
        code = 0
        with (output / f"{target}.log").open("w") as log:
            for command in commands(target, output):
                log.write("$ " + " ".join(command) + "\n")
                log.flush()
                with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                      text=True, errors="replace") as process:
                    for line in process.stdout:
                        print(line, end="", flush=True)
                        log.write(line)
                    code = process.wait()
                if code:
                    break
        report["results"][target] = "passed" if code == 0 else f"failed ({code})"
        save()
        if code:
            print(f"FAILED: {target}; see {output / (target + '.log')}. "
                  "Remaining targets were not run.", file=sys.stderr)
            return 1
    print(f"Passed: {', '.join(targets)}. Report: {output / 'summary.json'}")
    if targets != list(TARGETS):
        print("This is a partial check; run 'python3 scripts/ci/preflight.py all' for the full matrix.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
