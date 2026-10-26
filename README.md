# Nordend

**A native, Wayland-only file manager for the [Noctalia](https://github.com/noctalia-dev) ecosystem.**

![status](https://img.shields.io/badge/status-pre--alpha-orange)
![phase](https://img.shields.io/badge/phase%202-complete-3DA639)
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

## Progress

**Phase 1 — window and rendering: complete.** There is no file manager yet: the window shows a
test scene of everything the renderer can draw.

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

**Now — phase 3, file system:** asynchronous directory loading (chunks, natural sort), MIME
types identical to GLib's, and live updates through inotify are in; file icons are next. The full plan is in the
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
./build/nordend
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
