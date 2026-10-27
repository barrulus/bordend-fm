# Nordend

**A native, Wayland-only file manager for the [Noctalia](https://github.com/noctalia-dev) ecosystem.**

![status](https://img.shields.io/badge/status-pre--alpha-orange)
![phase](https://img.shields.io/badge/phase%203-complete-3DA639)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C?logo=cmake&logoColor=white)
![Wayland](https://img.shields.io/badge/Wayland-native-FFBC00?logo=wayland&logoColor=black)
![OpenGL ES](https://img.shields.io/badge/OpenGL%20ES-3.0-5586A4?logo=opengl&logoColor=white)
![doctest](https://img.shields.io/badge/tests-doctest-4B32C3)
![code license](https://img.shields.io/badge/code-MIT-3DA639)
![platform](https://img.shields.io/badge/platform-Linux-FCC624?logo=linux&logoColor=black)

## Goals

- **Native and fast:** C++20, Wayland + EGL/OpenGL ES, no Qt or GTK. Directory loading and
  thumbnailing never block the UI.
- **Finder-style column view** (Miller columns) and a thumbnail grid.
- **Quick Look:** press <kbd>Space</kbd> to preview a file, including the embedded JPEG of camera
  RAW files.
- **Noctalia integration:** follows the Noctalia color palette and reloads it live.
- **Keyboard first**, mouse always.

## Features

What already works, the way it is meant to stay:

- **Natural touch.** Scrolling that feels physical: on a touchpad the list follows your fingers
  1:1 and, when they lift, keeps gliding and slows down smoothly, stopping softly at the ends;
  a mouse wheel glides instead of jumping, and quick turns add up. The keyboard moves the same
  way: arrows and Page Up/Down glide, Home/End jump. Only the visible rows are drawn (0.2 ms
  per frame in a 10 000-file folder), and nothing is drawn when nothing moves.
- **Columns, as in Finder.** Every folder you pick opens in the next column; arrows walk the
  tree, Left from the first column climbs to the parent. Columns resize by dragging their edge
  and slide sideways by themselves to keep where you are in view. A file shows its details at
  the end.
- **Selection as you expect it.** Click, Ctrl+click, Shift+click, or the keyboard alone:
  Shift+arrows for a range, Ctrl+arrows and Ctrl+Space to pick here and there, Ctrl+A for all.
  A selection of many files shows how many and how big.
- **A grid when you want to see more.** Ctrl+2 lays the folder out as large icons, Ctrl+1
  brings the columns back: where you were and what you selected come along. Ctrl+plus/minus
  (or Ctrl+wheel) make the cells bigger or smaller, from 48 to 256 px; the arrows, Enter and
  Backspace walk the folders as in the columns.
- **Live folders.** Files created, removed or renamed by any program appear at once; a
  thousand-file copy arrives in a few smooth updates, not a thousand.
- **The right icon for every file**, the same line icons as Noctalia, tinted by your theme:
  camera RAW files and darktable sidecars have their own. File types match GTK apps exactly.
- **Your theme, live.** Change the palette in Noctalia and Nordend follows instantly.

## Progress

`nordend [DIR]` opens a folder (default: your home) in Finder-style columns;
`nordend --test-scene` shows everything the renderer can draw.

**Phase 1 — window and rendering: complete.**

- Wayland window (xdg-shell) with server-side decorations
- EGL + OpenGL ES 3.0 rendering, paced by frame callbacks: idle CPU at zero, smooth resizing
- Fractional scaling (fractional-scale-v1 + viewporter): sharp at 1, 1.25, 1.5, 2
- Drawing primitives in one SDF shader: rounded boxes with per-corner radii, borders, soft
  shadows, nested clipping, physical-pixel hairlines
- Text: fontconfig fallback, harfbuzz shaping, glyph atlases; accents, color emoji, CJK, invalid
  UTF-8, end and Finder-style middle truncation

**Phase 2 — input, loop and theme: complete.**

- Keyboard (xkbcommon, the compositor's layout, compose, key repeat) and pointer (click
  counting, smooth touchpad scrolling, cursor-shape-v1)
- One `poll()` event loop and a worker pool with priorities and cancellation (TSan clean)
- `~/.config/nordend/config.toml` with readable errors; `nordend --check-config`
- Live Noctalia theme: a Noctalia template writes `colors.toml`, Nordend reloads it on change

**Phase 3 — file system: complete.**

- Asynchronous directory loading in chunks with natural sort (10k files: first entries in
  0.8 ms, see [benchmarks](docs/benchmarks.md))
- MIME types from shared-mime-info, identical to GLib's on about 4 750 files compared
- Live updates through one shared inotify instance, renames recognised, bursts batched
- File-type icons from Noctalia's Tabler set, tinted by the theme; camera RAW files and
  darktable sidecars have their own

**Phase 4 — views: complete.**

- Virtualized scrolling list with touchpad inertia and wheel glide (0.2 ms per frame in a
  10 000-file folder)
- Miller columns: arrow-key navigation, resizable columns, animated horizontal scrolling, an
  info pane for the selected file
- Selection: click, Ctrl+click, Shift+click, and the same from the keyboard (Shift+arrows,
  Ctrl+arrows and Ctrl+Space, Ctrl+A); a summary of what is selected
- Grid view: virtualized cells, size from 48 to 256 px (Ctrl+plus/minus, Ctrl+wheel, Ctrl+0),
  Ctrl+1 / Ctrl+2 switch columns and grid keeping cursor and selection

**Next:** image decoding and thumbnails (phase 5). The full plan is in the
[roadmap](docs/ROADMAP.md).

## Benchmarks

Measured, not assumed. Opening a folder of **10 000 files** on a SATA SSD:

| Cache | First entries ready | Whole folder loaded and sorted | Target for the first frame |
|-------|--------------------:|-------------------------------:|---------------------------:|
| warm (median of 20) | **0.8 ms** | **19.5 ms** | < 50 ms |
| cold (after `drop_caches`) | **1.7 ms** | **81 ms** | < 50 ms |

Method, machine, cold-cache numbers and more folders: [docs/benchmarks.md](docs/benchmarks.md).

## Building

Void Linux package names:

```sh
sudo xbps-install -S cmake ninja pkgconf wayland-devel wayland-protocols \
    libxkbcommon-devel libglvnd-devel MesaLib-devel \
    harfbuzz-devel freetype-devel fontconfig-devel tomlplusplus-devel
cmake -B build -G Ninja
cmake --build build
./build/nordend            # your home folder; ./build/nordend ~/Pictures for another one
```

Needs a Wayland compositor with xdg-shell (developed on
[Umbriel](https://github.com/noctalia-dev)). `NORDEND_LOG=debug` (or `trace`) shows more detail.

Development builds:

```sh
# AddressSanitizer + UBSan (needs libsanitizer-devel)
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNORDEND_SANITIZER=address,undefined
cmake --build build-asan

# ThreadSanitizer
cmake -B build-tsan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNORDEND_SANITIZER=thread
cmake --build build-tsan

# Unit tests (needs doctest-devel), in any build directory
ctest --test-dir build

# Render the test scene offscreen, no window: <out.ppm> [width height scale [colors.toml]]
./build/nordend-snapshot scene.ppm 960 820 1.5

# Folders in columns offscreen, opened down to a path, and the scrolling benchmark
# (see docs/benchmarks.md)
./build/nordend-snapshot --columns ~ --reveal ~/Pictures columns.ppm
./build/nordend-snapshot --columns /tmp --reveal /tmp/bench-10k --bench 600 columns.ppm

# Directory loading benchmark (see docs/benchmarks.md)
./build/nordend-bench-dir --make 10000 /tmp/bench-10k
./build/nordend-bench-dir --runs 20 /tmp/bench-10k
```

## Documentation

- [Design](docs/DESIGN.md)
- [Roadmap](docs/ROADMAP.md)
- [Devlog](docs/devlog.md)
- [Benchmarks](docs/benchmarks.md)
- [Upstream bug log](docs/buglog.md)

## Development

Nordend is copiloted by **Claude Opus** (Anthropic), which assists with code writing, code analysis,
bug hunting, testing and documentation. Design decisions, testing on real hardware and every release
are the author's.

## License

[MIT](LICENSE)
