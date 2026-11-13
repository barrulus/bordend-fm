# Feature overview

[Documentation](README.md) · [Using rummage](usage.md) · [Roadmap](roadmap.md)

This is a map of the current source tree's capabilities. Detailed behavior belongs
in the linked guides. The fork develops beyond upstream Nordend 0.5.0; see [release history](releases.md).

## Browsing and navigation

- Columns, thumbnail grid and a details table; changing views retains location and selection.
- Expandable folders in details, loaded on demand; selection stays within one parent.
- Two persistent panes with their own tabs, locations, views, sorting, hidden settings and filters.
- Live filename filtering alongside recursive search; independent sidebar zoom.
- Sorting by name, size, modified date or type; hidden-file toggle; live directory updates.
- Keyboard and mouse selection, rubber bands, smooth scrolling and adjustable zoom.
- Breadcrumb, history, places, GTK bookmarks and removable-device integration.
- Fuzzy command palette and recursive filename/path search with cancellation.

See [Using rummage](usage.md) and the [shortcut reference](shortcuts.md).
Search is not content search; remote browsing, encrypted-volume unlocking and
bookmark reordering are not implemented. Name sorting is natural and ASCII
case-insensitive, not locale collation.

## Files and external applications

- Copy/move with progress, cancellation and conflict choices; rename and create from templates.
- Freedesktop Trash across filesystems, restore, confirmed permanent deletion and undo/redo.
- Clipboard exchange and drag/drop with desktop applications.
- Mounting internal partitions and removable disks through udisks2, with authorization when required.
- Properties and a folder-icon picker, sharing `.directory` metadata with Dolphin.
- Default applications and Open With from desktop MIME associations; terminal launching.
- darktable sidecars follow supported photo operations.
- Copy Location, automatic archive backends and configurable external actions.
- Destination pickers, completion notifications and nested/context-specific menus.

See [file operations](file-operations.md) and [actions](actions.md). Permanent
removal cannot be undone; external commands/extraction are outside file-operation
undo. File handling is local; general queued jobs, macros and batch rename are
not implemented.

## Previews

- Isolated image and text decoding through `rummage-preview`.
- PNG, JPEG, GIF, WebP, JPEG XL, HEIC/HEIF (HEVC), TIFF/BigTIFF and binary PGM/PPM previews.
- Camera raw embedded previews and development of supported DNG data when needed.
- Photo metadata, orientation/crop handling and shared freedesktop thumbnail caching.
- Quick Look for pictures and text, with file-to-file navigation.
- Text encoding detection, syntax colours, selection/copy and log tails.
- Optional isolated FFmpeg video thumbnails and Quick Look stills.

See [previews and formats](previews.md) for decoder limits. Video playback, rendered
Markdown, PDF previews, SVG document previews and darktable rating import are not
current capabilities. SVG **theme icons** have their own in-process renderer.

## Desktop integration and customization

- Folder-jump shortcuts, configurable startup locations, and per-folder view/sort/zoom
  preferences for up to 512 recent folders.

- Wayland input, clipboard, drag/drop, activation and fractional scaling where protocols exist.
- Server-side decorations or rummage's title bar; cursor-shape or Xcursor fallback.
- Desktop light/dark/accent settings, Noctalia palette template and live palette changes.
- Desktop SVG/PNG icon themes, custom place icons and an accent application icon.
- Fonts, pointer settings, background opacity, keybindings, actions and menu layouts in TOML.
- Remembered two-pane/multi-tab sessions and window/view state, without saving navigation history.

See [configuration](configuration.md), [appearance](appearance.md) and
[compatibility](compatibility.md). Fonts need a restart; the other settings have
specific reload rules. No graphical settings editor is provided.

## File selection for other applications

- Standalone open-file, multi-file, directory and save-file commands with NUL-separated output.
- Optional XDG FileChooser backend for OpenFile, SaveFile and SaveFiles.
- Glob/MIME filters, application choices, overwrite confirmation and cancellation.
- Shared views/previews while keeping the browser's saved session separate.
- D-Bus activation, Wayland parenting where available and NixOS module configuration.

See [file-picker setup](file-picker.md) and [command-line reference](command-line.md).
Only applications using the portal use this backend. Unsupported parent handles
open independent windows; filename-based MIME filtering has limits.

## Implementation and evidence

rummage uses C++20, Wayland and OpenGL ES without GTK, Qt or GLib. The
[architecture](architecture.md) explains modules and execution, and the
[security model](security.md) distinguishes isolated readers from other file
handling and external programs.

[Benchmarks](benchmarks.md) record performance at specific revisions and machines.
Binary sizes, dependency counts and test counts change with the build; they are
not repeated here as current guarantees. See [development](development.md) to
reproduce checks and [the roadmap](roadmap.md) for planned work.
