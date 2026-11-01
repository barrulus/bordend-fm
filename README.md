<h1 align="center">Nordend</h1>

<p align="center">
  <b>A fast, native file manager for Wayland, made for photographers and the
  <a href="https://github.com/noctalia-dev">Noctalia</a> desktop.</b><br>
  No GTK, no Qt: C++20 straight on Wayland and OpenGL ES.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-pre--alpha-orange" alt="status: pre-alpha">
  <img src="https://img.shields.io/badge/next-MVP-3DA639" alt="next: MVP">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white" alt="C++20">
  <img src="https://img.shields.io/badge/Wayland-native-FFBC00?logo=wayland&logoColor=black" alt="Wayland native">
  <img src="https://img.shields.io/badge/OpenGL%20ES-3.0-5586A4?logo=opengl&logoColor=white" alt="OpenGL ES 3.0">
  <img src="https://img.shields.io/badge/CMake-3.20%2B-064F8C?logo=cmake&logoColor=white" alt="CMake 3.20+">
  <img src="https://img.shields.io/badge/tests-272%20%C2%B7%20ASan%20%C2%B7%20TSan-4B32C3" alt="272 tests, ASan, TSan">
  <img src="https://img.shields.io/badge/license-MIT-3DA639" alt="MIT license">
  <img src="https://img.shields.io/badge/platform-Linux-FCC624?logo=linux&logoColor=black" alt="Linux">
</p>

> [!IMPORTANT]
> **Pre-release.** Nordend has not been released yet: there are no packages and no tagged
> versions. To try it, [build it from source](#building); it takes about a minute. The first
> release will be 0.1.0.

<p align="center">
  <a href="https://pankaspe.github.io/nordend-fm/"><img src="https://img.shields.io/badge/Read_the_guide-pankaspe.github.io%2Fnordend--fm-FFF59B?style=for-the-badge&labelColor=0E0E43" alt="Read the guide on pankaspe.github.io/nordend-fm"></a>
</p>

<p align="center">
  <img src="docs/images/screenshot-columns.png" alt="Nordend's column view on a folder of OM System raw photos, the photo and its EXIF in the pane at the right" width="100%">
  <br>
  <sub>The column view on a folder of OM System raw files: the camera's own preview, the shot's
  EXIF and the darktable sidecars, in the Noctalia theme on Umbriel.</sub>
</p>

<p align="center">
  <img src="docs/images/screenshot-quick-look.png" alt="Quick Look showing a raw photo of a rocky summit in the snow over the column view" width="100%">
  <br>
  <sub>Quick Look (<kbd>Space</kbd>) on the same raw file: the full preview in a fraction of a
  second, with its name, size and dimensions.</sub>
</p>

## Why Nordend

Nordend talks to the compositor directly: Wayland and OpenGL ES, **no toolkit at all**. Text,
icons, scrolling and animations come from its own renderer, so it does exactly what it is
designed to do and nothing gets in the way. It belongs to the Noctalia family (Noctalia Shell,
the Umbriel compositor) and wears your Noctalia theme, live; elsewhere it takes your desktop's
light or dark style and accent colour.

It is made for people who browse a lot of files, photos above all:

- **Photos first.** Thumbnails, the preview pane and Quick Look (<kbd>Space</kbd>) read the
  previews cameras store in their raw files (OM System, Canon, Nikon, Sony, Fujifilm,
  Panasonic, Hasselblad), upright and in the shape they were shot, with the shot's EXIF and your
  darktable rating. darktable sidecars travel with their photo through every rename, copy, move
  and trip to the trash.
- **Three views of the same folder.** Finder-style columns (<kbd>Ctrl</kbd>+<kbd>1</kbd>) to
  walk a tree from the keyboard, a thumbnail grid (<kbd>Ctrl</kbd>+<kbd>2</kbd>) to see a folder
  of photos, a details table (<kbd>Ctrl</kbd>+<kbd>3</kbd>) to compare sizes and dates;
  switching keeps your place and your selection.
- **Small and fast, and measured.** A 3 MB binary; a 10 000-file folder shows its first
  entries in 0.7 ms; a scrolling frame costs a fraction of a millisecond; a 20 MP raw photo opens
  in Quick Look in under 100 ms; copies run as fast as `cp`; and when nothing moves Nordend uses
  no CPU at all. See [Benchmarks](#benchmarks).
- **Natural scrolling.** The touchpad moves the list 1:1 under your fingers and lets it glide
  when they lift; wheel and keyboard glide too. Nothing jumps.
- **Careful with your files.** Nothing is ever overwritten without asking, nothing
  half-copied ever carries a real name, a real freedesktop trash, a card before anything is
  deleted for good, and <kbd>Ctrl</kbd>+<kbd>Z</kbd> for the last operation.
- **Keyboard first, mouse always.** Every action has a key (<kbd>F1</kbd> lists them all) and
  a click.

> **Status: pre-alpha.** Browsing, selection, thumbnails, previews and Quick Look work, and so do
> opening files in their apps, a context menu, renaming, new folders and files, copying, moving,
> the trash (restore and permanent delete included), undo, copy and paste and drag and drop
> with other programs, back and forward, hidden files, favourites in the sidebar and removable
> disks. Nordend carries its own icons, title bar and cursor for desktops that do not provide
> them, takes the desktop's colours without Noctalia, and was tried on GNOME, Plasma and niri.
> It remembers how it was left, and `nordend --about` prints what it runs on for bug reports.
> The interface is in English only for now; translations come after the first release.
> Every performance goal of the design is measured and met (see [Benchmarks](#benchmarks)); a
> week of daily use comes next, then the first release. There are no packages yet: see
> [Building](#building). What happens step by step is in the [devlog](docs/devlog.md), what
> comes next in [What's next](#whats-next).

## At a glance

| | |
|---|---|
| Binary | **3.50 MB** (2.92 MB stripped), Release, x86-64, with the wuffs decoders built in |
| Linked libraries | 21 direct: Wayland (with its cursor library), EGL/GLES, xkbcommon, fontconfig, FreeType, HarfBuzz, toml++, libwebp, libjxl, exiv2, zlib, sdbus-c++ (with basu) for udisks2 and the desktop portal, the C/C++ runtime. No GTK, no Qt, no GLib |
| Code | about 34 700 lines of C++20, plus 10 100 lines of tests (272 test cases, run under AddressSanitizer and ThreadSanitizer too) |
| Memory | **25 MB** of its own (private, anonymous) at start, 111 MB resident counting shared libraries (Mesa's GPU driver among them, shared with every OpenGL app); after browsing a folder of 163 raw photos in all three views and Quick Look, **54 MB** of its own, 143 MB resident: decoded photos go back to the system |
| Idle | **0 CPU**: nothing is drawn while nothing moves (0 ticks in 30 s, measured) |
| Threads | the window on one thread, I/O and decoding on a worker pool with priorities and cancellation |

## Benchmarks

Measured, not assumed: every number comes from a tool in this repository, on a Ryzen 7 5700G with
an RX 6650 XT and a SATA SSD. The design's four targets, measured on 2026-11-01:

| Target | Measured |
|--------|---------:|
| Cold start to the first frame on screen: < 150 ms | **128 ms** (warm: **64 ms**) |
| A 10 000-file folder: first frame < 50 ms, the rest in the background | first entries ready in **0.7 ms**, all of them in **17 ms** |
| Quick Look of an ORF raw photo: < 100 ms | **82-96 ms** from Space to the sharp image |
| Scrolling at the monitor's full rate, no dropped frame | worst frame **3.7 ms** of 13.3 (75 Hz) |

More numbers (thumbnails, copies, the trash, every view's scrolling) on the
[site](https://pankaspe.github.io/nordend-fm/#performance); every measurement with its method in
[docs/benchmarks.md](docs/benchmarks.md).

## Features and shortcuts

Everything Nordend does, how to use it and every keyboard shortcut are in the
[guide](https://pankaspe.github.io/nordend-fm/#guide) and on the
[shortcuts page](https://pankaspe.github.io/nordend-fm/#shortcuts); inside Nordend, <kbd>F1</kbd>
shows the shortcuts too. The full list, area by area, is in [docs/features.md](docs/features.md).

## Building

Void Linux package names:

```sh
sudo xbps-install -S cmake ninja pkgconf wayland-devel wayland-protocols \
    libxkbcommon-devel libglvnd-devel MesaLib-devel \
    harfbuzz-devel freetype-devel fontconfig-devel tomlplusplus-devel \
    libwebp-devel libjxl-devel exiv2-devel zlib-devel sdbus-c++-devel
cmake -B build -G Ninja
cmake --build build
./build/nordend            # your home folder; ./build/nordend ~/Pictures for another one
cmake --install build --prefix ~/.local --strip   # nordend and its desktop entry, for your user
```

About a minute on an 8-core CPU. Needs a Wayland compositor with xdg-shell (developed on
[Umbriel](https://github.com/noctalia-dev)); icons, title bar and cursor come with Nordend where
the desktop has none. Making it the default for folders, the command line, the environment
variables and `config.toml` are on the site:
[install](https://pankaspe.github.io/nordend-fm/#install),
[configuration](https://pankaspe.github.io/nordend-fm/#configuration).

<details>
<summary><b>Development builds and tools</b></summary>

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
./build/nordend-snapshot --columns ~ --reveal ~/Pictures columns.ppm
./build/nordend-snapshot --columns /tmp --reveal /tmp/bench-10k --bench 600 columns.ppm

# Start-up benchmark: every step from exec to the first frame on screen, 10 runs
./build/nordend-bench-start -n 10 -- ./build/nordend ~

# Quick Look timings (thumbnail, sharp image) in the log
NORDEND_LOG=debug ./build/nordend ~/Pictures

# Directory loading benchmark
./build/nordend-bench-dir --make 10000 /tmp/bench-10k
./build/nordend-bench-dir --runs 20 /tmp/bench-10k

# The grid and Quick Look offscreen, keys pressed first, thumbnails in a scratch cache
./build/nordend-snapshot --columns ~/Pictures --keys "Ctrl+2,Right,space" \
    --thumbnails /tmp/thumbs quick-look.ppm 1600 1000 1.5

# The details sorted newest first, and the scrolling benchmark over them
./build/nordend-snapshot --columns /tmp/bench-10k --keys "Ctrl+3" --sort "modified,modified" \
    --bench 600 details.ppm 1440 810 2

# Image decoding and thumbnails: sizes, timings, and a PPM to look at
./build/nordend-decode --runs 5 --ppm photo.ppm photo.jpg
./build/nordend-thumb --size large --cache /tmp/thumbs photo.ORF
```

</details>

## What's next

1. **A week of daily use** by the author, with a memory log, and every bug found fixed.
2. **Release 0.1.0**, with a Void Linux package and Nordend's own icon.

After the first release, roughly in this order:

- A command palette (<kbd>Ctrl</kbd>+<kbd>K</kbd>) with fuzzy search
- Tabs, a dual pane, a queue of operations with its undo history, batch renaming
- Photo culling: star ratings and colour labels in darktable's XMP sidecars, side-by-side
  comparison, EXIF and histogram
- Scripting in Luau
- A file chooser for other applications (the desktop portal's FileChooser)
- Opening text files in an external editor (Helix) from Nordend
- A richer Quick Look for text: syntax highlighting, selecting and copying
- Translations, Italian first

Step numbers in code comments and in the devlog ("ROADMAP 6.3") refer to the project's internal
roadmap.

## Documentation

- [The site](https://pankaspe.github.io/nordend-fm/): overview, install, guide, shortcuts,
  configuration, performance, compatibility, architecture, roadmap
- [Features](docs/features.md): everything Nordend does, area by area
- [Devlog](docs/devlog.md): what was done, measured and decided, step by step
- [Benchmarks](docs/benchmarks.md): every measurement, with its method
- [Compatibility](docs/compatibility.md): what to expect on each desktop, and what was tried
- [Upstream bug log](docs/buglog.md): bugs found in Noctalia and Umbriel

## Development

Nordend is copiloted by **Claude Opus** (Anthropic), which assists with code writing, code analysis,
bug hunting, testing and documentation. Design decisions, testing on real hardware and every release
are the author's.

## License

[MIT](LICENSE). Third-party code and data in the repository (the wuffs decoder, Noctalia's
default palette) are listed with their origin and license in [THIRD_PARTY.md](THIRD_PARTY.md).
