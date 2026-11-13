# Agent brief: Nordend's own SVG icon renderer (replace resvg)

[Documentation](README.md) · [Architecture](architecture.md) · [Current validation policy](svg-renderer-validation.md)

**Historical implementation brief.** The renderer now exists in `src/svg`; the
stage instructions below record the original plan, not work to repeat. Original
line numbers and dependency comparisons belong to that review. The
[validation policy](svg-renderer-validation.md) supersedes the stage-3 pixel-parity
criteria. Current builds have no resvg dependency. Use
[development](development.md) for current build commands.

## Goal

Replace resvg with a small SVG renderer written in this repo, `nordend::svg`, that renders
freedesktop icon-theme SVGs. Build it on FreeType, which Nordend already links
(`CMakeLists.txt:71`). Add no new third-party code: no vendored SVG library, no Rust and no new
system packages. resvg comes out of the build, the Nix package and the README right at the
start, in stage 0. It's used once, by hand, to capture reference images, and never again.

**Why:** resvg pulls a Rust toolchain into the build. We looked at the C++ libraries:
- LunaSVG plus PlutoVG would mean about 16k imported lines and still no filters.
- NanoSVG has no clipping, masks or `use`, flattens groups away, and its README says it isn't
  actively maintained.
- ThorVG is a large engine that only supports `feGaussianBlur`.

PlutoVG's rasterizer and stroker are copied from FreeType anyway. Writing the SVG layer ourselves
on FreeType gives us the subset icon themes need in about 3–4k lines we own.

## Read first

- `src/ui/src/icon_theme.cpp:247-298`: `IconTheme::render`, the only resvg call site. Keep its
  contract:
  - Return straight-alpha (not premultiplied) RGBA in a `picture::Image`.
  - Fit the longest side to `pixels` and keep the aspect ratio.
  - Return an empty image on any failure.
- `src/ui/src/icons.cpp:120-145`: the caller. Rendering happens **in-process, on the UI thread**,
  on demand, so both speed and robustness matter.
- `tests/icon_theme_test.cpp:82-96`: the existing SVG test. It must keep passing unchanged.
- `src/picture/CMakeLists.txt` and its `include/` and `src/` layout: copy this pattern for the
  new module.
- `tests/fuzz/CMakeLists.txt` and `tests/fuzz/fuzz_text.cpp`: how fuzz targets are built.
- `README.md:155-200` for building, `CMakeLists.txt:28-48` for sanitizer and fuzz builds.
- `docs/devlog.md`: newest-first entries. Add one when you finish.
- `.clang-format` and `.clang-tidy`: follow the existing code style and naming.

## Scope: the icon-theme subset

These counts come from the themes installed on the author's machine. Adwaita has 715 SVGs,
candy-icons 3801 and hicolor 103. Of those:
- `clipPath`: 28 in Adwaita, 25 in candy-icons, 4 in hicolor
- `mask`: 17 in Adwaita, 1 in candy-icons
- `filter`: 17 in Adwaita (all `feColorMatrix`), 3 in hicolor (all `feGaussianBlur`)
- `<style>`: 384 in candy-icons
- `currentColor`: 108 in candy-icons
- `use`: 8 in candy-icons, 4 in hicolor
- `text`: 15 in candy-icons
- `image`: 13 across the three
- `pattern`: 1 in hicolor

Every Adwaita filter is the same pattern: a group inside a `<mask>` gets
`<feColorMatrix values="0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0 0 0 1 0"/>`, which turns a
luminance mask into an alpha mask. Getting this wrong makes parts of icons such as
`user-trash`, `printer` and `package-x-generic` fade or disappear.

**Out of scope:**
- Animation, scripting, interactivity and links.
- External resources: any `href` that isn't a `#fragment` or a `data:` URI is ignored.
- DTDs and custom entities.
- CSS beyond what's listed below.
- `<text>`: render nothing for it, but don't fail the rest of the document.
- `foreignObject`, and filter primitives other than the two named below.

## Architecture

New module `src/svg/`, added in the top `CMakeLists.txt` next to `src/picture`:

```
src/svg/include/nordend/svg/svg.hpp   public API
src/svg/src/xml.cpp        minimal XML tokenizer
src/svg/src/css.cpp        style attribute, presentation attributes, <style> class/type/id selectors
src/svg/src/parse.cpp      element tree, units, viewBox, path data, transforms, colours
src/svg/src/paint.cpp      solid and gradient span painters
src/svg/src/render.cpp     tree walk, groups, layers, clip, mask, filters
src/svg/src/raster.cpp     FreeType wrapper: FT_Outline building, FT_Stroker, direct span rendering
```

Public API, roughly. Adjust the names to the codebase's style:

```cpp
namespace nordend::svg {
// Fits the document's longest side to `pixels`, keeping its aspect ratio.
// Returns straight-alpha RGBA, or an empty image on any parse or limit failure.
picture::Image render(std::span<const std::uint8_t> data, int pixels);
}
```

Link it as `PRIVATE PkgConfig::FREETYPE nordend::picture nordend::warnings`.

**Rasterizing with FreeType:**
- Build an `FT_Outline` per shape, converting arcs to cubic Béziers. Use 26.6 fixed point after
  applying the full transform, and set `FT_OUTLINE_EVEN_ODD_FILL` for `fill-rule="evenodd"`.
- Render with `FT_Outline_Render` and `FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT`. The span
  callback composites coverage × paint into the current layer.
- Stroke with `FT_Stroker`, which handles joins, caps and the miter limit. FreeType has no
  dashing, so split paths into dashes before stroking.
- Work in premultiplied RGBA internally, and convert to straight alpha once at the end.
- Use one `FT_Library` per thread or per call. Nothing global and mutable.

## Stages

Do them in order. Each stage ends with tests passing and its acceptance criteria met.
`IconTheme::render` uses `nordend::svg` from stage 0 on. The app has five users, and icons that
render wrongly between stages are accepted.

### Stage 0: capture references, then remove resvg
1. **The one resvg run.** Write `scripts/svg-capture-references.sh` and run it once through
   `nix shell nixpkgs#resvg`. resvg is never added to the flake, CMake or the dev shell.
   - Render every SVG in the installed icon themes (`$XDG_DATA_HOME/icons`, each
     `$XDG_DATA_DIRS/icons`, and `~/.icons`) at 16, 48 and 128 px. Write the PNGs to
     `tests/data/svg-corpus/<theme>/<size>/<path>.png` and add `tests/data/svg-corpus/` to
     `.gitignore`. These are renders of third-party icons with their own licences: they stay
     local and are never committed. Store a `manifest.tsv` alongside them with source path,
     source sha256, size and resvg's render time, so later checks can see when a theme's file
     has changed.
   - Render the hand-written unit-test SVGs from `tests/data/svg/*.svg` to
     `tests/data/svg/*.png`. These **are** committed. Write the stage 1 test SVGs before this
     run. If a later stage needs another reference, write its test SVGs now as well, or get the
     expected values from the spec in an exact-pixel test, without resvg.
   - Record resvg's total render time per size in the devlog. That's the speed baseline.
2. **Remove resvg in the same commit series.**
   - Delete the resvg code and the un-premultiply loop from `src/ui/src/icon_theme.cpp`, and
     call `nordend::svg::render`. At this point it can be a stub that returns an empty image, or
     the first piece of stage 1.
   - Remove resvg from `CMakeLists.txt:79-80`, `src/ui/CMakeLists.txt:88-89`, `nix/package.nix`
     (the input and `buildInputs`) and `README.md:257-258`.
   - Link `nordend::svg` from `nordend_ui`.
   - `nix build` must pass without resvg. `tests/icon_theme_test.cpp:82-96` may fail until
     stage 1 makes it pass, and must then pass unchanged.

### Stage 1: shapes, paint and structure
- XML: elements, attributes, self-closing tags, comments, `<?…?>` processing instructions,
  CDATA (used in `<style>`), the five predefined entities and numeric character references.
  Namespaced names such as `svg:path` and `xlink:href` map to their local names. Skip any
  `<!DOCTYPE>` without expanding it.
- Styling:
  - Presentation attributes, the `style=""` attribute, and `<style>` rules with type, `.class`
    and `#id` selectors (including comma-separated lists).
  - Cascade order: presentation attribute < stylesheet < `style` attribute.
  - Inheritance, and `inherit`.
- Colours: the full set of SVG named colours, `#rgb`, `#rrggbb`, `rgb()` and `rgba()` in
  integers and percentages, `none` and `currentColor` (black unless `color` is set).
- Paint properties: `fill`, `stroke`, `opacity` on shapes, `fill-opacity`, `stroke-opacity`,
  `fill-rule`, `stroke-width`, `stroke-linecap`, `stroke-linejoin`, `stroke-miterlimit`,
  `stroke-dasharray`, `stroke-dashoffset`, `display` and `visibility`.
- Shapes: `path` (every command, absolute and relative, with implicit repeats and compact number
  syntax such as `1.5.5` and `1e-3`), `rect` (including `rx`/`ry`), `circle`, `ellipse`, `line`,
  `polyline` and `polygon`.
- Geometry:
  - `transform`: matrix, translate, scale, rotate with and without a centre, skewX and skewY.
  - `viewBox`, every `preserveAspectRatio` value, and `width`/`height` in px, pt, pc, mm, cm,
    in or %.
  - A missing size falls back to the viewBox. With neither, fail.
- Gradients:
  - `linearGradient` and `radialGradient`, including `fx`/`fy`/`fr`.
  - `gradientUnits` (bounding box and user space), `gradientTransform` and `spreadMethod`
    (pad, reflect, repeat).
  - `stop` with `offset`, `stop-color` and `stop-opacity`.
  - `href` inheritance of stops and attributes, with cycle detection.
  - `fill="url(#id) fallback"`.
- Structure: `defs`, `g`, `use` (with x/y, and `href` to an element or a `symbol` with its own
  viewBox) and `symbol`.

### Stage 2: group compositing, clipping and masks
- Group opacity and `opacity` on `use`: render the group to an offscreen layer, then composite
  it. Skip the layer when it isn't needed.
- `clip-path` and `clipPath`: `clipPathUnits`, `clip-rule`, transforms on the clip path and its
  children, a `clip-path` on the clip path itself, and `use` inside a clip path. Build a
  coverage mask and multiply by it.
- `mask`: `maskUnits` and `maskContentUnits`, the x/y/width/height region, `mask-type` luminance
  (the default) and alpha, and nested masks. Compute luminance from un-premultiplied colour, the
  way the spec says.

### Stage 3: filters, patterns, embedded images
- `filter` with `filterUnits`, `primitiveUnits` and the filter region:
  - `feColorMatrix`: `matrix`, `saturate`, `hueRotate` and `luminanceToAlpha`. Apply it to
    un-premultiplied colour.
  - `feGaussianBlur`: `stdDeviation` with one or two values, as three box blurs. Expand the
    region by 3σ.
  - Support `in` set to `SourceGraphic` or `SourceAlpha` and `result` chaining between those
    two primitives.
  - A filter containing any other primitive: render the element unfiltered and log it once per
    file through the existing logging.
- `pattern`: render the tile to a layer and sample it, with `patternUnits`,
  `patternContentUnits`, `patternTransform` and viewBox.
- `<image>` with `data:image/png` or `data:image/jpeg` base64 only, decoded through the existing
  wuffs path in `nordend::picture`. Any other image source is ignored.

## Hardening (required)

Input comes from user-writable directories (`~/.local/share/icons`, `~/.icons`) and is parsed on
the UI thread. Treat it as untrusted:
- **Size and complexity limits:** reject input above the existing 8 MiB cap. Cap the element
  count (for example 100k), nesting depth (for example 256) and total path segments (for example
  1M).
- **Reference loops and expansion:** detect cycles in `use`, gradient and pattern `href`, and in
  `clip-path`, `mask` and `filter` references. Cap the expansion `use` can cause (for example
  10k expanded nodes) so a "billion laughs" chain fails.
- **Render limits:** cap the number of live offscreen layers and their total memory (for example
  64 MiB), and clamp blur σ.
- **Numbers:** reject NaN and infinity, and handle overflow when converting to 26.6 fixed point.
- **No fetching:** never read files or the network.
- **Fail cleanly:** never throw out of `render`, and never crash. Every failure returns an empty
  image.
- **Fuzzing:** add `tests/fuzz/fuzz_svg.cpp` and the `nordend-fuzz-svg` target with a seed
  corpus. It must run clean under `-DNORDEND_SANITIZER=address,undefined`, and in a fuzz build
  for at least 30 minutes at the end of each stage. Because the renderer ships to users from
  stage 0 on, a stage isn't finished until its fuzz run is clean. Report the run length and
  outcome.

## Testing

- **Unit tests** (`tests/svg_test.cpp`, doctest): write the tests for each feature before the
  code. Use small hand-written SVGs and check exact pixels where the geometry allows it: solid
  rectangles, gradient endpoints and midpoints, clip edges, the Adwaita mask pattern, and
  rejection on every hardening limit.
- **Reference images:** the committed PNGs from stage 0 (`tests/data/svg/*.png`). Compare with a
  tolerance for anti-aliasing: no pixel channel off by more than 64, and at most 1% of pixels off
  by more than 24. Put the comparison in a small test helper. resvg is never run again after
  stage 0.
- **Do not commit third-party icons, or renders of them.** Adwaita and the others have their own
  licences.
- **Local corpus check:** add `scripts/svg-corpus.sh`. It renders every icon listed in
  `tests/data/svg-corpus/manifest.tsv` with `nordend-svg-render`, a small dev-only tool you add
  under `src/tools/`, and compares the result with the captured reference.
  - Icons whose source sha256 no longer matches the manifest are reported as stale and left out.
    They are not re-rendered with resvg.
  - The script reports the pass rate at the tolerance above, lists the worst 20 by difference,
    and compares total render time with the resvg baseline in the manifest.
  - Run it at 16, 48 and 128 px at the end of every stage and record the results in the devlog.
    If the capture is missing (on a fresh machine, say), the script says so and exits without
    failing. It's a local check, not part of `ctest`.
- **Final acceptance** (end of stage 3):
  - At least 98% of the captured icons pass at each size.
  - All 17 filtered Adwaita icons and the 3 hicolor ones pass.
  - Total render time is no more than 1.5× the resvg baseline.
  - Every remaining failure is listed with its cause.

## Every stage ends with

- `nix build` and `ctest --test-dir build --output-on-failure` passing, and a sanitizer build's
  tests passing.
- The corpus check run and its numbers recorded.
- A clean fuzz run.
- A devlog entry in the existing style: what was done, the decisions, the corpus numbers,
  timings, the fuzz run and open points.

## Working rules

- Work on a branch off `main`. Commit at least once per stage with plain, descriptive messages.
  Don't add `Co-Authored-By` lines or any AI or tool attribution to commits, PRs or comments.
- Build and test inside `nix develop` (`README.md:167-170`).
- Don't change behaviour outside SVG rendering. If you find a bug elsewhere, note it in the
  devlog instead.
- If something in this brief turns out to be wrong, such as a FreeType API that doesn't behave as
  described here or a threshold that can't be met, stop and report what you found and what you
  propose. Don't quietly lower the bar.
- **Final report:** what's done per stage, the corpus pass rates and timings at each size, the
  icons that still fail and why, the fuzz run, and anything left out or deferred.
