#!/usr/bin/env python3
"""Check public docs' local links/anchors and TOML fences (Python 3.11+).

No network or third-party dependencies. This checks the Markdown conventions used
in this repository, not every Markdown extension. Historical TOML fences are
excluded: they can be fragments of old formats, rather than current examples.
"""

from __future__ import annotations

import html
from pathlib import Path
import re
import sys
import tomllib
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parent.parent
HISTORICAL = {
    "devlog.md", "benchmarks.md", "buglog.md", "svg-renderer-brief.md",
    "luciole-comparison.md",
}
LINK = re.compile(r"!?\[[^\]\n]*\]\(\s*(<[^>]+>|[^\s)]+)(?:\s+\"[^\"]*\")?\s*\)")
HTML_LINK = re.compile(r"(?:href|src)=[\"']([^\"']+)[\"']")
HEADING = re.compile(r"^#{1,6}\s+(.+?)\s*#*\s*$")
FENCE = re.compile(r"^\s{0,3}(`{3,}|~{3,})(.*)$")


def slug(text: str) -> str:
    text = re.sub(r"\[([^]]+)\]\([^)]*\)", r"\1", text)
    text = html.unescape(re.sub(r"<[^>]+>", "", text)).lower()
    return re.sub(r"[^\w\- ]", "", text).replace(" ", "-")


def scan(path: Path) -> tuple[set[str], list[tuple[int, str]], list[str], int]:
    anchors: set[str] = set()
    links: list[tuple[int, str]] = []
    errors: list[str] = []
    fence = ""
    language = ""
    block: list[str] = []
    block_line = 0
    toml_count = 0
    for number, line in enumerate(path.read_text().splitlines(), 1):
        marker = FENCE.match(line)
        if fence:
            if marker and marker[1][0] == fence[0] and len(marker[1]) >= len(fence) and not marker[2].strip():
                if language == "toml" and path.name not in HISTORICAL:
                    try:
                        tomllib.loads("\n".join(block))
                        toml_count += 1
                    except tomllib.TOMLDecodeError as error:
                        errors.append(f"{path.relative_to(ROOT)}:{block_line}: invalid TOML: {error}")
                fence = ""
            else:
                block.append(line)
            continue
        if marker:
            fence, language = marker[1], marker[2].strip()
            block, block_line = [], number
            continue
        heading = HEADING.match(line)
        if heading:
            base = slug(heading[1])
            anchor, suffix = base, 0
            while anchor in anchors:
                suffix += 1
                anchor = f"{base}-{suffix}"
            anchors.add(anchor)
        # Inline code can mention link syntax without actually creating a link.
        prose = re.sub(r"(`+).*?\1", "", line)
        links.extend((number, match[1].strip("<>")) for match in LINK.finditer(prose))
        links.extend((number, match[1]) for match in HTML_LINK.finditer(prose))
        anchors.update(re.findall(r'\bid=["\']([^"\']+)["\']', prose))
    if fence:
        errors.append(f"{path.relative_to(ROOT)}:{block_line}: unclosed code fence")
    return anchors, links, errors, toml_count


def main() -> int:
    pages = [ROOT / name for name in ("README.md", "CONTRIBUTING.md", "THIRD_PARTY.md")]
    # These local/private notes are deliberately not part of the public docs.
    pages += sorted(p for p in (ROOT / "docs").rglob("*.md") if p.name not in {"DESIGN.md", "ROADMAP.md"})
    scanned = {path.resolve(): scan(path) for path in pages}
    errors = [error for result in scanned.values() for error in result[2]]
    links_checked = 0
    for path, (_, links, _, _) in scanned.items():
        for line, target in links:
            url = urlsplit(html.unescape(target))
            if url.scheme or url.netloc:
                continue
            destination = (path.parent / unquote(url.path)).resolve() if url.path else path
            links_checked += 1
            prefix = f"{path.relative_to(ROOT)}:{line}"
            if not destination.exists():
                errors.append(f"{prefix}: missing target {target}")
            elif url.fragment and destination.suffix == ".md":
                if destination not in scanned:
                    anchors = scan(destination)[0]
                else:
                    anchors = scanned[destination][0]
                if unquote(url.fragment) not in anchors:
                    errors.append(f"{prefix}: missing anchor {target}")
    try:
        tomllib.loads((ROOT / "docs/config.example.toml").read_text())
    except tomllib.TOMLDecodeError as error:
        errors.append(f"docs/config.example.toml: {error}")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    blocks = sum(result[3] for result in scanned.values())
    print(f"docs: {len(pages)} pages, {links_checked} local links, {blocks} TOML blocks and example config passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
