# Development

[Documentation](README.md) · [Contributing](../CONTRIBUTING.md) · [Architecture](architecture.md)

## Build environment

Use the [installation dependency list](install.md#build-dependencies), or enter
`nix develop`. The shell supplies build dependencies, doctest, clang tools, GDB
and the optional JPEG benchmark dependency. It does not configure a build on entry.

Use separate directories for different configurations:

```sh
cmake -B build-dev -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-dev
ctest --test-dir build-dev --output-on-failure
./build-dev/rummage ~/Pictures
```

The GUI requires a Wayland session; many tests use offscreen rendering and require
working EGL/GLES and fonts even without a compositor. CMake generates
`compile_commands.json` in the build directory. Point your editor at that file.

## CMake options

| Option | Default | Purpose |
| --- | --- | --- |
| `CMAKE_BUILD_TYPE` | `Release` when unspecified for a single-config generator | Use `Debug` for local diagnosis |
| `CMAKE_INSTALL_PREFIX` | CMake default | Set explicitly for installation/portal activation |
| `RUMMAGE_BUILD_TESTS` | `ON` | Build tests if doctest is found |
| `RUMMAGE_WERROR` | `ON` | First-party warnings are errors; package builds may disable |
| `RUMMAGE_SANITIZER` | Empty | e.g. `address,undefined` or `thread` |
| `RUMMAGE_CLANG_TIDY` | `OFF` | Run clang-tidy while compiling |
| `RUMMAGE_MEDIA` | `OFF` | Build the optional sandboxed FFmpeg helper |
| `RUMMAGE_FUZZ` | `OFF` | Build libFuzzer targets; requires Clang |

Tests are silently absent from a later `ctest` run if configure skipped them;
read the configure notice and check `ctest --test-dir build-dev -N`.

## Tests

Most tests are doctest cases in `build-dev/tests/rummage-tests`, registered as a main CTest test plus a separate memory-test process. Filter cases at the executable level:

```sh
./build-dev/tests/rummage-tests --test-case='picker:*'
./build-dev/tests/rummage-tests --test-case='actions:*'
./build-dev/tests/rummage-tests --test-case='shortcuts:*'
./build-dev/tests/rummage-tests --test-case='the example config parses clean and matches the defaults'
```

Use `--list-test-cases` to find names rather than relying on a fixed count. The
portal and notification integration executables use a private D-Bus session;
CTest registers them when `dbus-run-session` is found:

```sh
ctest --test-dir build-dev -R 'file-picker-portal|action-notifications' --output-on-failure
```

Portal tests drive the real backend with a fake child. They do not replace a
manual check of an application's Open/Save dialog on the intended desktop.

The Nix test derivation supplies fonts, shared MIME data, software rendering and
archive tools. It runs CTest, including portal/notification integrations; the
allocator-sensitive test runs in a fresh process. Direct dev-shell tests use the
host environment and can differ. The normal package omits tests; `nix flake check`
checks a separate test build with media enabled, the installed package and the module.
See [CI/package validation](ci-packaging.md) for exact checks and distribution builds.

```sh
nix flake check
```

This checks the tests, installed package and module defined in the flake. In a Git-backed flake,
untracked source files can be excluded from the input; ensure intended source
files are included before interpreting a Nix build as verification of your work.

## Sanitizers and static analysis

```sh
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DRUMMAGE_SANITIZER=address,undefined
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure

cmake -B build-tsan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DRUMMAGE_SANITIZER=thread
cmake --build build-tsan
ctest --test-dir build-tsan --output-on-failure
```

Keep ASan/UBSan and TSan in separate builds. Sanitizer runtimes change memory
requirements and helper sandbox allowances; see [security](security.md).
Known third-party leak suppressions live in
`src/core/src/lsan_suppressions.cpp`. Record suppressions and environment workarounds
when reporting results.

Enable build-time analysis with `-DRUMMAGE_CLANG_TIDY=ON`. Check formatting with
`scripts/format.sh --check`. Neither formatter nor tidy covers documentation;
use `python3 scripts/check-docs.py` for its mechanical checks.

## Fuzzing

```sh
CC=clang CXX=clang++ cmake -B build-fuzz -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DRUMMAGE_FUZZ=ON \
  -DRUMMAGE_WERROR=OFF -DRUMMAGE_BUILD_TESTS=OFF
cmake --build build-fuzz
mkdir -p /tmp/rummage-fuzz-tiff-corpus
./build-fuzz/tests/fuzz/rummage-fuzz-tiff /tmp/rummage-fuzz-tiff-corpus \
  tests/data/image/tiff -max_total_time=60
```

The example is a smoke run, not a release qualification. Targets are `tiff`,
`camera`, `picture`, `dng`, `ljpeg`, `text` and `svg` with the
`rummage-fuzz-` prefix. Keep generated corpus files separate from committed seeds;
preserve crash inputs and reproduce them in the same instrumented build.
Camera corpora may require local samples; SVG corpus references are not all shipped.
SVG changes have the longer fuzzing and visual requirements in
[their validation policy](svg-renderer-validation.md).

## Diagnostic tools

These CMake targets are development executables and are not installed by the
normal application install. Run them from the build directory (prefix examples
with `./build-dev/`).

| Tool | Purpose |
| --- | --- |
| `rummage-snapshot` | Offscreen PPM renders of the scene/browser, interaction fixtures and scrolling measurements |
| `rummage-svg-render` | Render one SVG to PNG; optional reference comparison |
| `rummage-bench-start` | Time startup of a specified command |
| `rummage-bench-dir` | Directory-loading benchmark and fixture generation |
| `rummage-bench-find` | Recursive fuzzy-search measurements |
| `rummage-mime` | Print file MIME classification |
| `rummage-watch` | Observe live directory-model changes |
| `rummage-decode` | Decode/inspect images and raw previews with timings |
| `rummage-thumb` | Exercise the thumbnail cache |
| `rummage-open` | Inspect default-application resolution; use `--dry-run` to avoid launching |
| `rummage-transfer` | Exercise real copy/move operations on disposable fixtures |
| `rummage-bench-jpeg` | Optional wuffs/TurboJPEG comparison |
| `tests/rummage-bench-previews` | Optional exiv2/reference comparison |

Examples:

```sh
./build-dev/rummage-snapshot --columns ~/Documents /tmp/rummage.ppm 960 640 1
./build-dev/rummage-snapshot --columns ~/Documents --picker save /tmp/picker.ppm 960 640 1
./build-dev/rummage-bench-start -n 5 -- ./build-dev/rummage ~/Pictures
./build-dev/rummage-open --dry-run ~/Documents/notes.txt
```

Not every development tool implements `--help`; inspect its `src/tools/*.cpp`
usage string before adding options. Snapshot output checks pixels, not compositor
input or desktop interoperability. Record dimensions, scale, theme and fixtures.

`rummage-bench-start` uses `RUMMAGE_STARTUP_NS` and `RUMMAGE_STARTUP_QUIT` to collect
application startup marks. Prefer the wrapper to setting them by hand. Keep
timings separate from competing builds/fuzz runs and label warm/cold caches,
hardware versus software rendering, and on-screen versus offscreen measurements.
See [benchmarks](benchmarks.md) for historical methods and results.
