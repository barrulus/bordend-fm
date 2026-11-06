<p align="center"><img src="data/nordend.svg" width="96" height="96" alt=""></p>

<h1 align="center">Nordend</h1>

<p align="center">
  <b>A fast, native file manager for Wayland, made for photographers.<br>
  At home on any Wayland desktop, and right at home on <a href="https://github.com/noctalia-dev">Noctalia</a>.</b><br>
  No GTK, no Qt: C++20 straight on Wayland and OpenGL ES.
</p>

<p align="center">
  <a href="https://github.com/pankaspe/nordend-fm/releases/tag/v0.1.0"><img src="https://img.shields.io/badge/release-v0.1.0-2a6a9e" alt="release v0.1.0"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white" alt="C++20">
  <img src="https://img.shields.io/badge/Wayland-native-FFBC00?logo=wayland&logoColor=black" alt="Wayland native">
  <img src="https://img.shields.io/badge/tests-289%20%C2%B7%20ASan%20%C2%B7%20TSan-4B32C3" alt="289 tests, ASan, TSan">
  <img src="https://img.shields.io/badge/license-MIT-3DA639" alt="MIT license">
</p>

> [!NOTE]
> **Young software, used every day by its author.** The first release, 0.1.0, came out on
> 2026-11-06; since then `main` has gained **tabs** and **Sort By** from the context menu.
> On Void Linux, [install it as a package](#on-void-linux); anywhere else,
> [build it from source](#from-source) in about a minute.

<p align="center">
  <a href="https://pankaspe.github.io/nordend-fm/"><img src="https://img.shields.io/badge/Read_the_guide-pankaspe.github.io%2Fnordend--fm-6DBDF0?style=for-the-badge&labelColor=0E1519" alt="Read the guide on pankaspe.github.io/nordend-fm"></a>
</p>

<p align="center">
  <img src="docs/images/readme-grid-light.webp" alt="Nordend on the desktop with three tabs: a grid of raw photos of a snowy summit, one selected, its preview and EXIF in the info pane" width="100%">
  <br>
  <sub>The grid on a folder of OM System raw files, three tabs open, in Noctalia's Nord light theme on Umbriel.</sub>
</p>

<p align="center">
  <img src="docs/images/readme-columns-dark.webp" alt="Nordend in a dark theme: the column view five folders deep in a source tree, a text file's first lines in the info pane" width="100%">
  <br>
  <sub>The columns deep in a source tree, a text file's first lines in the info pane, in Nord dark.</sub>
</p>

## Why Nordend

Nordend talks to the compositor directly, with **no toolkit at all**: text, icons, scrolling and
animations come from its own renderer. It takes your desktop's light or dark style and accent
colour; on Noctalia (Noctalia Shell, the Umbriel compositor) it wears your Noctalia theme, live.

- **Photos first.** Thumbnails, the preview pane and Quick Look read the previews cameras store
  in their raw files, with the shot's EXIF and your darktable rating; darktable sidecars travel
  with their photo through every rename, copy, move and trip to the trash.
- **Three views of the same folder.** Columns, a thumbnail grid and a details table
  (<kbd>Ctrl</kbd>+<kbd>1</kbd> <kbd>2</kbd> <kbd>3</kbd>); switching keeps your place and your
  selection. Sort by name, size, date or type from the context menu, in every view.
- **Tabs.** <kbd>Ctrl</kbd>+<kbd>T</kbd>, or a middle click on a folder to open it behind the
  one you are in; each tab keeps its folder, view and history, and they all come back at the next
  start.
- **Small and fast, and measured.** A 3 MB binary, a 10 000-file folder in 0.7 ms, a raw photo
  in Quick Look in under 100 ms, and no CPU at all while nothing moves.
- **Careful with your files.** Nothing overwritten without asking, nothing half-copied under a
  real name, a real freedesktop trash, and <kbd>Ctrl</kbd>+<kbd>Z</kbd>.
- **Keyboard first, mouse always.** Every action has a key (<kbd>F1</kbd> lists them) and a click.

Everything it does, and every shortcut, is in the [guide](https://pankaspe.github.io/nordend-fm/#guide).

## At a glance

| | |
|---|---|
| Binary | **3.60 MB** (3.01 MB stripped), x86-64, decoders built in |
| Libraries | 21 direct; no GTK, no Qt, no GLib |
| Code | about 36 300 lines of C++20, 11 000 of tests (289 test cases, under ASan and TSan too) |
| Memory | **25 MB** of its own at start, **54 MB** after browsing 163 raw photos in every view |
| Idle | **0 CPU**: nothing is drawn while nothing moves |

## Benchmarks

The design's four targets, measured on 2026-11-01 (Ryzen 7 5700G, RX 6650 XT, SATA SSD):

| Target | Measured |
|--------|---------:|
| Cold start to the first frame on screen: < 150 ms | **128 ms** (warm: **64 ms**) |
| A 10 000-file folder: first frame < 50 ms | first entries in **0.7 ms**, all in **17 ms** |
| Quick Look of an ORF raw photo: < 100 ms | **82-96 ms** |
| Scrolling with no dropped frame | worst frame **3.7 ms** of 13.3 (75 Hz) |

More on the [site](https://pankaspe.github.io/nordend-fm/#performance), every method in
[docs/benchmarks.md](docs/benchmarks.md).

## Install

Nordend needs Linux with a Wayland compositor that offers xdg-shell (developed on Umbriel, also
tried on GNOME, Plasma and niri) and a GPU driver with OpenGL ES 3.0 (Mesa).

### On Void Linux

Nordend is developed on Void, and comes with an xbps-src template in
[`packaging/void`](packaging/void). It builds the latest release into a package that puts
`nordend`, its desktop entry and its icon under `/usr`:

```sh
# 1. void-packages, once (skip if you already have a checkout)
git clone --depth 1 https://github.com/void-linux/void-packages
cd void-packages
./xbps-src binary-bootstrap

# 2. Nordend's template, then the package
git clone --depth 1 https://github.com/pankaspe/nordend-fm /tmp/nordend-fm
cp -r /tmp/nordend-fm/packaging/void/nordend srcpkgs/
./xbps-src pkg nordend
sudo xbps-install -R hostdir/binpkgs nordend
```

The package is the latest release, **0.1.0**. Tabs and Sort By are on `main` and come with the
next release; to have them now, build from source.

### From source

On Void Linux the packages are these; elsewhere, the development packages of the same libraries
(their names differ and have not been tried yet):

```sh
sudo xbps-install -S cmake ninja pkgconf wayland-devel wayland-protocols \
    libxkbcommon-devel libglvnd-devel MesaLib-devel \
    harfbuzz-devel freetype-devel fontconfig-devel tomlplusplus-devel \
    libwebp-devel libjxl-devel exiv2-devel zlib-devel sdbus-c++-devel
git clone https://github.com/pankaspe/nordend-fm
cd nordend-fm
cmake -B build -G Ninja
cmake --build build
./build/nordend            # your home folder, or ./build/nordend ~/Pictures
cmake --install build --prefix ~/.local --strip   # nordend, its desktop entry and icon
```

### Then

Make it the default for folders (a browser's "Show in folder", `xdg-open`) and give it a key:

```sh
xdg-mime default nordend.desktop inode/directory
```

```toml
# Umbriel's config.toml; other compositors have their own way
"Mod+E" = { action = "spawn nordend", repeat = false }
```

Configuration, the command line and the development tools are on the
[site](https://pankaspe.github.io/nordend-fm/#install).

## Versions

Nordend stays at 0.x while it is young. A new **minor** version (0.2.0, 0.3.0…) brings features
you can see; a **patch** (0.2.1) only fixes things. 1.0.0 will come once Nordend has been its
author's everyday file manager for a while and its files (`config.toml`, `state.toml`) have
settled.

| Version | What it brought |
|---|---|
| on `main`, next | Sort By in the context menu; tabs (a folder, a view and a history each, remembered); zoom in the columns and the details, each tab its own, remembered |
| [0.1.0](https://github.com/pankaspe/nordend-fm/releases/tag/v0.1.0), 2026-11-06 | The first release: three views, raw previews and Quick Look, file operations with undo, trash, drag and drop, disks, the desktop's look |

## What's next

0.1.0 came out after a week of daily use by the author; since then, Sort By in the context menu,
tabs, and zoom in the lists. Coming, roughly in this order: a command palette (<kbd>Ctrl</kbd>+<kbd>K</kbd>); a dual
pane, a queue of operations and batch renaming; photo culling (ratings and colour labels in
darktable's sidecars, side-by-side comparison, histogram); scripting in Luau; a file chooser for
other applications; text files in an external editor; translations, Italian first. The full list
is on the [roadmap](https://pankaspe.github.io/nordend-fm/#roadmap).

## Documentation

[The site](https://pankaspe.github.io/nordend-fm/) ·
[Features](docs/features.md) ·
[Devlog](docs/devlog.md) ·
[Benchmarks](docs/benchmarks.md) ·
[Compatibility](docs/compatibility.md) ·
[Upstream bug log](docs/buglog.md)

## Development

Nordend is copiloted by **Claude Opus** (Anthropic), which assists with code writing, code analysis,
bug hunting, testing and documentation. Design decisions, testing on real hardware and every release
are the author's. Step numbers in code comments ("ROADMAP 6.3") refer to the internal roadmap.

## License

[MIT](LICENSE). Third-party code and data (the wuffs decoder, the Tabler icons, Noctalia's default
palette) are listed in [THIRD_PARTY.md](THIRD_PARTY.md).
