# Contributing

[Documentation](docs/README.md) · [Development](docs/development.md) · [Architecture](docs/architecture.md)

Start with the code and tests for the affected module. rummage is a C++20 Wayland
application with its own UI; changes should preserve responsive browsing, clear
file-operation behavior and preview isolation. Keep unrelated work separate and
describe visible behavior and limitations in the change.

## Build and verify

The [development guide](docs/development.md) gives the complete dependency,
configuration and test workflow. A typical local start is:

```sh
nix develop
cmake -B build-dev -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-dev
ctest --test-dir build-dev --output-on-failure
```

Use the repository's `.clang-format` and `.clang-tidy` settings. Check formatting
with `scripts/format.sh --check`; `scripts/format.sh` rewrites tracked C++ files,
so avoid running it across unrelated unfinished edits. New untracked files need
formatting explicitly until they are tracked.

For behavioral changes, add or update focused tests that demonstrate the outcome.
Run the relevant tests, then the appropriate broader checks. Renderer changes
also need visual inspection; a screenshot does not establish keyboard, clipboard
or portal behavior. Parser/sandbox changes need sanitizer and adversarial-input
coverage. SVG changes follow the existing
[SVG validation policy](docs/svg-renderer-validation.md).

## Before proposing a change

- State the problem and resulting user-visible behavior.
- Explain relevant tradeoffs, dependency changes and compatibility effects.
- List checks actually run and any failures or environment limits.
- Update the relevant guide, configuration example and tests alongside behavior.
- For measured improvements, record revision, machine, build mode and method.

Do not call a test suite successful if doctest was absent and CMake skipped it.
Do not promote old benchmark numbers to measurements of a new build.

## Reports

For bugs, include steps to reproduce, expected/actual behavior, `rummage --about`,
and relevant stderr output. Say whether the binary is a release package, a Nix
build or a locally modified checkout. For preview bugs, identify the format and
provide a shareable minimal sample when possible. Remove private paths/content
from anything you publish. See [troubleshooting](docs/troubleshooting.md).

## Documentation

Documentation is Markdown rendered by the repository browser; no site generator
or publishing step is required. [docs/README.md](docs/README.md) is the index.

Keep the root README as an introduction and entry point. Put procedures in topic
guides and detailed option lists in references. Add a link from the documentation
index for a new page; use relative links and descriptive text. Prefer stable
source-file links over line numbers that drift.

Current behavior belongs in the guides. Release history, dated benchmark results,
development logs and review briefs retain their context; do not silently rewrite
historical observations as though they describe the current tree. Add an explicit
correction or superseding reference when a historical statement is misleading.
Private `DESIGN.md`/`ROADMAP.md` notes and step numbers are historical context, not
required public documentation. The public [architecture](docs/architecture.md)
and [roadmap](docs/roadmap.md) should stand on their own.

When changing configuration, update defaults/ranges in
[configuration.md](docs/configuration.md), recipes in
[config.example.toml](docs/config.example.toml), and the affected topic guide.
Check TOML examples and `--check-config`; the existing config test verifies that
the active example matches defaults. When changing commands or shortcuts, update
their references from `src/app/main.cpp` and `src/views/src/shortcuts.cpp`.

Run `python3 scripts/check-docs.py` (Python 3.11+) to check local links, heading
anchors and TOML code blocks in the public documentation. It does not check external URLs or
prove that instructions match runtime behavior; source review and appropriate
focused checks are still needed.

## Project history and credits

Nordend’s upstream author records Claude Opus assistance in code, analysis, tests and
documentation, with design decisions, hardware testing and releases remaining
the author's. The [devlog](docs/devlog.md) preserves that development history.
Contributions should be reviewable on their behavior and evidence regardless of
the tools used. Bundled material and licenses are listed in
[THIRD_PARTY.md](THIRD_PARTY.md).
