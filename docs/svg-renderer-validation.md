# SVG renderer acceptance

On 2026-11-10 the author approved ending the pixel-parity effort, returning stroke
expansion to FreeType, and judging a fixed set of icons at native size alongside
geometric correctness and performance tests. This supersedes the original stage-3
requirements for 98% corpus agreement and pixel passes for every filtered icon.

The captured rasters and numeric comparator remain unchanged. Corpus percentages
are diagnostic measurements, not release gates. Changes to this policy must be
recorded explicitly; improving a percentage alone does not justify more custom
stroke or raster approximation code.

## Required checks

- All SVG semantic, geometric, API, bounds and committed original-fixture tests
  pass. Keep exact pixels where geometry supports them. Keep the existing fixture
  comparator; do not recapture or replace references to accommodate changes.
- Full release tests, ASan/UBSan tests, and the Nix package build pass, with the
  existing documented unrelated test accommodations. A final-source sanitizer
  fuzz run lasts at least 1800 seconds with no new finding, timeout or OOM.
- Total parse/render time stays within 1.5 times each preserved corpus baseline
  at 16, 48 and 128 px. Measure after competing builds and fuzzing stop.
- Review every entry in `tests/data/svg-review.tsv` at native 16, 48 and 128 px on
  light and dark backgrounds. The fixed selection includes all 17 filtered
  Adwaita icons, all three filtered hicolor icons, prior problem silhouettes,
  thin strokes, patterns, embedded images and a known sizing-contract difference.
  Do not replace entries because they look worse or have low pixel scores.

## Visual acceptance

An icon must retain its identity, overall silhouette, intended content and usable
contrast at its display size. Missing components, unexpected holes, clipping,
broken masks, misplaced images, missing patterns, unreadable outlines and broad
unexplained colour loss block acceptance. Also record rendering errors encountered
outside the fixed selection; this set is not permission to ignore known defects.

Differences in antialiasing, subpixel edge placement and fine stroke weight can be
accepted after inspection when identity, content and usability are retained.
Known unsupported text remains out of scope. Public aspect-ratio sizing continues
to round; the old CLI's ceiling-sized reference does not override that contract.
Enlargements help explain a difference but do not substitute for native-size review.

Record the renderer hash, selection, review result, named deviations and any open
defects. A contact sheet alone is not evidence that the review passed. If the
simpler backend has unacceptable native-size defects, report them and revisit the
renderer constraints; do not restart a custom pixel-matching loop.

## Reproduction

Inside `nix develop`, after building `nordend-svg-render`:

```sh
python3 scripts/svg_review.py --output build/svg-review
python3 scripts/svg_corpus.py --report build/svg-review/corpus.json
```

The review command verifies source hashes and requires all selected captures. It
creates a self-contained native-size comparison and a JSON rendering inventory.
Missing or changed sources make the review incomplete; they are never silently
replaced. Neither command invokes resvg. The full corpus still records every alias,
numeric failure and timing. Keep third-party sources and generated reviews local
under ignored build directories; only selection identifiers are committed.
