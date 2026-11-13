# Release history and source versions

[Documentation](README.md) · [Roadmap](roadmap.md) · [Development log](devlog.md)

rummage is a fork of Nordend. The project version remains 0.5.0 while fork work
continues; the source commit identifies behavior more precisely than that number.
The development download tag is a moving snapshot, not a new upstream release.
See [installation](install.md#development-downloads) for package/source identity.

## Current rummage additions

The fork includes system icon themes and SVG rendering, HEIC/HEIF still previews,
visibility of unmounted internal partitions, optional isolated video
previews, configurable shortcuts/actions/menus, destination pickers and completion
notifications, archive password handling, expandable details, two persistent panes,
filename filtering, properties/folder icons, independent sidebar zoom, status text
settings, the file-picker portal and NixOS integration. Packages and public names
use rummage. See [current features](features.md).

## Upstream release history

The following entries preserve Nordend's recorded releases and measurements.
They are historical descriptions, not benchmark results for the current fork.

## [0.5.0](https://github.com/pankaspe/nordend-fm/releases/tag/v0.5.0), 2026-11-09

Text previews: text files in Quick Look and the info pane, read in the caged helper
(UTF-8, UTF-16, Latin-1) with syntax colours from your theme for C, C++, Rust, Python,
JavaScript, TypeScript, Lua, Luau, shell (heredocs included), fish, CMake, Makefiles,
JSON, TOML, YAML, INI files and desktop entries, `.gitignore` and `.gitattributes`,
Markdown (code blocks in their own language) and diffs; select with the pointer, Ctrl+A,
Ctrl+C; long lines scroll sideways; Home, End, Page Up and Page Down; a caption with the
language, encoding and line ends; minified files shown plain; logs opened at their newest
lines. The helper became the first engine, described by a manifest

## [0.4.0](https://github.com/pankaspe/nordend-fm/releases/tag/v0.4.0), 2026-11-08

Image previews read by Nordend's own code: DNG developed when its previews are too small
(Leica M240 and M8, DJI drones, Adobe-converted files) with the file's colour, tone curve,
vignetting and lens corrections, read identically to LibRaw and 2-4 times faster; TIFF
with its own reader (BigTIFF, LZW, Deflate, PackBits, JPEG, 8/16-bit and float, CMYK,
tiles, a scanner's scan in a SubIFD); raw files' previews and the info pane's EXIF without
exiv2 (the same results, 3 to 100 times quicker); pictures made in small `nordend-preview`
processes, each in a cage of Nordend's own (seccomp and landlock), so a file that crashes
or hangs one costs only its preview; one path for every picture, told by the file's bytes;
a plain sentence when a file cannot be shown. Canon CRW and Minolta MRW get that sentence;
darktable sidecars' ratings are no longer read (Nordend's own ratings come next)

## [0.3.1](https://github.com/pankaspe/nordend-fm/releases/tag/v0.3.1), 2026-11-07

Fixes: DNG previews stored as a JPEG inside a TIFF (Leica M240, Samsung GX20 at full
size), broken previews passed over for the next one (Samsung SRW), PGM and PPM images, a
plain message for raw formats with no readable preview (Sigma X3F, old Panasonic RAW)

## [0.3.0](https://github.com/pankaspe/nordend-fm/releases/tag/v0.3.0), 2026-11-06

The command palette (<kbd>Ctrl</kbd>+<kbd>K</kbd>, fuzzy matching with fzy); finding files
under a folder (<kbd>Ctrl</kbd>+<kbd>F</kbd>); Nordend's icon follows the desktop's accent
colour (`icon.follow_accent`)

## [0.2.0](https://github.com/pankaspe/nordend-fm/releases/tag/v0.2.0), 2026-11-06

Sort By in the context menu; tabs (a folder, a view, a history and a zoom each,
remembered); zoom in the columns and the details

## [0.1.0](https://github.com/pankaspe/nordend-fm/releases/tag/v0.1.0), 2026-11-06

The first release: three views, raw previews and Quick Look, file operations with undo,
trash, drag and drop, disks, the desktop's look
