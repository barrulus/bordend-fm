# Nordend

**A native file manager for Wayland, made for the [Noctalia](https://github.com/noctalia-dev)
desktop.**

![status](https://img.shields.io/badge/status-pre--alpha-orange)
![phase](https://img.shields.io/badge/next-MVP-3DA639)
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

- **Three views of the same folder.** Finder-style columns (<kbd>Ctrl</kbd>+<kbd>1</kbd>) to
  walk a tree from the keyboard, a thumbnail grid (<kbd>Ctrl</kbd>+<kbd>2</kbd>) to see a folder
  of photos, a details table (<kbd>Ctrl</kbd>+<kbd>3</kbd>) to compare sizes and dates;
  switching keeps your place and your selection.
- **Natural scrolling.** The touchpad moves the list 1:1 under your fingers and lets it glide
  when they lift; wheel and keyboard glide too. Nothing jumps.
- **Small and fast, and measured.** A 2.6 MB binary; a 10 000-file folder shows its first entries
  in 0.8 ms; a scrolling frame costs a fraction of a millisecond; a 20 MP raw photo opens in
  Quick Look in about 50 ms; and when nothing moves Nordend uses no CPU at all.
  See [Benchmarks](#benchmarks).
- **Photos first.** Thumbnails, the preview pane and Quick Look (<kbd>Space</kbd>) read the
  previews cameras store in their raw files (OM System, Canon, Nikon, Sony, Fujifilm,
  Panasonic, Hasselblad), upright and in the shape they were shot, with the shot's EXIF and your
  darktable rating.
- **Keyboard first, mouse always.** Every action has a key (<kbd>F1</kbd> lists them all) and a
  click.

**Status:** pre-alpha. Browsing, selection, thumbnails, previews and Quick Look work, and so do
opening files in their apps, a context menu, renaming, new folders and files, copying, moving,
the trash, restoring from it and deleting for good, and copy and paste with other programs; the
MVP's last steps (other compositors, settings) come next. There are no packages yet: see [Building](#building). What happens step by step is in the
[devlog](docs/devlog.md), what comes next in the [roadmap](docs/ROADMAP.md).

## At a glance

| | |
|---|---|
| Binary | **3.05 MB** (2.56 MB stripped), Release, x86-64, with the wuffs decoders built in |
| Linked libraries | 18 direct: Wayland, EGL/GLES, xkbcommon, fontconfig, FreeType, HarfBuzz, toml++, libwebp, libjxl, exiv2, zlib, the C/C++ runtime. No GTK, no Qt, no GLib |
| Code | about 28 500 lines of C++20, plus 7 500 lines of tests (208 test cases, run under AddressSanitizer and ThreadSanitizer too) |
| Memory | **25 MB** of its own (private, anonymous) at start, 111 MB resident counting shared libraries (Mesa's GPU driver among them, shared with every OpenGL app); after browsing a folder of 163 raw photos in all three views and Quick Look, **54 MB** of its own, 143 MB resident: decoded photos go back to the system |
| Idle | **0 CPU**: nothing is drawn while nothing moves (0 ticks in 30 s, measured) |
| Threads | the window on one thread, I/O and decoding on a worker pool with priorities and cancellation |

## Benchmarks

Measured, not assumed: every number comes from a tool in this repository, on a Ryzen 7 5700G with
an RX 6650 XT and a SATA SSD. Method and more cases in [docs/benchmarks.md](docs/benchmarks.md).

**Opening a folder of 10 000 files**

| Cache | First entries ready | Whole folder loaded and sorted | Target for the first frame |
|-------|--------------------:|-------------------------------:|---------------------------:|
| warm (median of 20) | **0.8 ms** | **19.5 ms** | < 50 ms |
| cold (after `drop_caches`) | **1.7 ms** | **81 ms** | < 50 ms |

**Scrolling a 10 000-file folder**, per frame at 2880x1620, GPU work included (a 60 Hz frame is
16.7 ms):

| View | Median | p95 |
|------|-------:|----:|
| Columns | **0.26 ms** | 0.33-0.47 ms |
| Grid (icons) | **0.30 ms** | 0.51 ms |
| Grid (90 photo thumbnails) | **0.62 ms** | 0.76 ms |
| Details | **0.36-0.50 ms** | 0.58 ms |

Only what is on screen is drawn, so the folder's size does not matter; sidebar, breadcrumb and
status bar cost nothing measurable.

**Photos** (OM System OM-1: 20 MP JPEGs and ORF raw files), warm cache:

| What | Time |
|------|-----:|
| A grid screenful of thumbnails, made the first time / read from the cache | **0.15-0.4 s** / **12-22 ms** |
| One thumbnail from an OM-1 JPEG / ORF (embedded preview, scaled, PNG written) | **12 ms** / **36 ms** |
| Quick Look of an ORF, fitted to a 2880x1620 screen | **51-55 ms** |
| Quick Look of a Sony ARW (small embedded preview) | **17 ms** |
| A 20 MP JPEG decoded whole (wuffs) | **149 ms** |
| Sorting 10 000 entries by size, date or type | **1-3 ms** |

**Files**, warm cache:

| What | Nordend | For comparison |
|------|--------:|---------------:|
| Copying a folder of 342 photos, 3.1 GB, into tmpfs | **1.39 s** | `cp -a`: 1.38-1.40 s |
| Moving 1 001 items to the trash | **25 ms** | `gio trash`: 70 ms |
| Default app and Open With list, all 1 232 file types | **0 differences** | from `gio mime` |

<details>
<summary><b>Features</b>: what works today (the full list: <a href="docs/features.md">docs/features.md</a>)</summary>

- **Natural touch.** Scrolling that feels physical: on a touchpad the list follows your fingers
  1:1 and, when they lift, keeps gliding and slows down smoothly, stopping softly at the ends;
  a mouse wheel glides instead of jumping, and quick turns add up. The keyboard moves the same
  way: arrows and Page Up/Down glide, Home/End jump.
- **Columns, as in Finder.** Every folder you pick opens in the next column; arrows walk the
  tree, Left from the first column climbs to the parent. Columns resize by dragging their edge
  and slide sideways by themselves to keep where you are in view.
- **A grid when you want to see more.** Ctrl+2 lays the folder out as large icons and
  thumbnails; Ctrl+plus/minus (or Ctrl+wheel) make the cells bigger or smaller, from 48 to
  256 px.
- **Details when you want to compare.** Ctrl+3 shows the folder as a table of names, sizes,
  dates and types: click a title to sort by it, click again to reverse, drag a column's edge to
  make it wider. Cursor and selection go along between all three views.
- **Selection as you expect it.** Click, Ctrl+click, Shift+click, or the keyboard alone:
  Shift+arrows for a range, Ctrl+arrows and Ctrl+Space to pick here and there, Ctrl+A for all.
- **A preview beside every view.** The pane at the right shows the photo under the cursor
  with what it is: size, pixels, dates, and the shot itself (camera, lens, exposure, focal
  length, when, your darktable rating); text files show their first lines. "More details"
  opens permissions, owner and the rest. In the grid and the details Ctrl+I hides it, and its
  edge can be dragged to make it (and the photo) larger.
- **Your places, one click away.** A sidebar lists your home and its folders (Downloads,
  Pictures...), the file system, your GTK bookmarks and the mounted disks; a disk appears as
  soon as it is mounted and goes when it is unmounted. Ctrl+B hides it, F6 takes the keyboard
  there and back.
- **The path, one step at a time.** Above every view the folder's path is a row of steps,
  starting from your home; click one to go back up there, with the cursor on the folder you
  came from.
- **Everything at hand.** The bar at the bottom tells how many items there are, what is
  selected and how much space is left on the disk, and switches between the three views with a
  click; "?" (or F1) shows every keyboard shortcut.
- **Your photos at a glance.** Photos and camera raw files show their thumbnails, upright, in a
  fraction of a second: Nordend uses the previews your camera already stored in the file, and
  keeps the thumbnails in the freedesktop cache other apps read too. Not only OM System: Canon,
  Nikon, Sony, Fujifilm, Panasonic and Hasselblad raw files too, each in the shape it was shot,
  without black bars.
- **Quick Look.** Press Space and the photo fills the window, raw files included; the arrows
  walk the folder without closing it, and the next photo is already there. Text files open on
  a page you can scroll, darktable sidecars included; everything else shows what it is, how
  big, whose and who may touch it.
- **Opens in the right app.** Enter or a double click opens a file in its default application,
  the same one GTK apps choose (checked on all 1 232 file types); five photos go to one darktable,
  not five; terminal apps such as Helix get a terminal; and the new window comes up in front.
- **Rename where the name is.** F2 turns the name into a field, the extension left out of the
  selection; Ctrl+Shift+N makes a new folder ready to be named. Nothing is ever overwritten, and
  renaming a photo takes its darktable sidecars along, duplicates included, so no edit is lost.
- **Copy and move, safely.** Ctrl+C or Ctrl+X, then Ctrl+V: a move on one disk is instant, a
  copy runs as fast as `cp` with its progress, speed and time left in the status bar. Nothing
  half-copied ever carries a real name, an original leaves another disk only once its copy is
  safely written, dates and permissions stay, and darktable sidecars come along. A taken name
  asks: replace, skip or keep both, for one or for the rest.
- **A real trash.** Delete moves files to the freedesktop trash, on their own disk (no copies),
  where any other trash program finds and restores them; darktable sidecars go with their photo.
  The Trash in the sidebar shows every disk's trash as one folder, by original name, and Restore
  puts things back where they were, folder and sidecars included. Shift+Delete and Empty Trash
  delete for good, only after a card that says how many items and how many MB go.
- **Copy and paste with other programs.** Files copied or cut in Nordend paste in a file manager,
  in Firefox or as paths in a terminal; files copied or cut elsewhere paste in Nordend, copied or
  moved by the same careful engine. A program that does not answer never freezes the window.
- **A context menu.** A right click, a two-finger tap or the Menu key: Open With lists the same
  applications GTK apps would offer (0 differences from `gio` on 1 232 types), New File makes an
  empty file or one from your templates, and every action shows its shortcut.
- **Live folders.** Files created, removed or renamed by any program appear at once; a
  thousand-file copy arrives in a few smooth updates, not a thousand.
- **The right icon for every file**, the same line icons as Noctalia, tinted by your theme:
  camera RAW files and darktable sidecars have their own. File types match GTK apps exactly.
- **Your theme, live.** Change the palette in Noctalia and Nordend follows instantly.
- **Sharp at any scale.** Fractional scaling (1, 1.25, 1.5, 2...), text shaped by HarfBuzz
  with font fallback for accents, emoji and CJK.

</details>

<details>
<summary><b>Keyboard shortcuts</b> (F1 in Nordend shows the same list)</summary>

| Keys | Action |
|------|--------|
| <kbd>Ctrl</kbd>+<kbd>1</kbd> / <kbd>2</kbd> / <kbd>3</kbd> | Columns / grid / details |
| <kbd>Space</kbd> | Quick Look (Space or Escape closes it) |
| <kbd>Ctrl</kbd>+<kbd>I</kbd> | Info pane (grid, details) |
| <kbd>Ctrl</kbd>+<kbd>B</kbd> | Sidebar |
| <kbd>F6</kbd> | Keyboard into the sidebar and out |
| <kbd>F1</kbd>, <kbd>?</kbd> | Every shortcut |
| <kbd>Ctrl</kbd>+<kbd>Q</kbd> | Quit |
| <kbd>↑</kbd> <kbd>↓</kbd> (<kbd>←</kbd> <kbd>→</kbd> in the grid) | Previous, next |
| <kbd>PgUp</kbd> <kbd>PgDn</kbd>, <kbd>Home</kbd> <kbd>End</kbd> | A screenful, first, last |
| <kbd>Enter</kbd>, double click | Open: a folder here, files in their default app |
| <kbd>→</kbd> (columns, details), <kbd>Alt</kbd>+<kbd>↓</kbd> (grid, details) | Open the folder |
| <kbd>Backspace</kbd>, <kbd>←</kbd> (columns, details), <kbd>Alt</kbd>+<kbd>↑</kbd> | Parent folder |
| <kbd>Shift</kbd>+moves | Select a range |
| <kbd>Ctrl</kbd>+moves | Move without selecting |
| <kbd>Ctrl</kbd>+<kbd>Space</kbd> | Select or unselect |
| <kbd>Ctrl</kbd>+<kbd>A</kbd> | Select all |
| <kbd>Ctrl</kbd>+<kbd>C</kbd> / <kbd>X</kbd> / <kbd>V</kbd> | Copy / cut (<kbd>Escape</kbd> keeps them) / paste, with other programs too |
| <kbd>Delete</kbd> | Move to the trash (in the Trash: delete permanently, after asking) |
| <kbd>Shift</kbd>+<kbd>Delete</kbd> | Delete permanently, after asking |
| <kbd>Menu</kbd>, <kbd>Shift</kbd>+<kbd>F10</kbd>, right click | Context menu |
| <kbd>F2</kbd> | Rename (<kbd>Enter</kbd> keeps the name, <kbd>Escape</kbd> gives up, <kbd>Ctrl</kbd>+<kbd>V</kbd> pastes text, <kbd>Ctrl</kbd>+<kbd>Z</kbd> undoes) |
| <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>N</kbd> | New folder |
| <kbd>Escape</kbd> | Back to one entry |
| <kbd>Ctrl</kbd>+<kbd>+</kbd> / <kbd>−</kbd> / <kbd>0</kbd> | Grid: larger, smaller, usual size |

</details>

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
[Umbriel](https://github.com/noctalia-dev)). `NORDEND_LOG=debug` (or `trace`) shows more detail;
`nordend --check-config` checks `~/.config/nordend/config.toml`
([example](docs/config.example.toml)).

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

## Documentation

- [Features](docs/features.md): everything Nordend does, area by area
- [Design](docs/DESIGN.md) (Italian): what Nordend is and why
- [Roadmap](docs/ROADMAP.md) (Italian): the steps, each with when it counts as done
- [Devlog](docs/devlog.md): what was done, measured and decided, step by step
- [Benchmarks](docs/benchmarks.md): every measurement, with its method
- [Upstream bug log](docs/buglog.md): bugs found in Noctalia and Umbriel

## Development

Nordend is copiloted by **Claude Opus** (Anthropic), which assists with code writing, code analysis,
bug hunting, testing and documentation. Design decisions, testing on real hardware and every release
are the author's.

## License

[MIT](LICENSE). Third-party code and data in the repository (the wuffs decoder, Noctalia's
default palette) are listed with their origin and license in [THIRD_PARTY.md](THIRD_PARTY.md).
