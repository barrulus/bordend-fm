"""One-time local reference capture. Only the hand-written fixtures are distributable."""

import csv
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import time


def main():
    if not shutil.which("resvg"):
        raise SystemExit("Run through nix shell nixpkgs#resvg --command bash scripts/svg-capture-references.sh")
    corpus = Path("tests/data/svg-corpus")
    if corpus.exists():
        raise SystemExit("Reference capture already exists; resvg must not be run again.")
    fixtures = sorted(Path("tests/data/svg").glob("*.svg"))
    if not fixtures or any(p.with_suffix(".png").exists() for p in fixtures):
        raise SystemExit("Expected SVG fixtures with no existing PNG references.")
    roots = [
        Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "icons",
        *(Path(p) / "icons" for p in os.environ.get("XDG_DATA_DIRS", "/usr/local/share:/usr/share").split(":")),
        Path.home() / ".icons",
    ]
    sources = []
    seen = set()
    names = {}
    missing = []
    for root in roots:
        if not root.is_dir():
            continue
        for theme in sorted(root.iterdir()):
            if not theme.is_dir() or theme.resolve() in seen:
                continue
            seen.add(theme.resolve())
            files = sorted(theme.rglob("*.svg"))
            if not files:
                continue
            names[theme.name] = names.get(theme.name, 0) + 1
            label = theme.name if names[theme.name] == 1 else f"{theme.name}-{names[theme.name]}"
            for source in files:
                if not source.is_file():
                    missing.append(str(source))
                    continue
                digest = hashlib.sha256(source.read_bytes()).hexdigest()
                for size in (16, 48, 128):
                    output = corpus / label / str(size) / source.relative_to(theme).with_suffix(".png")
                    sources.append((source.absolute(), digest, size, output))
    if not sources:
        raise SystemExit("No installed SVGs found; capture has not started.")
    corpus.mkdir(parents=True)
    (corpus / "missing-sources.txt").write_text("\n".join(missing) + "\n")
    (corpus / "capture-started.txt").write_text(time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()) + "\n")
    totals = {size: 0.0 for size in (16, 48, 128)}
    failures = 0
    with (corpus / "manifest.tsv").open("w", newline="") as manifest, (corpus / "resvg.log").open("w") as log:
        writer = csv.writer(manifest, delimiter="\t")
        writer.writerow(("source", "sha256", "size", "seconds", "reference", "status", "render_seconds"))
        work = sources + [(p.absolute(), hashlib.sha256(p.read_bytes()).hexdigest(), 64, p.with_suffix(".png")) for p in fixtures]
        for index, (source, digest, size, output) in enumerate(work):
            output.parent.mkdir(parents=True, exist_ok=True)
            start = time.perf_counter()
            result = subprocess.run(["resvg", "--width", str(size), "--height", str(size), "--perf", str(source), str(output)], capture_output=True, text=True, timeout=60)
            elapsed = time.perf_counter() - start
            log.write(f"{source}\t{size}\n{result.stdout}{result.stderr}\n")
            phases = re.findall(r"^(?:XML Parsing|SVG Parsing|Rendering): ([0-9.]+)ms$",
                                result.stdout + result.stderr, re.MULTILINE)
            render_seconds = sum(float(value) for value in phases) / 1000
            if index < len(sources):
                writer.writerow((str(source), digest, size, f"{elapsed:.9f}", str(output.relative_to(corpus)), result.returncode, f"{render_seconds:.9f}"))
                totals[size] += elapsed
            failures += result.returncode != 0
            if result.returncode:
                print(f"FAILED: {source} at {size}: {result.stderr}", flush=True)
            if (index + 1) % 300 == 0:
                manifest.flush()
                print(f"Captured {index + 1}/{len(work)}", flush=True)
    summary = f"Theme renders: {len(sources)}; fixtures: {len(fixtures)}; failures: {failures}\n"
    summary += "CLI wall time, including parse, rendering, PNG encoding and process startup:\n"
    summary += "".join(f"{size}px: {seconds:.6f}s\n" for size, seconds in totals.items())
    (corpus / "capture-complete.txt").write_text(summary)
    print(summary, flush=True)
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
