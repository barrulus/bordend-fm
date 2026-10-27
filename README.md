# Nordend

**A native file manager for Wayland, made for the [Noctalia](https://github.com/noctalia-dev)
desktop.**

![status](https://img.shields.io/badge/status-pre--alpha-orange)
![phase](https://img.shields.io/badge/phase%205-complete-3DA639)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C?logo=cmake&logoColor=white)
![Wayland](https://img.shields.io/badge/Wayland-native-FFBC00?logo=wayland&logoColor=black)
![OpenGL ES](https://img.shields.io/badge/OpenGL%20ES-3.0-5586A4?logo=opengl&logoColor=white)
![doctest](https://img.shields.io/badge/tests-doctest-4B32C3)
![code license](https://img.shields.io/badge/code-MIT-3DA639)
![platform](https://img.shields.io/badge/platform-Linux-FCC624?logo=linux&logoColor=black)

Nordend is a file manager written from scratch in C++20 that talks to the compositor directly:
Wayland and OpenGL ES, **no GTK, no Qt**, no toolkit at all. Text, icons, scrolling and
animations come from its own renderer, so it does exactly what it is designed to do and nothing
gets in the way. It belongs to the Noctalia family (Noctalia Shell, the Umbriel compositor) and
wears your Noctalia theme, live.

It is made for people who browse a lot of files, photos above all:

- **Two views of the same folder.** Finder-style columns (<kbd>Ctrl</kbd>+<kbd>1</kbd>) to walk
  a tree from the keyboard, a thumbnail grid (<kbd>Ctrl</kbd>+<kbd>2</kbd>) to see a folder of
  photos; switching keeps your place and your selection. A details view
  (<kbd>Ctrl</kbd>+<kbd>3</kbd>: name, size, date and type in sortable columns) comes next.
- **Natural scrolling.** The touchpad moves the list 1:1 under your fingers and lets it glide
  when they lift; wheel and keyboard glide too. Nothing jumps.
- **Fast, and measured.** A 10 000-file folder shows its first entries in 0.8 ms and is fully
  loaded and sorted in 20 ms; a scrolling frame costs 0.2 ms; a 20 MP raw photo opens in Quick
  Look in about 50 ms. Loading, decoding and thumbnails run on a worker pool and never block the
  window, and when nothing moves Nordend uses no CPU at all. Every number is in
  [docs/benchmarks.md](docs/benchmarks.md), with the method to measure it again.
- **Photos first.** Thumbnails and Quick Look (<kbd>Space</kbd>) read the previews cameras store
  in their raw files (OM System, Canon, Nikon, Sony, Fujifilm, Panasonic, Hasselblad), upright
  and in the shape they were shot; thumbnails go to the freedesktop cache other apps share.
- **Keyboard first, mouse always.**

**Status:** pre-alpha. Browsing, selection, thumbnails and Quick Look work; file operations
(open with, rename, copy and move, trash, clipboard) are the next phase. There are no packages
yet: see [Building](#building).

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
- **A preview beside every view.** The pane at the right shows the photo under the cursor
  with what it is: size, pixels, dates, and the shot itself (camera, lens, exposure, focal
  length, when, your darktable rating); text files show their first lines. "More details"
  opens the rest. In the grid and the details Ctrl+I hides it, and its edge can be dragged to
  make it (and the photo) larger.
- **Details when you want to compare.** Ctrl+3 shows the folder as a table of names, sizes,
  dates and types: click a title to sort by it, click again to reverse, drag a column's edge to
  make it wider. Cursor and selection go along between all three views, and so does the info
  pane at the right, which Ctrl+I shows and hides.
- **A grid when you want to see more.** Ctrl+2 lays the folder out as large icons, Ctrl+1
  brings the columns back: where you were and what you selected come along. Ctrl+plus/minus
  (or Ctrl+wheel) make the cells bigger or smaller, from 48 to 256 px; the arrows, Enter and
  Backspace walk the folders as in the columns.
- **Your photos at a glance.** In the grid, photos and camera raw files show their thumbnails,
  upright, in a fraction of a second: Nordend uses the previews your camera already stored in
  the file, and keeps the thumbnails in the shared cache other apps read too. Not only OM
  System: Canon, Nikon, Sony, Fujifilm, Panasonic and Hasselblad raw files too, each in the
  shape it was shot, without black bars.
- **Quick Look.** Press Space and the photo fills the window, raw files included; the arrows
  walk the folder without closing it, and the next photo is already there. Text files open on
  a page you can scroll, darktable sidecars included; everything else shows what it is, how big,
  whose and who may touch it. Space or Escape closes it, and you are where you stopped.
- **Live folders.** Files created, removed or renamed by any program appear at once; a
  thousand-file copy arrives in a few smooth updates, not a thousand.
- **The right icon for every file**, the same line icons as Noctalia, tinted by your theme:
  camera RAW files and darktable sidecars have their own. File types match GTK apps exactly.
- **Your theme, live.** Change the palette in Noctalia and Nordend follows instantly.

## Progress

`nordend [DIR]` opens a folder (default: your home) in Finder-style columns; Ctrl+2 switches to
the thumbnail grid, Ctrl+3 to the details table, Ctrl+1 back, Ctrl+I shows or hides the info
pane of the grid and the details, Space opens Quick Look. `nordend --test-scene` shows everything
the renderer can draw.

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

**Phase 5 — thumbnails, Quick Look and the details view: complete.**

- Image decoding: PNG, JPEG and GIF (wuffs, memory-safe by construction), WebP and JPEG XL
  (libwebp, libjxl), first frame of animations, EXIF orientation; a 20 MP JPEG in about
  150 ms
- Thumbnails in the grid through the freedesktop cache (`~/.cache/thumbnails`, shared with
  other apps), made from the previews camera files carry (OM System JPEG and ORF) or from the
  whole image; a screenful in 0.15-0.4 s the first time, about 20 ms after
- Quick Look: Space shows the entry under the cursor over the window, arrows walk the folder
  underneath; images and raw files fitted to the screen (an ORF in about 55 ms), the thumbnail
  first, neighbours prepared ahead; an information card for the rest
- Raw files of other cameras (Canon, Nikon, Sony, Fujifilm, Panasonic, Hasselblad):
  uncompressed TIFF previews (3FR, Nikon TIFF), Panasonic GX8 previews cropped to the aspect
  ratio shot, no black bars from 4:3 EXIF thumbnails; Quick Look of a raw file with a small
  preview in a few ms
- Quick Look of text files (code, configuration, logs, darktable sidecars) on a page with line
  numbers, in a monospace font (`font.monospace`), scrolling with the wheel or the touchpad; an
  information card for everything else, with permissions, owner, link target and a folder's
  number of items
- Details view (Ctrl+3): name, size, date and type in columns; a click on a title sorts by it,
  a second one reverses; the column edges can be dragged; a 10 000-file folder sorts in 1-3 ms
  and scrolls at 0.3-0.5 ms per frame
- Info pane in every view, at the right of the grid and the details too; Ctrl+I shows and
  hides it in all three
- The info pane as a preview: the photo's thumbnail (or a text file's first
  lines), then size, pixels and dates, the shot (camera, lens, exposure, focal length, date,
  darktable rating), and "More details" for permissions, owner and the rest; Ctrl+I in the grid
  and the details only

**Next:** phase 6, file operations: open with the default app, new folder and rename, copy and
move with progress, trash, clipboard.
The full plan is in the [roadmap](docs/ROADMAP.md).

## Benchmarks

Measured, not assumed. Opening a folder of **10 000 files** on a SATA SSD:

| Cache | First entries ready | Whole folder loaded and sorted | Target for the first frame |
|-------|--------------------:|-------------------------------:|---------------------------:|
| warm (median of 20) | **0.8 ms** | **19.5 ms** | < 50 ms |
| cold (after `drop_caches`) | **1.7 ms** | **81 ms** | < 50 ms |

Photos (OM System OM-1: 20 MP JPEGs and ORF raw files), Ryzen 7 5700G, warm cache:

| What | Time |
|------|-----:|
| A grid screenful of thumbnails, made the first time / read from the cache | **0.15-0.4 s** / **12-22 ms** |
| One thumbnail from an OM-1 JPEG / ORF (embedded preview, scaled, PNG written) | **12 ms** / **36 ms** |
| Quick Look of an ORF, fitted to a 2880x1620 screen | **51-55 ms** |
| Scrolling the grid with thumbnails, per frame at 2880x1620 | **0.62 ms** |

Method, machine, cold-cache numbers and more folders: [docs/benchmarks.md](docs/benchmarks.md).

## Building

Void Linux package names:

```sh
sudo xbps-install -S cmake ninja pkgconf wayland-devel wayland-protocols \
    libxkbcommon-devel libglvnd-devel MesaLib-devel \
    harfbuzz-devel freetype-devel fontconfig-devel tomlplusplus-devel \
    libwebp-devel libjxl-devel exiv2-devel zlib-devel
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

[MIT](LICENSE). Third-party code and data in the repository (the wuffs decoder, Noctalia's
default palette) are listed with their origin and license in [THIRD_PARTY.md](THIRD_PARTY.md).
