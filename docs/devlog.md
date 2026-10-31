# Devlog

Work log, newest first. Each entry: what was done, decisions, measurements, open points.

---

## 2026-10-31 — The desktop's colours without Noctalia (ROADMAP 7.4, last item)

**Done**
- `theme::desktop_palette(Desktop)`: a palette from light/dark and an accent colour, in OKLCH.
  Neutral surfaces (a trace of the accent's hue, chroma 0.005) around GNOME's grey in dark mode
  and near white in light mode; the accent for `primary` (selection, icons, links, the context
  menu's row), its lightness kept in a band and pushed further until it reads on
  `surface_container_high` (4.5:1); `on_*` colours chosen light or dark by contrast; error roles
  a fixed red. No accent: GNOME's blue `#3584e4`.
- `theme::Theme::set_desktop()`: `colors.toml` wins; without it the desktop's palette, otherwise
  the default one. Removing the file falls back to the latest desktop look.
- `platform::DesktopPortal`: the freedesktop portal's Settings on the session bus, namespace
  `org.freedesktop.appearance` (`color-scheme`, `accent-color`), read before the window opens
  (synchronous, 300 ms at most; later, the answer comes through the loop) and followed through
  `SettingChanged`. `appearance_from()` checks the values: color-scheme 0 (no preference) reads as
  light, as GTK and Qt apps read it; an accent outside 0..1 is the portal's "unset".
- `platform::BusLoop`: the D-Bus connection served by the event loop, moved out of `Udisks` so
  the portal uses it too.
- `ui::Color` gains `operator==`. Snapshot: `--desktop dark|light[,#rrggbb]`.
- Tests (256): contrast of every text/fill pair for 15 accents (GNOME's nine, Plasma's blue,
  white, black, grey, pure yellow, pure blue) in both modes; surfaces step the right way; the
  accent's hue; the portal's values; Theme's rules (file wins, back to the desktop, nothing to
  redraw on the same look); a read-only test against the real portal (here: dark, no accent).

**Measured**
- Reading the portal at start: about 1 ms (`docs/benchmarks.md`).
- Binary 3.41 MB, 2.85 MB stripped (+70 KB); no new library.
- Snapshots looked at: dark and light with GNOME's blue, dark orange, light green, light purple
  and red, with the title bar, the context menu and the rename field.

**Decisions**
- Selection, icons and the context menu's row take the accent, as GNOME and Plasma show them.
- The first test showed brownish dark surfaces with an orange accent: the tint went from 0.008 to
  0.005 chroma.

**Tested by the author** (2026-10-31, the VM): on GNOME 48.7 light/dark and the accent change
Nordend at once (screenshot: light, orange accent); on Plasma 6.7.5 too, Breeze and Breeze Dark and
the accent (screenshot: dark, the wallpaper's salmon accent, the same KWin shows), so
xdg-desktop-portal-kde gives `accent-color`. On Umbriel, with `colors.toml`: Noctalia's colours
as before. In the VM, GNOME Settings stays dark in light mode because of the Noctalia
`~/.config/gtk-4.0/gtk.css` there, not Nordend's business.

**Open**
- Plasma's colour schemes (Breeze's own greys and their many roles) are not read: only light/dark
  and the accent, through the portal.

---

## 2026-10-31 — Nordend on GNOME, Plasma, niri and Umbriel (ROADMAP 7.4, the author's tests)

**Done**
- The checklist of `docs/compatibility.md` on four desktops, results in its table.
- VM: the Void VM `nordend-dev` instead of an Arch one (decided with the author): GNOME 48.7 and
  Plasma 6.7.5 installed in it, Nordend built and tested there over SSH (copied with
  `git archive`). Snapshot `before-7.4` taken before the install.
- niri 26.04 nested in Umbriel on the host (in the VM niri needs virgl, which BUG-017 of
  void-noctalia rules out), with a minimal config without `prefer-no-csd`; removed afterwards.

**Measured**
- 249 tests pass in the VM (1 skipped: udisks2), Release build with GCC 14.2.

**Decisions**
- New item in 7.4, asked by the author before going public: without `colors.toml`, follow the
  desktop's light/dark and accent colour through the freedesktop portal.

**Tested by the author** (2026-10-31): every point of the checklist on GNOME (Nordend's title
bar; fractional scale after enabling `scale-monitor-framebuffer`), Plasma (KWin's bar), niri
(Nordend's bar; scale not tried, nested) and Umbriel (Umbriel's bar). Files opened in the apps of
each `mimeapps.list`, focused.

**Open**
- The definition of `nordend-dev` had virgl on again (`accel3d='yes'`, SPICE GL), although
  BUG-017 records it as removed; every older snapshot carries it. It booted with virgl for a few
  seconds before it was noticed and forced off; no amdgpu fault in the host log (*to be verified*
  as root). The definition and `before-7.4` are now without 3D; reverting an older snapshot
  brings virgl back (to note in void-noctalia, with the author's OK).
- KWin shows the generic Wayland icon: no desktop entry for the app id `nordend` (with the 0.1.0
  package, 7.8).
- Colours follow only Noctalia's palette (the new 7.4 item).

---

## 2026-10-30 — A title bar of its own where the compositor draws none (ROADMAP 7.4, third part)

**Done**
- `ui::Window`: client-side decorations when the compositor has no xdg-decoration, when it
  configures client-side mode, or with `NORDEND_FORCE_CSD=1` (then no decoration object is made,
  which tells the compositor the client decorates). `client_decorations()`, `state()` (maximized,
  fullscreen, activated, tiled per edge from configure; minimize, maximize and window menu from
  `wm_capabilities`, all assumed without it), and `move`, `resize(Edge)`, `minimize`,
  `toggle_maximized`, `show_window_menu`, `close`.
- `views::TitleBar`: the bar (36 px, `surface_container_low` like the sidebar, a hairline under
  it), the title centered on the window and never under the buttons, minimize / maximize
  (restore, `squares`, when maximized) / close at the right, close red under the pointer;
  buttons act on release over them; a press on the title moves the window, a double click
  maximizes, a right click opens the compositor's window menu; a 5 px band inside the window's
  edges (16 px at the corners) resizes it, not when maximized or fullscreen nor on a tiled edge.
  The actions go through `WindowControls`, a struct of functions wired by main (faked in tests).
  Fullscreen: no bar.
- `CursorShape` gains the eight edge and corner arrows (cursor-shape-v1 and xcursor names).
- main: the bar is drawn above the Browser, which gets the rest; the bar sees the pointer first,
  except while a press that started in the content is held; the Browser hears a leave when the
  bar takes the pointer.
- Snapshot: `--csd STATE` (`normal`, `maximized`, `inactive`, `no-minimize`, `hover-close`,
  `hover-maximize`, `press-close`).
- `docs/compatibility.md`: what depends on the compositor, what each desktop should give, the
  checklist for the author's sessions, and a results table.
- Tests (250): what is where (title, buttons, every edge and corner, content), maximized, tiled,
  fullscreen and no minimize; buttons act on release over them only, holding the pointer while
  pressed; drag, double click, right click, resize; the edges' cursors and back to the arrow.

**Measured**
- Headless weston 15 (`--renderer=pixman --debug`), screenshot by `weston-screenshooter`: the
  window with Nordend's bar, the title dimmed (weston activates no window without a seat), the
  three buttons, the Browser below unchanged. Snapshots in every state at scale 1 and 1.5 sharp.
- Binary 3.34 MB, 2.78 MB stripped.

**Decisions**
- The resize band is inside the window: GTK puts it outside, in an invisible margin, which needs
  extra surfaces (or a transparent margin around the buffer) and an offset window geometry.
  Inside, 5 px of the window's edge resize instead of reaching what is there.
- The move starts on the press (as weston's own clients do), not after a few pixels of drag.

**Tested by the author** (2026-10-30, Umbriel, `NORDEND_FORCE_CSD=1`, also with
`NORDEND_FORCE_XCURSOR=1`): move, double click, the buttons, the window menu, the edges and
their arrows, the clicks under the bar, and nothing changed without the variables. But Umbriel
drew its own title bar too (two bars): `NORDEND_FORCE_CSD` then made no decoration object, and
Umbriel decorates such windows anyway, although the protocol says that without a negotiation
"clients continue to self-decorate as they see fit" (a candidate for `buglog.md`, *to be
verified* in Umbriel's code, upstream only with the author's OK). Fixed on our side the same
day: the variable now asks for client-side mode through xdg-decoration and the compositor's
answer decides, so there can never be two bars. Tried again by the author: Umbriel answers
client-side and only Nordend's bar shows; without the variable, only Umbriel's. Umbriel's
behaviour is NB-003 in `buglog.md`.

**Open**
- No rounded corners, no shadow.
- The title is always "Nordend" (the folder shown could go in it).
- Umbriel decorates windows that make no decoration object (NB-003, suspected).

---

## 2026-10-30 — A cursor of its own without cursor-shape-v1 (ROADMAP 7.4, second part)

**Done**
- `ui::XcursorFallback` (`ui/xcursor.hpp`): when the compositor has no cursor-shape-v1 (GNOME,
  weston), the pointer's image comes from the xcursor theme through `libwayland-cursor`:
  `XCURSOR_THEME` (its "default" without it), `XCURSOR_SIZE` (24 unless a number from 1 to 256),
  loaded at the window's scale rounded up and reloaded when it changes, on a surface of
  Nordend's own set with `wl_pointer.set_cursor` on every enter and every shape change.
- Size: with viewporter the surface gets the image's logical size whatever size the theme has
  (Adwaita has no 72 px cursor for 24 px at scale 3); without it the buffer scale is used only
  when it divides the image, else the image shows at scale 1 (too large, but no protocol error).
- Names: each shape tries the cursor spec's CSS name first, then the X11 names older themes use
  (`pointer`, `hand2`, `hand1`...). Animated cursors (`wait`, `progress`) show their first frame.
- `Display` binds `wl_shm`; `Seat` picks cursor-shape-v1 or the fallback when the pointer
  appears, and takes the window's scale from main's draw handler (`set_cursor_scale`).
  `NORDEND_FORCE_XCURSOR=1` uses the fallback even with cursor-shape-v1, to try it on Umbriel.
- Links `wayland-cursor` (part of `wayland-devel`, same package as `wayland-client`).
- Tests (245): every shape has a CSS name then an X11 one, `XCURSOR_SIZE` parsing, every
  shape's first name present in the installed Adwaita theme.

**Measured**
- Binary 3.32 MB, 2.76 MB stripped (+15 KB); 21 direct libraries (+ `libwayland-cursor`).
- Headless weston (`--backend=headless`) offers no `wl_seat`: no pointer, so the fallback cannot
  be exercised there. Verified by the build, the tests and, for the author, on Umbriel with
  `NORDEND_FORCE_XCURSOR=1`.

- Under headless weston the ASan build reports a 128-byte leak in an unloaded module (no
  symbols; most likely Mesa's software driver, as EGL finds no device there). The commit before
  this step, built the same way, leaks the same 128 bytes: not ours, not on Umbriel.

**Tested by the author** (2026-10-30, Umbriel with `NORDEND_FORCE_XCURSOR=1`): Adwaita at 24 px
and scale 1 in the log, the arrow, the double arrow on the column dividers, 48 px with
`XCURSOR_SIZE=48`, and the normal path unchanged without the variable. The test plan also asked
for a hand over buttons and a text cursor while renaming: a slip of the copilot's, Nordend never
asks for those shapes (only `standard` and `ew_resize`, with cursor-shape-v1 too).

**Open**
- The hand and text shapes exist in `CursorShape` but no view uses them (to propose).
- The busy cursors are not animated (first frame only).
- On integer scale without viewporter a theme without the exact size shows a larger cursor.

---

## 2026-10-30 — Nordend's own icon font (ROADMAP 7.4, first part)

The author approved vendoring a subset of the official Tabler Icons font and Arch as the VM
distro for the compatibility tests.

**Done**
- `third_party/tabler-icons/`: the official webfont (npm `@tabler/icons-webfont` 3.50.0, the
  same files as the GitHub release `v3.50.0`, which ships no font asset) cut down by
  `scripts/make-icon-font.sh` with `pyftsubset` to the glyphs in `icons.txt` (47 outline) and
  `icons-filled.txt` (1 filled: `trash`), glyph names kept, layout features dropped; the
  upstream `LICENSE` beside them. The script checks the tarball's sha256 and gives the same
  bytes on every run (checked twice).
- `cmake/embed.cmake` turns each font into a C++ array at build time (`nordend::embedded`), so
  the fonts are inside the program: no install path, no lookup, and dev runs, tests and the
  snapshot tool all use the same font.
- `FontSystem::add_font_data` loads a font from memory (`FT_New_Memory_Face`, a read-only
  harfbuzz blob); `add_font_file` goes through it.
- `IconSet` uses the two embedded fonts; a name ending in `-filled` is looked up in the filled
  font without the suffix (upstream calls it `trash`, Noctalia's merged font `trash-filled`). The
  XDG search and Noctalia's `noctalia-tabler.ttf` are gone: the embedded font is always there,
  so a fallback would only hide a missing name. An unknown name still draws `file` and is now
  logged once.
- Also kept for the title bar to come: `minus`, `square`, `squares`, `x`.
- Tests (242): the icon tests no longer skip without Noctalia; every name of the MIME table, the
  places and the views' own (sidebar, breadcrumb, status bar, info pane, title bar) is in the
  font, `trash-filled` too; `folder-filled` and `-filled` are not.

**Measured**
- Font: 13 432 bytes (outline) + 1 072 (filled), against Noctalia's 2 413 312.
- Binary: 3.30 MB, 2.75 MB stripped (+5 KB unstripped: the README's figures do not change).
- Snapshots of the columns, grid and details views (offscreen, also under ASan): the same icons
  as before, no warning about a missing name.

**Decisions**
- Embedded rather than installed under `share/nordend/fonts`: 14 KB is nothing, and the
  program then needs no data files at all.
- No fallback to Noctalia's font, as explained above (the plan said "ours first, Noctalia's
  as fallback"; the author is told).

**Tested by the author** (2026-10-30, on Umbriel): the icons everywhere as before, the full
Trash icon, `loaded font tabler-icons.ttf` in the log, no `no icon named`, and Noctalia's font
not open among the process's file descriptors.

**Open**
- A new icon means: add its name to `icons.txt`, run `scripts/make-icon-font.sh`, update the
  sha256 in `THIRD_PARTY.md`.
- Next in 7.4: the fallback cursor, then client-side decorations.

---

## 2026-10-30 — Removable disks through udisks2 (ROADMAP 7.3)

"Fatto quando" proposed and approved by the author the same day, with sdbus-c++ 2.3 as the
D-Bus library (a system library: `sdbus-c++-devel`, recorded in DESIGN and README), no automount,
LUKS after the MVP.

**Done**
- `platform/volumes`: plain data and pure functions. `UdisksObject` (Block, Filesystem, Drive
  properties), `volumes_from` (a Block with a Filesystem, usage "filesystem", not HintIgnore;
  the system's own only while mounted; external = removable, ejectable or on USB/SD/FireWire),
  `size_name` ("466 GB Volume"), `with_volumes` (mounted devices learn their volume and become
  ejectable with a USB icon; volumes not mounted are added without a path), `on_same_drive`.
- `platform::Udisks`: a system bus connection whose socket, event fd and timeout are watched by
  `core::EventLoop` (`getEventLoopPollData`/`processPendingEvent`, re-armed after every pump);
  `GetManagedObjects` read at start and 150 ms after any burst of udisks2 signals; `mount`,
  `unmount`, `eject` (every mounted file system of the drive unmounted in turn, then `PowerOff`,
  else `Eject`), all asynchronous; errors made readable ("in use", "not allowed", the text after
  udisks2's last ": "). Without a bus or udisks2: a warning, `available()` false.
- `fs::Place::Disk` (volume, mounted, ejectable). Sidebar: disks not mounted drawn quieter, an
  eject icon on external disks (its own click, hover), the place handed whole to the open
  handler; a disk not mounted is never a drop target and holds no folder.
- Browser: `set_devices`; a click on a disk not mounted mounts it, opens it, then checks on a
  worker whether it came read-only ("“kinetic” is read-only: it was not closed properly
  (Windows' chkdsk or ntfsfix -d fixes it)"); eject and unmount first take the view home when it
  shows that disk; messages for in use, errors, "can be unplugged now"; a disk's right click:
  Mount, Unmount, Eject.
- Fixed on the way (since 6.x): a right click that opened a menu left `button_held_` set (the
  release went to the menu), so the next click in the view still went to the sidebar.
- Snapshot: `--fake-disks`.
- Tests (242): which volumes show and their names, the author's machine as udisks2 describes it
  (USB Kinetic, internal DATA, the other system's partition hidden, swap/crypto/ignored left
  out); the places merged; the drive's file systems for eject; mount then open, a mount error;
  eject and unmount leaving the disk first, in use, unplugged, not allowed; the eject icon and
  the disk menu clicked for real (offscreen); a drop refused onto a disk not mounted; udisks2 on
  the real system bus listing the disks, read only (/boot/efi, DATA, the root here).

**Measured**
- The sidebar snapshot with a disk not mounted and one mounted, both ejectable. ASan and TSan
  clean, the real-bus test included.

**Open**
- LUKS volumes are not listed (after the MVP).

**Tested by the author**: the Kinetic plugged in shows up quieter without mounting, a click
mounts and opens it, eject from inside FOTOGRAFIA goes home and powers it off ("can be unplugged
now"), unplugged and plugged in again, "in use" while a terminal stood in it, and Unmount, Mount,
Eject from the right click.

---

## 2026-10-30 — Sidebar favourites (ROADMAP 7.2)

"Fatto quando" proposed and approved by the author the same day (a separate drop zone for
bookmarks; reordering and renaming after the MVP).

**Done**
- `core::write_file_atomically` (hidden temporary beside the file, fsync, rename; the folder
  made, the permissions kept, 0644 for a new file) and `core::file_uri`, moved from `thumbs`
  (`thumbs::file_uri` is now an alias) so `fs` writes URIs exactly as GLib does.
- `fs::bookmarks_file`, `fs::add_bookmark`, `fs::remove_bookmark`: text in, text out; the other
  lines (`sftp://`, labels, order, a missing final newline) stay; the same folder with or without
  a trailing slash is one bookmark. `PlacesWatcher::reload()` is public and starts watching a
  file whose folder appeared since (`gtk-3.0` made by the first bookmark).
- Browser: `add_to_sidebar`, `remove_from_sidebar`, applied one at a time on a worker (read,
  change, write), then the places are read again; messages "“Foto” added to the sidebar", "2
  folders added", "Already in the sidebar", "Only folders go in the sidebar", "“Foto” removed",
  "The sidebar could not be changed: …". Ctrl+D (the folder shown), "Add to Sidebar" in the menu
  of selected folders and of a folder's empty part, "Remove from Sidebar" on a bookmark's right
  click.
- Sidebar: `in_bookmark_zone` (the "BOOKMARKS" title, or the space below the last place) and a
  line where the bookmarks would go; dropping folders there adds them (Nordend's own drag only
  when every entry is a folder; another program's: whatever is not a folder is left out), and
  nothing moves. Dropping onto a place still moves into it.
- Tests (235): the text functions (GLib escaping with a space and "ï", duplicates, removal, other
  lines kept); the atomic write (folder made, permissions, nothing left beside it, an error);
  Ctrl+D twice, the menus, removal, a write error; folders dropped below the places and on the
  title, a file refused, the bookmark's menu.

**Measured**
- Snapshots: the insertion line in the sidebar, the menu with "Add to Sidebar  Ctrl+D", the
  three-column overlay with the new line. ASan and TSan clean.

**Decisions**
- Bookmarks are written by Nordend, not through GLib: the file format is a line per bookmark,
  and the URIs match GLib's byte for byte (`core::file_uri`, checked against gio in 5.x).

**Tested by the author**: Ctrl+D and "Already in the sidebar", "Add to Sidebar" on two folders,
folders dragged onto the "BOOKMARKS" title (the line after the last bookmark), a file refused
there and moved when dropped onto a bookmark, "Remove from Sidebar", the file's escaped name,
and the bookmarks in Firefox's Open dialog (Ctrl+O).

**My slip**
- The first full test run after adding Ctrl+D wrote three bookmarks of temporary test folders
  into the author's real `~/.config/gtk-3.0/bookmarks` (the test that presses every key pressed
  Ctrl+D, and tests read `$XDG_CONFIG_HOME`). The file did not exist before (the sidebar showed no
  bookmarks); removing it was left to the author. Fixed for good: the test runner points
  `XDG_CONFIG_HOME`, `XDG_DATA_HOME` and `XDG_CACHE_HOME` at its own temporary root before any
  test runs; a full run now leaves the real file byte for byte as it was (checked with `cmp`).

---

## 2026-10-30 — Drag and drop (ROADMAP 7.1)

"Fatto quando" proposed and approved by the author the same day (copy only towards other apps;
the sidebar's Trash trashes; a drag image; spring-loading after the MVP).

**Done**
- `ui::Clipboard` carries drag and drop on its `wl_data_device` (v3): `start_drag` (a second
  `wl_data_source`, offered for copy only, its own formats; `dnd_finished`/`cancelled` end it),
  `cancel_drag`; the drop side reports enter/motion/leave/drop (`DropEvent`), keeps the offer's
  `source_actions` and agreed `action`, `accept_drop` (accept + set_actions), `read_drop` (then
  `wl_data_offer_finish`) and `finish_drop` (Nordend's own drag, not read). A dropped offer
  survives the leave that may follow the drop.
- `ui::Window::make_drag_icon`: a second surface (viewport at fractional scales) drawn once with
  the window's renderer into its own EGL surface; `ui::Seat::modifiers()`.
- `ui::Offscreen` (surfaceless EGL + framebuffer), moved out of the snapshot tool, so tests can
  draw: the drag and drop tests lay the browser out for real and point at what it shows.
- Views: `hit(x, y)` (folder and entry under a point), a `DropMark` drawn as the target (an
  entry: primary container with a ring; a whole column or view: an outline); sidebar
  `place_under` and breadcrumb `path_at`, both with a marked drop target.
- `Folder::press`/`release_press`: a plain click on an entry selected with others keeps the
  selection until the button goes up, so the press can drag them all (as Finder and GTK do).
- Browser: a press on an entry moved 6 px starts a drag (the selection when the entry is in it;
  never from the trash); offered as `text/uri-list`, `text/plain;charset=utf-8` and `text/plain`
  (paths). Escape cancels. Over the window: the target is a folder entry, the empty part of a
  column or view (its folder), a breadcrumb step or a sidebar place; refused (not accepted, not
  marked) when it is the dragged folder or inside it, or where the items already are; from
  another program, only drags carrying `text/uri-list`. Drop: Nordend's own drag decides by
  itself (Ctrl copies, Shift moves, else a `stat` on a worker: same device moves, another
  copies); another program's follows the action agreed (copy unless the source allows move and
  the compositor chose it); the Trash place trashes; non-file URLs: "Only files can be
  dropped". Everything goes through `transfer_paths` (one batch: Ctrl+Z, sidecars, conflicts,
  progress). Near the top or bottom edge of the view (40 px) a timer scrolls it while the drag
  stays there.
- Snapshot: `--drag NAME` (the drag image drawn by the pointer), `--drop-over X,Y`.
- Tests (231): the drag of a selection (and a click that collapses it on release), the formats,
  Escape; drops in the columns moving and Ctrl copying, then Ctrl+Z; Shift onto a column's empty
  part; refusals into itself and where they are, leave clearing the mark; another program's file
  with a space and "ï" copied, a web URL refused, a drag without files ignored; the breadcrumb, a
  sidebar place, the trash.

**Measured**
- Snapshots with the author's palette: a folder row, a column, a breadcrumb step, the sidebar's
  Trash as targets, the drag image. ASan and TSan: tests and a drag snapshot clean.

**Decisions**
- Towards other programs only copy (the source offers `copy`): Nordend never deletes its files
  because another program says it took them. Inside Nordend the move is Nordend's own decision.
- *To be verified on Umbriel*: whether the keyboard's modifiers and Escape still reach Nordend
  during its drag (the compositor holds the pointer); whether the compositor applies Ctrl/Shift
  to the action of another program's drag.
- Fixed on the way: `drop_paths` read `paths.front()` in one lambda while another moved `paths`
  in the same call (argument order is unspecified): a crash under the first test.

**Open**
- Spring-loading (hovering a folder opens it) after the MVP, as agreed.
- Escape does not stop a drag on Umbriel: during the drag no key reaches Nordend, and Umbriel
  does not cancel by itself (buglog NB-002, suspected). Ctrl and Shift do reach it.

**Tested by the author**: moves and copies in the columns with Ctrl and Shift (sidecars along),
Ctrl+Z, a whole selection dragged, refusals into itself and where they are, the breadcrumb, the
sidebar and the Trash, a path dropped into a terminal, a photo into Firefox and a download from
Firefox into Nordend, a web image refused, auto-scroll near the edges. Escape did not stop a drag
(see Open).

---

## 2026-10-30 — Navigation history and hidden files (ROADMAP 7.0)

First step of phase 7; the "Fatto quando" proposed and approved by the author the same day.

**Done**
- `fs::Listing` leaves hidden entries (dotfiles) out on request: kept aside unsorted
  (`hidden()`), still updated by additions, replacements and removals, merged back in order when
  shown again; one name (`keep`) stays shown either way. `set_filter` returns the names that left
  and came. `DirectoryModel::set_filter` notifies them as removed and added, so the views and the
  selection follow with no new path; its trash diff and its known names count the hidden too.
- `Folder(…, show_hidden)` and `Folder::set_show_hidden(show, keep_cursor)`: left out, hidden
  names leave the selection (Ctrl+A then Delete never touches what is not seen); a cursor on one
  moves to the next entry shown, else the one before, the rest of the selection kept; with
  keep_cursor the entry stays (the columns before the focused one keep the way). `select_name`
  and `expect_name` keep a hidden name in sight: Backspace from `.config` opened from a bookmark,
  a reveal through it, a rename (F2) to ".name".
- The three views create their folders with the setting and apply it (`set_show_hidden`); the
  browser's Ctrl+H toggles it for all, with a message. Hidden by default; dotfiles shown are
  still dimmed. Status bar: "6 items, 5 hidden". The info pane counts a folder's items as it will
  show them (`preview::Info::hidden_items`).
- History in the browser: `note_location()` after every key, button press/release and
  open/go_to/reveal (and before every key and button, so the step left keeps its cursor); a new
  folder is a step (in the columns, the focused column's; not while a reveal is still unfolding),
  otherwise the step's cursor is updated. Alt+Left/Alt+Right and the mouse's side buttons
  (`travel`) go to the step with `go_to` and put the cursor back by name; a new step after going
  back drops the ones ahead; at most 100. A folder gone is found at once (opening it fails to
  watch: ENOENT), dropped from the history, the next one tried; nothing left: back where it was;
  "a no longer exists" / "2 folders no longer exist". The side buttons no longer go to the parent
  in the views (Backspace and Alt+Up still do).
- `ColumnView::sync_after` now drops a reveal whose way is lost (an entry not found), which
  otherwise stayed pending forever; `revealing()` tells the browser.
- Shortcuts: Ctrl+H "Hidden files" (General); "Alt+←  Alt+→" "Back, forward (mouse too)"
  (Moving), the second chord a line without keys (the overlay skips those).
- Tests (225): back and forward with the cursor in all three views, ends of the history; side
  buttons; steps ahead dropped; folders gone, skipped and none left; 100 steps; Ctrl+H in all
  views with status and selection; the cursor's nearest entry after and before; the way to a
  hidden folder kept in the columns; a rename to a hidden name; `Listing` filtering; the info
  count.

**Measured**
- Snapshots (columns, grid, details, hidden off and on; the overlay at 1400×800 and 1000×800)
  looked at. ASan and TSan: tests and a snapshot clean.

**Decisions**
- The filter lives in `fs::Listing`, not in the views: every view, count and selection reads the
  same `entries()`, so nothing can act on an entry it does not show.
- Existence is not checked on a worker before going back: opening a missing folder fails at once
  on the loop thread (the watch), the same syscall any open makes.

**Open**
- The two-column overlay (window narrower than 1200 px) now cuts Quick Look's lines at 800 px
  of height: two lines more than before (already an open point).
- Renaming the cursor's entry to a hidden name from another program loses the cursor (only F2
  keeps it).

**Tested by the author**: hidden files off at start with the count, Ctrl+H both ways, the
cursor moving off a dotfile, Ctrl+A leaving dotfiles out, `.config` kept in the column before;
back and forward with keys and the mouse's side buttons in all three views, cursor restored; a
folder deleted meanwhile skipped with its message; the overlay's new lines.

---

## 2026-10-30 — The README, ready to go public

Asked for by the author ("rendi il readme carino", the repository goes public soon).

**Done**
- The README opens with the name, a one-line pitch, the badges and a screenshot of the column
  view (the author's, `docs/images/screenshot-columns.png`), then "Why Nordend" (photos first,
  three views, measured speed, natural scrolling, care with files, keyboard), the status as a
  note, and the sections as before. The tests badge counts 215 under ASan and TSan. "Building"
  says plainly that the icons come from Noctalia's font for now.
- The screenshot showed people's names among the author's folders: three rows are blurred in the
  committed copy (`magick`, `-blur 0x7` over each row); the original was never committed.
- `docs/benchmarks.md` no longer names the author's photo folders by path.

---

## 2026-10-30 — Undo the last operation on files (ROADMAP 6.7)

Proposed after 6.6 and approved by the author, with two choices of theirs: an undone copy goes
to the trash (not deleted), and Ctrl+Shift+Z redoes.

**Done**
- `fs::TransferResult::items` (`PlacedItem{from, to}`): for each item placed, sidecars left out,
  where it was and where it is now: copies and moves (`to` with the name kept, "P1 2.ORF"), the
  trash (`to` in files/, `trash_item` now returns it), restores (`from` in files/).
- `views::FileOperation` (`undo.hpp`): kind (rename, create, copy, move, trash, restore, erase)
  and items; `describe()` for the status bar. Undoing one runs its inverse, and the inverse's
  own result is recorded as what Ctrl+Shift+Z undoes: rename ↔ rename back; move ↔ move back;
  trash ↔ restore; copy, create → trash (whose inverse, restore, is the redo). A permanent
  delete (and Empty Trash) is recorded as `erase`: "A permanent delete cannot be undone".
- Browser: every transfer goes through `submit(request, batch)`; a batch (`open_batch`) is one
  operation, possibly several transfers (a paste from two folders, a move back to two folders);
  when its last transfer is done, `record()` makes it what Ctrl+Z undoes (the user's: redo
  forgotten; an undo's: what Ctrl+Shift+Z redoes; a redo's: what Ctrl+Z undoes again). Renames
  (F2) and new folders/files are recorded when the worker answers. `undoing_` refuses another
  Ctrl+Z while one is on its way ("Still undoing the last one"). A new folder or file is taken
  away only if it is as it was made (an empty folder; a file as long as its template, or empty),
  checked on a worker.
- What changed meanwhile goes through the engines' own errors and messages ("Moved 0 files; 1
  could not be: P2.ORF: ..."); nothing undone, nothing to redo, and Ctrl+Z answers again.
- Shortcuts: Ctrl+Z "Undo the last operation", Ctrl+Shift+Z "Redo it" (views, Quick Look); the
  name's field keeps its own Ctrl+Z while editing. The two-column overlay moved Quick Look to the
  left column, the shorter one.
- The shortcuts test's reset now cancels a conflict card: pressing every key undoes a Delete,
  whose restore finds the name made again and asks.
- Tests (215): rename with sidecars undone, redone, undone again, "Nothing to undo/redo"; a move
  back and forth; a copy to the trash and restored by redo; Delete restored and trashed again; a
  permanent delete refused; a new folder kept while it holds a file, then trashed; a moved file
  removed meanwhile; the messages.

**Tested by the author**: a rename with its sidecar undone, "Nothing to undo", redone; a move
to DATA undone and redone; a copy undone into the trash (`trash-list` showed the copy and its
sidecar) and redone; a Delete undone; a new folder undone; a permanent delete refused.

**Decisions**
- Only the last operation, in memory: a history with a list is ROADMAP 9.
- An undo's undo is a redo by construction (the inverse of the inverse), so Ctrl+Shift+Z never
  needs its own engine.

**Open**
- A moved item that took another name ("P1 2.ORF", keep both) comes back under that name.
- An undone copy that had replaced a file (Replace on the card) cannot bring the replaced file
  back: only the copy goes to the trash.
- Ctrl+Z inside the trash folder works on the last operation, wherever it was.

---

## 2026-10-30 — Undo in the name's field (ROADMAP 6.6)

Asked for by the author after 6.5 ("premo Ctrl+Z e non torna indietro"); the order 6.6 (the
field), 6.7 (the last file operation), then phase 7 agreed with them.

**Done**
- `ui::TextField` keeps the states before each edit (text, caret, selection; up to 100) and
  those undone: Ctrl+Z restores the last, Ctrl+Shift+Z gives it back; any new edit forgets what
  could be redone, `set_text` forgets everything. Grouping: characters typed one after another
  are one edit, the selection they replaced included (F2, "Lake", one Ctrl+Z: the old name);
  characters removed one after another (Backspace, Delete) are one; a selection or a word
  removed (Ctrl+Backspace) is one; `insert` (a paste) is one; moving the caret or selecting ends
  the edit going on.
- The shortcuts list "Ctrl+Z — Renaming: undo (Shift: redo)" (`any_shift`).
- The shortcuts overlay had outgrown two columns: at 800 px of height Quick Look's lines were
  cut off since 6.5's Ctrl+V line (a slip of mine: I did not look at the overlay then). It now
  shows three columns when the window has 1 200 px for the card (General, Grid, Quick Look |
  Moving, Selecting, Sidebar | Files), two otherwise as before. Checked at 1280x800 and
  1100x900.
- Tests (208): grouping, the selection given back, redo, redo forgotten after a new edit, a
  paste undone; in the browser, F2, Ctrl+V of a text, Ctrl+Z gives the name back.

**Tested by the author**: F2 and "Lake" undone and redone, a pasted "Lago di Como" undone,
three Backspaces undone at once, the overlay in three columns and back to two when narrowed.

**Open**
- In a narrow, short window (two columns, under ~850 px of height) the overlay still cuts its
  last lines: it does not scroll.

---

## 2026-10-30 — The system clipboard (ROADMAP 6.5)

Proposed to the author with its "Fatto quando" and approved; Firefox is the other program to try.

**Done**
- `ui::Clipboard` on `wl_data_device` (manager bound up to v3): offers a `wl_data_source` with
  several types and writes each request to its pipe from the loop (non-blocking, `POLLOUT`, a
  slow reader never blocks the window); reads the selection's offer through a non-blocking pipe
  the loop watches, up to 64 MB, given up after 3 s; keeps every offer's types; drag offers are
  dropped when the drag leaves (drag and drop is not taken yet); `cancelled` on Nordend's source
  = another program copied: `set_lost_handler`. SIGPIPE is blocked with the other signals and
  ignored when it arrives (a reader that went away: the write fails with EPIPE); applications
  started from Nordend get an empty mask as before.
- `views/file_clipboard`: `uri_list` (CRLF, URIs from `thumbs::file_uri`, GLib's escaping),
  `gnome_copied_files`, `path_lines`; `path_from_uri` (file://, empty host or localhost,
  case-insensitive scheme, `%XX` undone, `?#` cut, a NUL refused, a trailing slash dropped),
  `parse_uri_list` (comments, CRLF or LF), `parse_gnome_copied_files`. `SystemClipboard`: the
  browser's view of it as functions (offer, clear, owned, types, read); the app wires the real
  one, tests a fake.
- Browser: Ctrl+C / Ctrl+X offer `x-special/gnome-copied-files`, `text/uri-list`,
  `text/plain;charset=utf-8`, `UTF8_STRING`, `text/plain` (paths), and
  `application/x-kde-cutselection` "1" for a cut. Ctrl+V: Nordend's own offer → the internal
  clipboard as before (a cut pasted once now also empties the system clipboard); another
  program's → its files (`gnome-copied-files` first, else `text/uri-list`, with KDE's cut type
  read first when there), one transfer per source folder, cut = move; text only → "The clipboard holds no files"; other hosts or schemes → "Only files
  on this computer can be pasted". Losing the clipboard forgets what Ctrl+C/X took and the cut
  marks. Ctrl+V in the name's field reads the first text type offered and inserts its first line
  without control characters (`NameEditor::paste`); listed in the shortcuts (`kEditor`).
- Another program's cut empties the clipboard only once something arrived, and only if the
  clipboard still holds it (`Clipboard::generation()`, changed by every new selection): every
  transfer goes through `Browser::submit`, which keeps, in the queue's order, the generation to
  empty after it. Found in the author's test: a cut of files that did not exist (a fish variable
  unset in the second terminal) had emptied the clipboard anyway.
- Tests (207 with the raw samples): URIs and lists both ways with odd names; what a list
  refuses; Ctrl+C/X offering the formats; Nordend's own cut pasted once and the clipboard
  emptied; losing it; another program's URI list from two folders (copied), GNOME's and KDE's
  cut (moved, sidecar along); a web URL refused; a cut that moved nothing, or a clipboard that
  changed meanwhile, not emptied; Ctrl+V in the name's field.

**Tested by the author** (the Wayland side: no headless compositor here): `wl-paste
--list-types` showed the five types, `wl-paste -t text/uri-list` the URIs of `a b%#?.txt` and
`caffè.txt` as written above, plain `wl-paste` the paths; Firefox pasted a file copied in
Nordend; a `wl-copy -t text/uri-list` of `P1.ORF` and `x.txt` was copied with the sidecar; a
GNOME cut of `z.txt` moved it; `wl-copy hello` ended the cut fading; Ctrl+V in the name's field;
0 CPU ticks in 30 s idle. After the GNOME cut, `wl-paste --list-types` listed text types
(`TEXT`, `STRING`, ...) that Nordend does not offer: likely a clipboard manager putting back its
last text entry (*to be verified*); there were no files on it. After the fix: a cut of a missing
file left `x-special/gnome-copied-files` on the clipboard; a cut of `w.txt` moved it and took it
off.

**Decisions**
- When Nordend owns the clipboard, Ctrl+V uses its own copy of what was taken, not a read
  through the compositor: no round trip, and "a cut is pasted once" stays exact.
- After pasting another program's cut, Nordend empties the clipboard, as GNOME's file manager
  does: the files are no longer where its URIs say.
- Text is offered as full paths, one per line (what a terminal wants), not names.

**Open**
- Images on the clipboard (`image/png`, a screenshot) are not pasted as new files yet.
- The clipboard dies with Nordend unless a clipboard manager keeps it (Wayland's way).
- Ctrl+C in the name's field does not copy text yet.

---

## 2026-10-30 — Empty Trash and Delete Permanently (ROADMAP 6.4d)

The half of 6.4c that deletes, as split with the author's OK.

**Done**
- `fs/erase` (`plan_erase`, `run_erase`; `TransferKind::erase` and `empty_trash`, run by the
  transfer queue): deletes for good, what a folder holds first, through folder descriptors
  (`openat` + `fstatat` + `unlinkat`), links never followed; a folder of the user's without
  u+rwx is opened up first (it is going anyway). A regular file takes its darktable sidecars; a
  photo that stays keeps them. Errors per entry ("Trip/x.txt: Permission denied"), cancellation
  between entries. Progress counts entries: a first walk adds them up.
- From the trash folder (`source` = `trash_path()`, names = paths in files/): each item's
  `.trashinfo` goes once the item is gone, the sidecars are those Restore would take along
  (`trashed_groups`, sharing `trashed_sidecars` with Restore), and `directorysizes` loses the
  folder's line (`prune_directory_sizes`; the file goes when empty). Empty Trash: every
  existing trash (home and disks, `existing_trashes`, found on the worker), everything in files/,
  then every `.trashinfo` whose item is gone (orphans too), then `directorysizes`.
- `views::ConfirmDialog`: a card like the conflict one: title, what goes, "Nothing goes to the
  trash: this cannot be undone.", Cancel (Escape) and a red Delete / Empty (Enter); it takes every
  key and click while open. It opens at once with "Adding up the size…"; `plan_erase` runs on a
  worker and fills in "With its darktable sidecar · 21.0 MB · 55 files and folders in all". Enter
  before the count arrives works: the delete is submitted when it does.
- Shift+Delete (views and Quick Look; `Command::erase`), Delete in the trash folder; menus:
  "Delete Permanently…" on entries (and on trashed ones, under Restore), "Empty Trash…" on the
  trash's empty space and on a right click on Trash in the sidebar (`Sidebar::set_menu_handler`,
  the sidebar's first menu; disabled while the trash is empty). Status bar: "Deleting 30 of 120
  files and folders", then "Deleted “P1.ORF” permanently (21.0 MB)" / "Emptied the trash: 12
  items deleted (412 MB)".
- Shortcut scopes are 16 bits now (`kConfirm` was the ninth); the overlay lists Shift+Delete and
  the card's keys. `nordend-transfer --erase` for measuring.
- Tests (201 with the raw samples): the engine (sidecars, a read-only folder, a hard link counted
  once, a link not followed, cancel, from the trash with `directorysizes`, Empty Trash over two
  trashes with an orphan `.trashinfo` and an item without one); the browser: Escape keeps
  everything, Enter before the count, the cursor goes on, every key listed for the card, Delete
  and Empty Trash in the trash folder, the icon back to empty, "The trash is empty already".

**Tested by the author**: on screen with the real trash (home, DATA and /tmp's): Shift+Delete
on a photo and on a folder holding a read-only one, Delete and Empty Trash in the trash folder,
Empty Trash from the sidebar's menu; afterwards `trash-list` showed nothing and the home's files/
and info/ and DATA's files/ were empty.

**Measured** (benchmarks): 10 101 entries in 0.20 s on ext4, `rm -rf` 0.17 s; on tmpfs 80 ms
against 54 ms. The difference is the counting walk.

**Decisions**
- Enter confirms (the Delete button is the default): the user already pressed Shift+Delete or
  chose the menu item on purpose; Escape and Cancel are the way out. Proposed to the author.
- Sizes are the files' sizes (`st_size`, what the views show), not blocks as in
  `directorysizes`.
- A sidecar selected along with its photo is counted as an item, not again as "with its
  sidecar".

**Open**
- No undo, by definition: the card is the only guard.
- The sidebar's menu has only the trash's item; other places have none yet.
- The progress bar counts entries, not bytes: one huge file is one step.

---

## 2026-10-29 — The trash in the sidebar, and Restore (ROADMAP 6.4c)

Asked for by the author after 6.4b ("manca l'icona del cestino?"); split in two with the author's
OK: this step only gives back, 6.4d (Empty Trash, Delete Permanently) is the one that deletes.

**Done**
- A "Trash" place after "File system", with Noctalia's `trash` icon, `trash-filled` while
  anything is in a trash (the Browser keeps a `DirectoryModel` on the trash for it).
- The trash folder, `fs::trash_path()` ("trash:///", not a real path): `DirectoryModel` lists it
  from every trash, the home trash and each mount point's `.Trash/uid` and `.Trash-uid`
  (`existing_trashes`), each item by its original name; when several share one, the oldest
  keeps it and the others are numbered as GLib numbers trashed names ("a.2.txt"). Items without
  a `.trashinfo`, and `.trashinfo` without an item, are left out (as `trash-list` does). The
  info/ folders are watched; any change reads the whole trash again on a worker and applies the
  differences. The home trash is made when missing, so it is watched from the start.
- `.trashinfo` parsing (`parse_trash_info`): `[Trash Info]` first, the first `Path` and
  `DeletionDate`, escapes undone, relative paths joined to the top directory (to
  `$XDG_DATA_HOME` for the home trash).
- `Folder::path_of(entry)`: where an entry really is. The ten places that joined the folder's
  path and the name (previews, thumbnails, Quick Look, the info pane, the columns, entering a
  folder, sniffing types) use it, so the trash folder shows previews, thumbnails and Quick Look
  like any other; a trashed folder can be entered (it is a real folder in files/).
- Read-only: Delete, F2, Ctrl+Shift+N, Ctrl+C/X/V say so; Enter on a file says "Restore it first
  to open it"; no menu on empty space. The breadcrumb shows the trash icon and "Trash". The info
  pane adds "Original location" and "Deleted".
- Restore (`TransferKind::restore`, `run_restore`), from the menu: each item goes back to its
  original path, the folder made again (0777 less the umask) if it is gone; a taken name asks on
  the conflict card (Skip, Keep both, Replace for a file over a file; a folder is never merged,
  `Conflict::merge` false hides Replace there); the `.trashinfo` is removed after the move; a
  photo takes along its sidecars trashed from the same folder (the newest of each name), renamed
  with it when kept both. The cursor goes on to the next entry, as Delete does.
- Slip, found in the author's first try (the DATA trash was missing from the view): the mount
  points were read with `core::read_text_file`, which reads as many bytes as a file reports, and
  `/proc` files report 0. `places` already had the right reader; it is now `fs::read_proc_file`,
  used by both, and a test reads the real `/proc/self/mountinfo` through `fs::mount_points`.
- Tests (194 with the raw samples): `.trashinfo` parsing; two trashes merged, names numbered,
  orphans left out; restore with a missing folder and sidecars; conflicts (skip, keep both with
  sidecars, replace, a folder not replaced); the model following an external trashing and a
  restore; the browser: what is refused, the menu's Restore, the icon full then empty. The test
  binary now sets its own trash (`tests/main.cpp`): no test reads or makes the user's.

**Tested by the author**: on screen with the real trash (items left by the day's tests, from
folders since deleted) and one on DATA. The view showed what `trash-list` showed (after the
mountinfo fix); `gio trash` from a terminal appeared at once; Restore brought `P1.ORF` back with
its sidecar into a folder made again, Keep both gave `uno 2.txt` next to the new `uno.txt`, five
items restored at once; `trash-list` afterwards: only the author's own item.

**Decisions**
- Original names, not the names in files/: what the user knows. The numbering only shows when
  the same name was trashed twice.
- One model for the trash rather than one folder per trash: the photos trashed on DATA show
  with the home's, as `trash-list` shows them.
- The sidebar's icon follows the trashes watched when the browser started: a trash made later
  on another disk (the first file ever trashed there) shows at the next start.

**Open**
- No sorting by deletion date, no "Original location" column in the details.
- A trash on a disk mounted after the trash folder was opened appears when it is opened again.

---

## 2026-10-29 — Context menu (ROADMAP 6.4b)

Asked for by the author after 6.4 ("manca il menu contestuale?"); added to DESIGN with the
author's OK.

**Done**
- `views::ContextMenu`: a menu drawn by Nordend over the window, in the theme's colors (panel
  `surface_container_high`, highlight `hover` / `on_hover` like Noctalia's menus), items with the
  shortcut at the right, separators, one level of submenus. Moved to stay whole: it opens to the
  left of the point near the right edge, a submenu to the left of its menu, both pushed up near
  the bottom. Keys: Up/Down over what can be chosen (wrapping), Home/End, Enter, Right into a
  submenu, Left and Escape out of it, Escape closes; every key is the menu's while it is open.
  Pointer: hovering highlights and opens a submenu, a click runs an item, a click outside only
  closes it.
- Right click (and two-finger tap, which libinput sends as the right button) in the three views:
  on an entry, selected first unless it already was (a right click inside a selection keeps it);
  on empty space, the selection cleared and the folder's own menu; on the info pane, the menu of
  what it shows. Menu and Shift+F10 open it from the keyboard, under the cursor's row or cell
  (`cursor_anchor()` in each view).
- On entries: Open (as Enter), Open With ▸, Copy, Cut, Paste, Rename… (one entry), Move to
  Trash. On a folder's empty space: New Folder, New File ▸, Paste. Paste is dimmed with nothing
  taken, Rename with several entries, Open With is left out for a folder. The keys shown come
  from the shortcuts table (`keys_for(Command)`).
- Open With: `Applications::all_for(type, parents)`, GLib's `g_app_info_get_all_for_type` /
  `_recommended_for_type` (read in `gdesktopappinfo.c` 2.88.3: added associations and cache
  entries dir by dir, removed ones blocked from then on, parents after). The menu lists the
  default first ("Darktable (default)"), then the rest once each; the chosen application opens
  every selected file (`plan_open(..., app_id)`, `Opener::open(..., app_id)`), folders left out.
  Found on a worker (`Opener::choices`); the submenu says "Looking for applications…" until then,
  "No applications" when there are none.
- New File: "Empty File" makes "New File" ("New File 2"...), a template makes a copy under its
  name ("Letter.txt", "Letter 2.txt"), with its permissions, created with `O_EXCL` (never over
  anything), removed if the copy fails; the name is edited at once, as for a new folder. The
  templates are the visible regular files of `XDG_TEMPLATES_DIR` (user-dirs.dirs), else
  `~/Templates` when there is no user-dirs.dirs (as the sidebar's user dirs); backups `~` left
  out. Looked for on a worker every time the menu opens.
- Empty files open as their name says (asked for after the author's test, where an "Empty
  File" renamed `Lettera.txt` answered "No application opens application/x-zerosize"):
  `MimeDatabase::to_open(mime, name)` turns `application/x-zerosize` (GLib's type for any empty
  file) into the name's type when it has one; opening, Open With and `nordend-open --dry-run`
  use it, and so do icons (which already followed the name). The type shown in the info pane is
  still GLib's. An empty file without a known extension still has no application. Nautilus does
  something similar for empty files: *to be verified*.
- `nordend-open --apps TYPE...` prints both lists; `nordend-snapshot` gets an Opener (lists only,
  nothing launched) so "Open With" can be checked offscreen.
- Tests (187 with the raw samples): the menu's keyboard (skipping, wrapping, submenus, a submenu
  filled while the keyboard waits in it, Escape twice, a click outside); the items for one entry,
  several, a folder, empty space, before and after Copy; Open With choosing an application and
  opening only the files; Move to Trash and Rename from the menu; New File empty and from a
  template twice (permissions kept); `make_file` numbering and failure; `templates_in`,
  `templates_dir`; `all_for` and `choices_for` against a fake system (added, cached, removed,
  parents, not installed).
- Slip found on the way, and fixed: `Environment::from_environment()` (6.1) looped over views
  into a temporary string (`split(env("XDG_CURRENT_DESKTOP"), ':')`): use after scope whenever
  `XDG_CURRENT_DESKTOP` is set (here: `umbriel`), so the desktop-specific `umbriel-mimeapps.list`
  could be read under a garbled name. ASan caught it in the new snapshot. No other loop does
  this.

**Tested by the author**: on screen, with a temporary config holding `XDG_TEMPLATES_DIR`. The
menu in the three views, Open With opening Darktable and Helix, New File from the template twice
("Lettera 2.txt", "Lettera 3.txt", both "Caro,"), an empty `Lettera.txt` opening in Helix after
the fix. The first try used empty test files (`touch`): GLib's `application/x-zerosize`, no
application, which led to the fix above. The theme did not follow Noctalia during the test only
because the temporary config had a copy of `colors.toml`; without it, "theme: reloaded".

**Measured** (see benchmarks): `nordend-open --apps` against `gio mime` on all 1 232 types and
aliases: 0 differences in both lists ("Registered" and "Recommended"); 564 types have
applications, 380 of them different lists with and without parents.

**Decisions**
- In the window, not an `xdg_popup`: simpler and like the conflict card; it cannot go past the
  window's edges. A real popup can come later.
- Activation on press, not release: the press that opens the menu never runs an item.

**Open**
- Snapshot keys reach the menu before the templates are found, so `--keys` cannot choose a
  template (the tests can).
- No icons in the menu (applications' icons would need their icon themes).
- Open With offers the cursor's file's applications even for a mixed selection.

---

## 2026-10-29 — Trash (ROADMAP 6.4)

**Done**
- `fs/trash`: the freedesktop Trash specification 1.0 (read from its source, `xdg-specs`
  `trash/index.rst`), with GLib's `g_local_file_trash` (2.88, `glocalfile.c`) as the reference
  for the details the spec leaves open.
  - Which trash: the home trash (`$XDG_DATA_HOME/Trash`, else `~/.local/share/Trash`, made 0700
    when missing) when the folder is on its device; else the top directory (`mount_top`: the
    highest ancestor on the same device) holds it: `$topdir/.Trash/$uid` when `.Trash` is a real
    folder with the sticky bit, else `$topdir/.Trash-$uid`, made when missing; either must be a
    folder owned by the user. Never copied between disks: no trash there, the item stays and the
    status bar says why.
  - Each item: `info/NAME.trashinfo` created with `O_EXCL` and written whole, then a rename into
    `files/` without replacing; a failed rename removes the info. Names as GLib numbers them
    (`P1.ORF`, `P1.2.ORF`; `Trip.2`); a name too long for `.trashinfo` is shortened from its
    front at a character boundary. `Path=` absolute in the home trash, relative to the top
    directory elsewhere, escaped like `g_uri_escape_string(path, "/")`; `DeletionDate` in local
    time.
  - Folders go to `directorysizes` (`du -B1`, a hard link counted once; the info file's mtime),
    rewritten through a temporary and a rename; lines of folders no longer in `files/` are
    dropped. gio does not keep this file; KDE reads it.
  - Not trashed again: what is in the trash (refused), the folder that holds it (an error).
  - Darktable sidecars go with their photo, each with its own `.trashinfo` (restored one by one
    by any trash program).
- `TransferKind::trash`: the transfer queue runs `run_trash` for it, so Delete waits behind a
  copy and shows its progress the same way ("Moving 3 of 120 items to the trash").
- Delete (and keypad Delete) in the views and Quick Look: the selection, or the cursor's entry.
  The cursor moves at once to the first entry after the last one trashed that stays (else the
  one before). Messages: "“P2.ORF” with 1 darktable sidecar moved to the trash", "3 items moved
  to the trash; 1 could not be: ...".
- `nordend-transfer --trash SOURCE_DIR NAME...` trashes from the command line (the real trash).
- Tests (179 with the raw samples): escaping, numbering, `.trashinfo`; the home trash with
  sidecars, a folder with a hard link (size checked against `du -B1`), the same names twice, a
  stale file in `files/`; a read-only folder and a missing name (nothing lost, no info left);
  the trash not trashed; the top directory's trash (`.Trash-uid`, `.Trash` without sticky bit or
  as a link refused, `.Trash/uid` with it; paths relative); `mount_top`; Delete in the browser,
  the cursor moving on and back.
- Slip, fixed before anything else: the shortcuts test presses every listed key, so Delete
  trashed its temporary files into a real `/tmp/.Trash-1000` (tmpfs is not the home's disk).
  The fixture now uses a trash inside its temporary folder and makes its files again; the stray
  trash was removed. The author's home trash was not touched.

**Measured** (see benchmarks): 1 001 items in 24-25 ms, `gio trash` 69-71 ms. The
`.trashinfo` files match gio's for awkward names.

**Decisions**
- No permanent delete (Shift+Delete) and no trash view in the sidebar in this step: the first
  needs its own confirmation card, the second (with restore) is a step of its own.
- The trash goes through the transfer queue rather than a worker: trashing a folder on a slow
  disk is quick, but its `du` can take a while, and the order with a copy running matters.

**Tested by the author**: on screen, in `~/nordend-trash-test` and on DATA. `trash-list`
(trash-cli, installed for this) lists the five items Nordend trashed, the home ones with their
absolute paths and `x.txt` from DATA's `.Trash-1000` (relative `Path=`); `trash-restore` put
`Viaggio` back in its folder. The read-only folder's file stayed. `directorysizes` gave
`Viaggio` 0 bytes: right, on btrfs a folder and an empty file take no blocks (`du -B1` says 0
too).

**Open**
- No check that a folder's contents can be deleted later (GLib's `check_removing_recursively`):
  a folder with a read-only subfolder goes to the trash and then needs `chmod` to be emptied.
- Bind mounts of one file system: the top directory is found by device, so a rename across them
  fails (EXDEV) and is reported, as in GLib.

---

## 2026-10-29 — Giving decoded photos back (ROADMAP 6.3c)

**Done**
- `core::give_back_large_blocks()`: `mallopt(M_MMAP_THRESHOLD, 16 MB)`, first thing in `main()`
  (and in `nordend-snapshot`, which has `--default-malloc` to compare). Blocks of 16 MB or more
  get mappings of their own and go back to the system when freed; glibc's dynamic threshold
  would otherwise grow to the decode buffers' size and serve them from the workers' arenas.
- Quick Look drops the fitted pixels once they are a texture (the size stays for the caption).
- The memory report also gives the peak (`VmHWM`).
- Tests (173 with the raw samples): eight 20 MB blocks allocated and freed leave nothing kept
  (checked to fail without the call: 21 MB kept); skipped under ASan and TSan, whose malloc
  replaces glibc's. Quick Look's pixels need GL to be uploaded, so that one is checked with
  `nordend-snapshot --memory` ("pixels kept 0.0 MB"), not a unit test.

**Measured** (see benchmarks): offscreen, a screenful of new thumbnails 205-215 ms instead of
188-201 ms (+7 %), 152 MB afterwards instead of 386 MB; Quick Look over 20 photos 155 MB
instead of 584 MB. Limiting the thumbnails made at once (4 or 2) lowers the peak but costs 20 %
or twice the time: not kept. The thumbnail texture budget (256 MB, in VRAM) stays.

**Tested by the author**: the five steps on screen (`giro.sh dopo`): 111, 120, 141, 143 and
143 MB back home, instead of 545 MB; peak 240 MB. Thumbnail textures reached 120 MB of VRAM
(257 thumbnails, grid and info pane sizes), kept under the budget.

---

## 2026-10-29 — Memory with photos (ROADMAP 6.3b)

Asked for after `top` showed 290 MB (6.2) and 412 MB (6.3) resident. Measure first; fixes are
their own step, with the author's OK.

**Done**
- `core::read_memory_report` / `describe` (`core/memory`): `smaps_rollup` (RSS, anonymous,
  private, shared), `smaps` grouped by mapping (`[heap]`, `[anon]`, libraries, fonts, DRM),
  `mallinfo2` (in use, freed but kept, arenas, mmapped), the DRM fdinfo (`drm-memory-vram`,
  `-gtt`, `-cpu`, one count per client id).
- Nordend's own counts: `Thumbnails::usage()` (slots, textures, bytes, pixels waiting),
  `QuickLook::usage()` (slots, texture bytes, pixels kept), `Renderer::atlas_bytes()`,
  `Browser::memory_lines()` (folders, entries).
- The app logs the report on SIGUSR1 (the signalfd now reads which signal came); the snapshot
  tool prints it with `--memory`, and `--trim` calls `malloc_trim(0)` first and times it.
- A test reads the report around a 64 MB block. 172 test cases with the raw samples.
- A script for the author's run (the first try had no reports: the `pkill` in a second terminal
  was forgotten): it starts Nordend with a fresh cache, prompts each step and sends the signal
  itself, checking the report reached the log.

**Measured** (see benchmarks, with all the numbers)
- On screen, the author's five steps: home 111 MB, `vero` 130 MB, the grid with every thumbnail
  526 MB, after Quick Look 545 MB, **back home still 545 MB**. Of it, 430 MB is memory malloc has
  been given back and keeps: only 21 MB are in use.
- heaptrack: the kept memory is the thumbnails' decode buffers (the ORF's embedded JPEG decoded
  whole by wuffs: about 30 MB of pixels and 15 MB of work buffer each, eight workers at once).
  Freed at once; glibc's dynamic mmap threshold grows to their size, so they are served from the
  workers' arenas, which glibc does not give back.
- Not the cause: thumbnail textures (14-89 MB, and in VRAM), the 3 GB copy (11.6 MB peak),
  libraries (89 MB, shared), GPU memory (separate from the RSS).
- With the mmap threshold fixed at 16 MB (`GLIBC_TUNABLES`): the same steps stay at 111, 120, 139,
  139, 140 MB, a 405 MB saving; offscreen the grid's first screenful of new thumbnails takes
  199-201 ms instead of 183-189 ms.

**Tested by the author**: the five steps on screen, both runs, through the script (the numbers
above); the step ticked on the author's OK.

**Proposed** (a step of its own, after the author's OK)
1. `mallopt(M_MMAP_THRESHOLD, 16 MB)` at startup (the app and the tools): -405 MB with photos,
   +12 ms on a screenful of new thumbnails; 1 MB saves a little more (138 MB) for +25 ms.
2. The transient peak while thumbnails are made is still eight decodes at once (about 370 MB for
   a moment): fewer concurrent full-size decodes, or decoding the preview scaled down
   (libjpeg-turbo's DCT scaling, which would also be faster), *to be measured*.
3. Quick Look keeps the fitted pixels after uploading them (10.5 MB for three slots): drop them.
4. Thumbnail textures stay on the GPU after leaving a folder, up to the 256 MB budget (89 MB
   seen): a smaller budget, or dropping a folder's thumbnails when it is left, *to be measured*.

---

## 2026-10-29 — Copy and move (ROADMAP 6.3)

**Done**
- `fs::run_transfer` (`fs/transfer`), the engine, blocking:
  - a move on one file system is `renameat2(RENAME_NOREPLACE)`, instant (falls back to copying
    on EXDEV, a mount point below the source);
  - a copy, or a move between file systems, copies data with `copy_file_range` in 8 MB pieces
    (read/write when the kernel refuses across file systems) into a hidden
    `.nordend-part-<pid>-<n>` next to the target, renamed to its name only when complete; a new
    folder is built the same way, as a whole. Cancelled or failed: the temporary is removed, so
    nothing half-written ever carries a real name;
  - between file systems the copy is `fsync`ed (and its folder) before the source is deleted; a
    folder's source goes only if all of it arrived;
  - permissions, access and modification times (nanoseconds) and symbolic links are kept;
    special files are reported, not copied;
  - darktable sidecars travel with their photo (`sidecars_of`, now split out of
    `sidecar_renames` with `sidecar_name`), named after the photo's final name; a sidecar picked
    with its photo goes with it once; a taken sidecar name is a conflict on the photo;
  - conflicts go to a `resolve` callback: replace (a file replaced only once the new one is
    complete, by an atomic rename over it; folders merged, same names replaced), skip, keep both
    (`free_name`: "P1 2.ORF" with "P1 2.ORF.xmp", "Trip 2"), cancel. A file never replaces a
    folder nor the reverse. A copy into its own folder keeps both by itself; a move into its own
    folder does nothing;
  - refused before anything moves: a folder into itself or below ("Cannot copy Trip into
    itself"), or more bytes than the destination's free space ("Not enough space: 3.4 GB
    needed, 1.2 GB free").
- `fs::TransferQueue`: one thread, one transfer at a time in order. Progress reaches the loop at
  most every 100 ms; a conflict is posted as a question and the thread waits for the answer
  (cancel and destruction answer it); the result is posted. Callbacks check an `alive` flag.
- `Browser`: Ctrl+C / Ctrl+X take the selection (or the cursor's entry); cut entries fade in all
  three views (`CutMarks`, `ItemState::cut`, thumbnails drawn at 45 %); Escape keeps them where
  they are. Ctrl+V submits a copy or a move into the folder shown (a cut is pasted once). The
  status bar shows "Copying 163 files · 2.1 GB of 3.4 GB · 180 MB/s · 8 s left" (rate measured
  from the first byte, restarted after a question), a 2 px bar along its top and an × that
  stops the transfer; the result is one line ("Copied 342 files (3.1 GB)", "Stopped: 40 files
  copied so far", "Copied 161 files; 2 could not be: ..."), and what arrived is selected when
  its folder is shown (`Folder::expect_names`). `loading()` counts transfers.
- `views::ConflictDialog`: a card ("“P1.ORF” is already in album"), a note on what Replace does
  (or that it cannot, file against folder), "Do the same for the rest" (A), Skip (S), Replace
  (R), Keep both (Enter), Escape stops. It takes every key and click while open.
- Shortcuts: Ctrl+C, Ctrl+X, Ctrl+V, and the card's keys under a new `kConflict` scope (a test
  presses each on a real conflict). The overlay's columns were rebalanced (Grid moved left).
- Tools: `nordend-transfer [--move] [--force-copy] SRC DEST [NAME...]` (the engine, timed);
  `nordend-snapshot --transfer TEXT` and `--conflict NAME` to look at the bar and the card.
- Tests (171 with the raw samples, 13 new): copy (contents, nanosecond times, permissions,
  links, recursive folders, sidecars, progress totals), move on one disk (same inode), move
  between disks forced (new inode, sources gone), each resolution, sidecar names on keep both,
  folder merge, file against folder, own-folder copy, into itself, not enough space (a 1 PiB
  sparse file), cancel after the first piece (no temporary left, source whole), a read-only
  destination, the queue (question answered later, destroyed while asking), and in the browser:
  copy and paste with selection, cut and Escape, move once, each answer, "for the rest", stop,
  every listed key. Release, ASan, TSan pass (transfer and paste ones 6 times in Release and
  TSan); snapshots under ASan and TSan; tidy and format clean.

**Measured** (see benchmarks): the author's `vero` folder (342 files, 3.1 GB) from the SATA SSD
into `/tmp` (tmpfs), page cache warm, three runs each: Nordend 1.38-1.39 s (about 2.2 GB/s),
`cp -a` 1.38-1.40 s. The same speed.

**Tested by the author**
- On screen: the 342-file copy of `vero` with progress, rate and time left (`diff -r` equal,
  dates kept), a cancelled copy leaving no temporary, the conflict card (skip, replace, keep both
  for the rest, Escape), a cut fading and Escape, a move on one disk and one onto the DATA disk,
  the three refusals (into itself, not enough space, read-only), F1, `top` at 0.0 % CPU
  afterwards: "il resto funziona tutto".
- `top` showed 412 MB resident after the 3 GB copy and browsing the photos: memory with photos is
  still *to be measured* (open since 5.8; next, before 6.4, is the proposal).

**Open**
- Cold-cache throughput (after `drop_caches`, the author's `sudo`) and a real move between disks
  (an SD card or a USB disk) are for the author's test.
- No drag and drop yet: proposed as a next step on the same engine.
- The temporary is visible (dimmed, hidden name) in the destination while a file is copied.
- Merging folders replaces same-named files without asking again inside them.
- No key stops a transfer (the × only); copying files between Nordend and other apps is 6.5.

---

## 2026-10-29 — New folder and rename (ROADMAP 6.2)

**Done**
- `fs/operations`: `name_problem` (empty, `.`/`..`, `/`, NUL, more than 255 bytes),
  `make_folder` ("New Folder", else "New Folder 2"...: `mkdir` itself decides, the next name on
  EEXIST, so two makers never collide), `rename_entry` (`renameat2(RENAME_NOREPLACE)`, with a
  check-then-rename fallback only on file systems that answer EINVAL/ENOSYS) and
  `sidecar_renames`: a regular file takes its darktable sidecars along, `X.ORF.xmp` and the
  duplicates `X_NN.ORF.xmp` (two digits or more), unless a file has the duplicate's name
  (`X_1234.ORF` next to `X.ORF`: that sidecar is its own). If a sidecar's new name is taken,
  nothing is renamed; if one fails after the photo moved, the message says which.
- `ui::TextField`: one line of UTF-8 text with a caret and a selection; typing (dead keys and
  compose arrive as `KeyEvent::text`), Backspace, Delete, arrows, Home/End (Up/Down too), Shift
  to select, Ctrl to go by words (spaces, dots, dashes, underscores end them), Ctrl+A; a click
  places the caret, Shift+click extends, a double click selects all. Scrolls sideways to keep
  the caret in sight. The caret does not blink, so an open field costs no frames.
- `views::NameEditor` + `EditSlot`: the browser names the entry; the view showing it (column
  rows, grid cells, details rows) leaves the name out and notes where it would be, and the
  editor draws the field there after the view. The name is selected without its extension (a
  folder's whole name). Enter confirms, Escape cancels, a press outside confirms and then does
  what it does. A bad name is refused at once; a good one goes to a worker and the field waits
  (border dimmed, keys swallowed): on success it closes, on failure ("P1.ORF already exists",
  "Permission denied") it stays for another try. While it is open every key is the field's.
- `Browser`: F2 (`start_rename`) and Ctrl+Shift+N (`new_folder`), both closing Quick Look and
  doing nothing in the sidebar (generalised: in the sidebar a command not listed for it is
  nothing, as Space already was). `loading()` also counts new folders and renames on their way.
  A field whose entry disappears (another program) closes.
- `Folder::expect_name`: the cursor waits for an entry Nordend just made (a plain `select_name`
  gives up at the next change). After a rename the cursor follows by itself (the watcher reports
  renames), and a new folder renamed before the watcher saw it moves the wait to the new name:
  found by a test in the columns, where typing is faster than inotify's settle.
- Shortcuts: a "Files" group (F2, Ctrl+Shift+N, and the field's Enter/Escape and Ctrl+arrows under
  a new `kEditor` scope). The overlay listed its groups by hand and left "Files" out (seen in a
  snapshot): `overlay_groups()` now feeds the test, which fails on a group the overlay does not
  show. The "nothing else does anything" sweep now includes Ctrl+Shift.
- `nordend-snapshot --keys`: a single letter types itself ("F2,L,a,k,e").
- Tests (158 with the raw samples, 11 new): names, new folder names, sidecar matching (decoys
  included), renames that must not replace (the photo or a sidecar), folders without sidecars,
  a read-only folder, the text field's keys and UTF-8, F2 in all three views with sidecars,
  refused and failed names, Escape, Ctrl+Shift+N in all three views, F2 without a cursor and in
  the sidebar, the editor's shortcuts. Release, ASan, TSan pass (the browser ones 6 times in
  Release and TSan); snapshots under ASan and TSan; tidy and format clean.

**Measured**: scrolling 10 000 entries (600 frames at 2880x1620, three runs) is unchanged:
details 0.37-0.50 ms, grid 0.30-0.32 ms, columns 0.25-0.27 ms median; details with a field open
0.50 ms.

**Tested by the author**
- On screen: F2 with the extension left out, accents typed, an ORF renamed with its two
  sidecars, a taken name and a `/` refused with the field kept, Escape, Ctrl+Shift+N in the three
  views, a click outside, clicks in the field, a read-only folder's messages, F1's "Files", and
  `top` at 0.0 % CPU with the field open: "funziona tutto perfetto".
- `top` showed 290 MB resident in a folder with an ORF (117 MB measured on 2026-10-28 in a source
  folder): previews and thumbnails held as textures, likely; memory with photos is still *to be
  measured* (an open point since 5.8).

**Open**
- Renaming only the case of a name on a case-insensitive file system (an exFAT SD card) is
  refused as "already exists": `RENAME_NOREPLACE` sees the same entry. A two-step rename would do
  it; not done yet.
- Paste into the field comes with the clipboard (6.5); no undo (after the MVP's operation queue).
- The pointer does not turn into a text cursor over the field.
- Caret moves by code points, not grapheme clusters (an emoji with a modifier takes two steps).

---

## 2026-10-29 — Open with the default application (ROADMAP 6.1)

**Decision** (with the author, DESIGN "Apertura dei file"): no `xdg-open`, no `gio open`; Nordend
reads the two freedesktop specs itself. Measured first on this machine (xdg-utils 1.2.1, generic
mode under Umbriel): `xdg-open` waits for the application to quit (it runs it as a child of its
shell), ignores `Terminal=true` (Helix, the default for text/plain, would start without a
terminal and fail silently) and takes the type from `file`, which differs from ours (`.xmp`:
`text/xml` against `application/xml`).

**Done**
- New module `platform/`:
  - `desktop_entry`: the [Desktop Entry] keys Nordend needs, string escapes, lists, the localized
    `Name` by the spec's locale matching; `Environment` (XDG dirs, desktops, locale, PATH,
    `$TERMINAL`, read once at startup); `find_program`.
  - `exec`: `split_exec` (the spec's double quotes, plus single quotes and backslashes as GLib's
    shell parser reads them), `expand_exec` (`%f %F %u %U %i %c %k %%`, deprecated codes dropped,
    a list code gives one process, a single one a process per file, no code appends the file as
    GLib does), `in_terminal` (`$TERMINAL`, `xdg-terminal-exec`, then foot, ghostty, kitty,
    alacritty, wezterm, konsole, gnome-terminal, xterm: forms of the ones not installed here *to
    be verified*).
  - `applications`: GLib's order: per type, then its parents (aliases resolved, subclasses
    breadth first), the defaults of every dir, then added associations and cached entries minus
    removed ones; the first installed wins (not Hidden, an Application, `TryExec` and the `Exec`
    program in the PATH; the highest copy of an ID decides and hides the lower ones).
  - `launcher`: `pidfd_spawnp` (glibc 2.39) with `POSIX_SPAWN_SETSID`, an empty signal mask and
    default dispositions (Nordend blocks SIGINT/SIGTERM for its signalfd: without this every
    application it starts would inherit them blocked), stdio on `/dev/null`,
    `addclosefrom_np(3)`, `addchdir_np`. The pidfd sits in the event loop; the child is reaped
    there (`waitid(P_PIDFD)`): no thread, no zombie, children survive Nordend.
  - `opener`: types (the view's, else detected), applications and command lines on a worker
    (`plan_open`, pure), processes on the loop thread, each with its own activation token; one
    line reported.
- `ui::Activation` (xdg-activation-v1, offered by Umbriel): a token tied to the serial of the last
  key or button press (`Seat::input_serial()`) and to the window's surface, passed as
  `XDG_ACTIVATION_TOKEN` and `DESKTOP_STARTUP_ID`; given up after 500 ms.
- `Browser`: Enter (and keypad Enter) on a file opens the selected files (folders in the
  selection left out; a cursor outside the selection opens only itself), with the types the view
  already knows; on a folder the view goes in as before. A double click on a file does the same in
  the three views. `Browser::set_open_handler` keeps views free of `platform`.
- `StatusBar::show_message`: a line for 4 s in place of the status ("Opening 5 files with
  Darktable…" in the text color, problems in the error color): no application for a type, no
  terminal, a program that cannot start, or one that quits with an error within 2 s.
- `MimeDatabase::parents()` (the subclasses file, as `g_content_type_get_parents`); duplicate
  parent lines from several mime dirs are no longer stored twice.
- `nordend-open` (dev tool): `--dry-run FILE...` prints types, applications, folders and command
  lines; `--default TYPE...`, `--types`; without options it opens for real (no token).
- `nordend-snapshot --message TEXT` (`!TEXT`: an error) to look at the status bar's message.
- Shortcuts table: Enter is now "Open (files in their app)".
- Tests (147 with the raw samples, 11 new): desktop entry parsing and locales, lists, quoting,
  field codes, the terminal order, defaults/associations/cache by priority, desktop-specific
  lists, removed associations, parent types, aliases, hiding by ID, Hidden/TryExec/missing
  program/wrong Type, IDs from subfolders, the plan (grouping, terminal, problems), the launcher
  (exit status, signal, missing program, environment, folder, empty signal mask read from
  `/proc/self/status`), the opener end to end with fake XDG dirs and scripts, and the browser's
  Enter. Release, ASan, TSan pass (timing-based ones 8 times in Release and TSan); tidy and format
  clean.

**Measured** (see benchmarks)
- Reading every desktop entry and list: 0.42-0.48 ms for 21 entries, so it is done again on every
  open (on a worker): new applications and changed defaults count at once, nothing is watched.
- Against `gio mime TYPE` (GLib 2.88.3) on every type of the shared-mime-info database and every
  alias (1 232 names, 564 with an application): 0 differences. The first run had 7; they taught
  two things about GLib, both now done the same way (and checked with gio on scratch XDG dirs):
  - the entries declaring a type come from `mimeinfo.cache`, not from the entries' `MimeType`
    keys: an entry missing from the cache is not offered, a stale cache line still is, and a
    user copy of an entry in a dir without a cache hides the system copy's associations (only
    `mimeapps.list` defaults still reach it);
  - types in the cache and in `mimeapps.list` are filed under their canonical name, merged in
    file order (`image/x-targa` and `image/x-tga` both reach `image/x-tga`; swayimg comes first
    because its alias line comes first).
- Binary: 2 254 728 bytes (1 918 968 stripped), +275 KB; 18 direct libraries, unchanged.

**Tested by the author**
- On screen: an ORF in darktable, five ORFs in one darktable, a JPEG in swayimg (double click
  too), a text file in Helix inside foot, the focus on every new window (Umbriel honours the
  token), the zip's error, Enter on a folder, F1, no zombies, swayimg left open after Ctrl+Q:
  "funziona tutto, anche il focus".
- A file swayimg cannot decode (`broken.jpg`) did not give an error: the author's swayimg config
  has `imagelist.adjacent = true`, so swayimg loads the folder, skips the broken file, shows the
  next one (an ORF) and exits 0. Right from Nordend's side: the application started and is
  running; only a quick non-zero exit is reported.

**Open**
- "Open with…" (every application for a type, choosing one) needs a menu: a later step, on the
  same `Applications`.
- The terminal cannot be set in the config yet (`$TERMINAL` can): it comes with the external
  editor's `editor.terminal` (DESIGN appendix).
- `DBusActivatable=true` applications start from their `Exec` line (none installed here).
- `Path` of an entry wins; otherwise the process starts in the first file's folder (handy for
  terminal applications; GLib keeps the caller's folder).
- Whether Umbriel gives the focus to the new window with the token: to be seen on screen.

---

## 2026-10-28 — Status bar and shortcuts (ROADMAP 5.11)

**Done**
- `views::StatusBar` under every view: at the left the folder's status (moved from the top
  bar: "12 items", "1 item", "3 of 12 selected", the error) and "824.7 GB free of 1.0 TB"
  (`statvfs` on a worker, `f_bavail`: what the user may write); at the right a button per view
  (layout-columns, layout-grid, list-details; the one shown marked) and "?" (help).
- The free space is read again when the folder or its listing changes:
  `DirectoryModel::revision()` counts the notifications, so a copy into the folder shown
  (any program) refreshes it.
- `views::shortcuts()`: one table of every shortcut (group, keys as shown, action, where it
  works, modifiers, keysyms, the browser's command if any). The browser runs its commands from
  it (Ctrl+1/2/3, Ctrl+I, Ctrl+B, F6, Space, F1/?) and passes a view or the sidebar only the
  chords listed for it (`listed()`). The views' `key()` now returns whether it used the key.
  Consequence: chords that worked by accident are gone (Alt+Left, Shift+Enter, Ctrl+Escape,
  Alt+Page Down... did the same as the plain key).
- `views::ShortcutsOverlay`: F1, "?" (Shift ignored: it needs it on most layouts) or the "?"
  button show the table on a card, two columns of groups; F1, "?", Escape or a click close it,
  and nothing under it reacts meanwhile.
- Tests (136 with the raw samples): every chord of every shortcut does something in every view
  it is listed for, in the sidebar and in Quick Look (checked to fail with a bogus Ctrl+Z
  entry); a sweep of ~70 keys with no modifier, Ctrl, Shift, Alt finds nothing that works
  without being listed; Ctrl+Q stays the window's; the overlay swallows keys; the status texts;
  the free space read once per listing and again after a file arrives. Release (6 runs of the
  new tests), ASan, TSan pass; snapshots under ASan and TSan; tidy and format clean.

**Measured**: nothing per frame (see benchmarks).

**Tested by the author**
- On screen (screenshots with Noctalia's `screenshot-window`): the status bar, the view
  buttons, the shortcuts overlay, the free space after a copy, and the breadcrumb fix below:
  "funziona tutto".

**Breadcrumb fix** (found by the author on screen, from 5.10): in the columns, a click on a step
whose column was open only focused it, and the deeper columns stayed. Now
`ColumnView::focus_folder` closes the columns after the next one and that one, the folder we
came from, shows again without a cursor (test updated).

**Open**
- Ctrl+Q is in the table but handled by the app (`main.cpp`), so the tests cannot drive it.
- The sweep leaves out Ctrl+Shift and Ctrl+Alt combinations: they are not listed, so the
  browser filters them, but no test sends them.
- Buttons have no tooltips.

---

## 2026-10-27 — A breadcrumb (ROADMAP 5.10)

Asked by the author with the status bar, the shortcuts overlay and a settings panel; agreed
order: breadcrumb (5.10), status bar and shortcuts (5.11), phase 6, then settings with the
remembered state (7.0b). System information (distro, Noctalia, Umbriel) goes to the settings'
"About", not the status bar.

**Done**
- `views::Breadcrumb`, drawn by the `Browser` above every view (the views no longer draw a
  title bar: they get the bounds under it). The folder's path as steps: the home as its icon
  when the folder is in it, else the root as the file system's icon, then the folders' names
  separated by chevrons; the last step (the folder shown) brighter. Too long: the first step,
  "…", and as many of the last steps as fit; the last one is cut in the middle when even it
  does not fit. Hover box on the steps; the status ("12 items"...) stays at the right until
  5.11 moves it.
- A click on a step: `Browser::go_to(folder, child)`. In the grid and the details the folder
  opens with the cursor on the step after it. In the columns, the column showing that folder
  is focused (its cursor is already on the way down; the columns after it stayed: fixed with
  5.11, they close); above the
  first column, the columns are revealed from it down to the step after it.
- The pointer goes to the sidebar, the breadcrumb or the view, and stays with the one that got
  a press until the release: a drag of the info pane's edge that crosses the sidebar no longer
  loses its release.
- The details table leaves out the type, then the date, then the size when the name would be
  narrower than its 160 px minimum. It overlapped before (with the info pane, under about
  700 px of view); the sidebar made that common at ordinary window widths.
- Tests (130 with the raw samples): the steps from home or root, going up in the grid, the
  details and the columns (focused column, revealed from above). Release, ASan, TSan pass;
  snapshots under ASan and TSan; tidy and format clean.

**Measured**: no measurable cost per frame (see benchmarks).

**Tested by the author**
- On screen: the columns (focusing an open column, revealing from above the first), the grid
  and the details, the home icon, a narrow window: "funziona tutto quello che dici".

**Open**
- The "…" is not clickable (the steps it hides are reachable from the first one).

---

## 2026-10-27 — The places sidebar (ROADMAP 5.9)

Asked by the author before phase 6: the classic sidebar was in neither the DESIGN nor the
ROADMAP (only udisks2, after the MVP). Added to both with their OK.

**Done**
- `fs::load_places` (`places.*`): Home; the XDG user dirs that exist, from `user-dirs.dirs`
  (Desktop, Documents, Downloads, Music, Pictures, Videos; templates and public share left
  out; without the file, the folders with those English names in the home, as on the author's
  machine, which has no `user-dirs.dirs`); the file system; GTK bookmarks
  (`~/.config/gtk-3.0/bookmarks`, local `file://` ones, percent-decoded); mounts under
  `/run/media`, `/media`, `/mnt` from `/proc/self/mountinfo` (octal escapes decoded). Labels are
  the folders' names, so localized user dirs show as they are on disk.
- `fs::PlacesWatcher`: reloads when the mount table signals POLLPRI (in the one `poll()`) and
  when `user-dirs.dirs` or the bookmarks change (`core::FileWatcher`, only for folders that
  exist, since it creates missing ones); tells the browser only when the list differs.
  `/proc` files report size 0, so the mount table is read in chunks, not by
  `core::read_text_file`.
- `views::Sidebar`: sections PLACES / BOOKMARKS / DEVICES, the place holding the current folder
  marked (longest matching path, whole names: Pictures before Home before the file system),
  hover, a click opens the place in the view shown (`Browser::open`), scrolls when taller than
  the window, right edge draggable (140-400 px, default 200, double click restores), left out
  when the view would keep less than 480 px.
- `Browser`: draws it at the left, the view in the rest; routes the pointer (leave sent to
  whichever loses it; a drag stays with the sidebar). Ctrl+B shows/hides it. F6 moves the
  keyboard in (the cursor starts on the current place) and out; inside, Up/Down/Home/End move,
  Enter opens and goes back to the view, Escape goes back; a click in the view also takes the
  keyboard back. Ctrl+1/2/3, Ctrl+I, Ctrl+B work from the sidebar too.
- Keyboard: the proposal said "Ctrl+B and arrows"; Ctrl+B both showing and entering was
  ambiguous, so entering is F6 (as in GTK file managers).
- `nordend-snapshot` lists the places of `$HOME`/`$XDG_CONFIG_HOME`; "Ctrl+B" and "F6" in
  `--keys`.
- Tests (127 with the raw samples): parsing user dirs, bookmarks and mounts; loading with and
  without `user-dirs.dirs`; the watcher following a bookmarks change; every place icon in the
  font; `place_holding`; F6/Down/Enter/Escape/Ctrl+B through the browser. Release (8 runs of
  the places tests), ASan, TSan pass; snapshots under ASan and TSan; tidy and format clean.

**Measured** (see benchmarks): the sidebar costs 0.03-0.06 ms per frame; the mount table's
POLLPRI fires once per mount or unmount and never while idle.

**Tested by the author**
- On screen: the three views, Ctrl+B, F6, dragging the edge. A loop-mounted ext4 image under
  `/mnt` appeared under DEVICES and went away on `umount` without a restart; so did an NTFS
  disk mounted by hand (ntfs3, read-only) under `/run/media`.
- Idle CPU with the window open: 0 ticks in 30 s (`/proc/PID/stat` utime+stime).
- Found while testing: a folder with a localized name ("Scaricati") does not show without
  `user-dirs.dirs` (only the English names are guessed): the file has to name it.

**Open**
- Mounting, unmounting and ejecting: udisks2 (ROADMAP 11). Mounts show the "database" icon,
  removable or not (telling them apart needs sysfs or udisks).
- Adding and removing bookmarks from Nordend (Ctrl+D): a possible follow-up.
- While the keyboard is in the sidebar, the view's selection still looks focused.
- The width and the shown/hidden state are not remembered across runs.
- The home is not watched: a standard-named folder created while Nordend runs shows after a
  restart (or a change to `user-dirs.dirs`).

---

## 2026-10-27 — The info pane's width (5.8, follow-up)

Asked by the author after testing 5.8.

**Done**
- In the grid and the details, the line between the view and the info pane can be dragged:
  the pointer turns into ew-resize over it (4 px either side), the line turns primary while
  grabbed, a double click brings the pane back to 300 px (as for the columns). The width is
  `InfoPane`'s, so both views share it; it stays within 240-720 px, and the view keeps at least
  320 px (`InfoPane::width_in`: the pane narrows, then is left out). In the grid the cells per
  row change while dragging and the cursor's row stays where it is (`keep_place`).
- The preview follows the pane: as wide as the pane (at most half its height), so a wider pane
  shows a larger photo; the rows keep to their centered 360 px column.
- `nordend-snapshot --info-width W`. Test (118 with the raw samples): the width's limits and
  `width_in`. Release (5 runs), ASan, TSan pass; snapshots with a 600 px pane walking an ORF
  folder in the grid and the details under ASan and TSan; tidy and format clean.

**Tested by the author**
- On screen, in the grid and the details: dragging, limits, double click, narrow windows:
  "perfetto, funziona tutto". With this, phase 5 is complete.

**Open**
- The columns' pane is not draggable: it is their last column and takes what is left.
- The width is not remembered across runs.

---

## 2026-10-27 — A preview in the info pane (ROADMAP 5.8)

Asked by the author: Ctrl+I only where the pane is optional, a preview in it, the content
centered and structured, more information behind a "more details" control.

**Done**
- `views::InfoPane` (public now, owned by the `Browser`, shared by the three views): the
  content is one centered column (at most 360 px) with, from the top: the preview (a photo's
  or raw file's thumbnail in a square reserved at once, so nothing moves when it arrives; a
  text file's first 14 lines on a small page; else the icon), name and type, then sections:
  INFORMATION (size or items, pixel dimensions, created, modified), PHOTO (camera, lens,
  exposure "1/2000 s · F5.6 · ISO 200", focal length, date taken, the darktable rating), and
  "More details" (a click) for MORE: permissions, owner, location, link target, and the
  exposure program, compensation, metering, white balance, flash, software. Nothing selected:
  the folder itself; several: how many and how big. It scrolls (wheel, touchpad) when taller
  than the window. What the entry says shows at once; the rest is read on a worker
  (`preview::summarize`), the thumbnail comes from the grid's `Thumbnails` store at the size
  the square needs (xx-large at scale 2).
- `image::read_photo_info`: EXIF as exiv2 prints it, size upright, the maker left out when
  the model names it or the maker is a long name ("OM-1", "Canon EOS 5DS", "SONY ILCE-7M2"),
  and `Xmp.xmp.Rating` from `FILE.xmp` (darktable; -1 is "Rejected"). Checked on the 22 raw
  samples and the author's OM-1 ORF, JPEG and WebP: 0.1-7.6 ms each.
- `preview::Info` has the birth time (statx); `exiv2` is set up once for both its users.
- Ctrl+I acts on the grid and the details only; the columns always show the pane, their last
  column.
- `nordend-snapshot --info-expanded` and `--walk N` (the cursor walks down a frame at a time,
  workers' results delivered between frames; only frames are timed).
- Tests (117 with the raw samples): the shot read from a new fixture `shot.jpg` (written by
  `make.sh` with exiftool, with a sidecar rated 3), a plain PNG, a missing file; the summary of
  a photo, a long text file (40 lines kept) and a folder; Ctrl+I leaves the columns alone.
  Release (5 runs), ASan, TSan pass; `nordend-snapshot` walking an ORF folder with the pane
  expanded in all three views under ASan and TSan; tidy and format clean.

**Measured** (docs/benchmarks.md), the "Fatto quando": walking a folder of 163 ORF one entry
per frame at 2880x1620 with the pane: 0.41-0.61 ms median, 0.62-1.39 ms p95, 3.7 ms at most
(thumbnail uploads) per frame, cold and warm cache. Once the cursor stops, the shot appears in
about 4 ms and the thumbnail in about 107 ms the first time (xx-large made from the 3200x2400
preview), 10 ms when cached.

**Decisions**
- One pane, the same everywhere: only its place changes with the view.
- Types are still MIME names; dimensions of Panasonic raw files are the sensor's (GX8:
  5264x3904), as exiv2 reports them.

**Tested by the author**
- On screen, walking ORF folders in the three views, "More details", Ctrl+I: "molto molto
  bene, perfetto, funziona tutto". Step 5.8 closed. Asked next: a draggable divider to widen
  the pane in the grid and the details.

**Open**
- The first thumbnail at pane size takes about 0.1 s per ORF: the grid's smaller one could stand
  in meanwhile, as in Quick Look.
- Fuji RAF dimensions are not reported; some exposure times print as decimals ("0.769231 s").
- Human-readable type names, a hand cursor over "More details", remembering it across runs.

---

## 2026-10-27 — Info pane in every view (ROADMAP 5.7)

Asked by the author after testing 5.6; shortcut and behaviour agreed before starting.

**Done**
- The columns' info pane is now `draw_info_pane` (`src/views/src/info_pane.*`), shared: the
  columns draw it after their last column as before, the grid and the details at their right,
  300 px wide, the view narrower by as much. Besides a file and a multiple selection, it shows a
  folder under the cursor (icon, type, date) and, with nothing selected, the folder itself
  ("Items"). It is left out when the view would keep less than 320 px.
- Ctrl+I shows and hides it, one setting for the three views (`Browser::set_info_visible`),
  shown at start. In the grid the cells per row change: the zoom's anchor (now `keep_place`)
  keeps the cursor's row where it was on screen.
- Tests (115 with the raw samples): Ctrl+I toggles it in every view and through view switches,
  cursor and selection untouched, Caps Lock's keysym included. Release (5 runs), ASan, TSan
  pass; `nordend-snapshot` toggling the pane in grid, details and columns under ASan and TSan;
  tidy and format clean.

**Measured** (docs/benchmarks.md): scrolling the 10 030-entry folder at 2880x1620 with the pane
open or closed: details 0.33-0.46 ms median (it varies between runs either way), 0.50-0.54 ms
p95; grid 0.28-0.30 ms median, 0.46-0.48 ms p95. Checked on screen offscreen: hiding the pane in
a grid scrolled to entry 5 000 keeps the cursor's row in place (5 → 7 cells per row).

**Tested by the author**
- On screen, in the three views, over photo folders: "funziona tutto". Step 5.7 closed.

**Open**
- The pane could show the thumbnail of an image; the setting is not remembered across runs.

---

## 2026-10-27 — Details view (ROADMAP 5.6)

**Done**
- `views::DetailsView` (Ctrl+3): a row per entry with name (icon, name cut in the middle), size
  (right-aligned, "—" for folders), date and type, under a header. A click on a title sorts by
  it (a chevron shows the direction), a second click reverses; dragging a column's left edge
  resizes it (the pointer becomes ew-resize, the name takes what is left, every column keeps a
  minimum). Keys and mouse as in the grid, with Right/Left opening the folder / the parent as
  in the columns. Selection, hover, scrolling with inertia, Quick Look over it: as elsewhere.
- `Browser` has three modes and switches between any two (Ctrl+1/2/3) handing over the same
  `Folder`: cursor, selection and sniffed types stay.
- Sorting: `fs::SortKey::type` (by type name, types the name does not tell last, then the
  name) and `Folder::set_order` (cursor and selection follow their entries by name, the view
  jumps to the cursor). By size, directories keep their name order: their own size is their
  listing's (found by a test: on tmpfs it depends on the content).
- `nordend-snapshot --sort FIELDS` clicks titles offscreen ("modified,modified": newest first);
  `--bench` scrolls the details.
- Tests (114 with the raw samples): switching through all three views keeps the folder, cursor
  and selection; sorting by size, reversed, by type; the cursor stays on its entry; a folder
  opened in the details is sorted the same way, and the columns show it so; Right/Enter and
  Left/Backspace; type order with unknown types last; directories by size. Release (5 runs),
  ASan, TSan pass; `nordend-snapshot` over the details (sorted, benchmark, Quick Look, switching
  views) under ASan and TSan; tidy and format clean.

**Measured** (docs/benchmarks.md), the "Fatto quando": a 10 030-entry folder (sparse files of
random sizes and dates) sorts by size in 1.1 ms, by date in 1.1-2.0 ms, by type in 3.2 ms; it
scrolls at 0.31-0.45 ms per frame (median), 0.53 ms p95, at 1440x810 and 2880x1620.

**Decisions**
- The order belongs to the details view and is applied to each folder it shows; a folder keeps
  it in the other views (the columns show the same folder sorted the same way). The columns and
  the grid have no sort control yet.
- Types are shown as MIME names ("image/x-olympus-orf"): readable descriptions need
  shared-mime-info's comments, which the database does not load yet.
- By type, the type the name tells is used (sniffed types arrive later and would make rows
  jump).

**Tested by the author**
- On screen, over photo folders: sorting by every column and reversed, resizing columns,
  keys, scrolling, switching views: "funziona tutto". Step 5.6 closed. Asked next: the info
  pane of the columns in the grid and the details too, shown and hidden with a shortcut.

**Open**
- Human-readable type names; remembering the column widths and the order across runs (config
  or state file).

---

## 2026-10-27 — Text previews and the information card (ROADMAP 5.5)

**Done**
- New module `src/preview` (already in DESIGN): the `Provider` interface (`can_handle`, `kind`,
  `load(request, cancel)`) and three providers, chosen by `provider_for(mime)` in this order:
  images and camera raw files (`image::load_to_fit`, as before), text (the whole text/plain
  family: source code, JSON, XML, shell scripts, darktable `.xmp` sidecars), and the
  information card, which takes everything else. Quick Look keeps one `preview::Content`
  (image, text or information) per entry instead of an image; the provider is part of the slot
  key, so an entry whose type is sniffed later gets the right one.
- Text: the first 256 KB, split into lines (`\n` and `\r\n`), tabs expanded to 4 columns,
  lines cut at 1 KB on a character boundary, a UTF-8 BOM dropped; a NUL byte means binary (or
  UTF-16), and the card is shown instead. Drawn on a page up to 120 characters wide, with line
  numbers, long lines ending in an ellipsis; only the visible lines are shaped. The page
  scrolls with the wheel and the touchpad (the same `Scroller` as the views, with inertia);
  arrows still walk the folder. The caption says how many lines ("first N lines" when cut).
- Monospace text: `FontSystem` has a second fallback chain, looked up on first use, sharing the
  fonts both chains have; `Typeface::monospace` on `layout` and `draw_text`. New config key
  `font.monospace` (default "monospace": DejaVu Sans Mono here).
- The information card reads, on a worker: permissions as `ls -l` shows them (in monospace),
  owner and group names, a link's target, and a folder's number of entries.
- `nordend-snapshot --bench` scrolls Quick Look's text page when one is open.
- Tests (110 with the raw samples): lines, CRLF, tabs, BOM, binary and UTF-16, cuts by bytes and
  by line length (UTF-8 kept whole), permissions with the sticky bit, folder entries, links
  (dangling too), the provider per type; Quick Look shows text and a folder's card, and a wheel
  turn over text does not close it. Release (5 runs), ASan, TSan pass; `nordend-snapshot` over
  text, XMP, binary, folder and link, and the text benchmark, under ASan and TSan; tidy and
  format clean.

**Measured** (docs/benchmarks.md): a 1.6 MB log read (its first 256 KB, 3 196 lines) in
0.67 ms, a 10 000-entry folder counted in 4.3 ms; scrolling that log's page at 2880x1620:
0.82 ms median per frame, 1.03 ms p95.

**Found**: Quick Look over a card (also on the previous commit) makes TSan report a race
inside `libdrm_amdgpu`, between Mesa's gl0 thread and a glyph upload: both sides are driver
code. With the author's OK the suppression now covers `libdrm_amdgpu` too; it relies on GL
being called from the main thread only, as the renderer already requires.

**Decisions**
- The provider interface follows DESIGN, plus `kind()`: the view must know what to draw before
  the content arrives (the thumbnail for an image, nothing for text, which takes a few ms, the
  card at once for the rest).
- Text is not wrapped: code reads better with whole lines, and the ellipsis shows a line goes
  on. Tabs are 4 columns.

**Tested by the author**
- On screen, over source code, long files, folders, binaries and darktable sidecars, scrolling
  with touchpad and wheel: "tutto perfetto". Step 5.5 closed. Asked for later: syntax
  highlighting and text selection in the preview (ROADMAP 15, DESIGN "Dopo l'MVP").

**Open**
- Text in other encodings (Latin-1) shows replacement characters; UTF-16 gets the card.
- No syntax highlighting, no horizontal scrolling, no selection or copy in the text page
  (highlighting and selection: ROADMAP 15).

---

## 2026-10-27 — Raw previews (ROADMAP 5.4)

The "Fatto quando" (Quick Look of an ORF < 100 ms) was already met in 5.3 (51-55 ms); this step
went through the open points of 5.2 with the rawsamples.ch files of other cameras.

**Done**
- TIFF previews: `EmbeddedPreview` now carries `data` (a JPEG or a TIFF) and `image::decode_preview`
  decodes either, cropped. Our reader for the simple kind raw files carry (uncompressed, chunky,
  8-bit RGB, in strips) in `src/image/src/tiff.{hpp,cpp}`, with the TIFF integer reader moved
  there from `decode.cpp`. A TIFF is taken only when no JPEG is large enough and the TIFF is
  larger: the Hasselblad 3FR now has a thumbnail and a Quick Look (320x240, its only preview).
- Canon's TIFF previews are never taken: extracted and looked at, they are sensor data without
  white balance (green, dark), 16 bits on the 1D X and 5DS. So the Canon 5DS large thumbnail
  stays at ~370 ms (its next preview is the 50 MP JPEG).
- Panasonic crop: the GX8 stores a 4:3 preview whatever the aspect ratio, with the frame in its
  first IFD (CropTop/Left/Bottom/Right, tags 0x2F-0x32, which exiv2 0.28.9 does not expose),
  relative to the sensor borders (0x04-0x07). Read by us; the crop applies to every preview of
  the file, and the full size reported is the cropped one (16:9: 5184x2920). The GX7 has no
  crop tags (it stores the preview cropped).
- Previews shaped unlike the largest one (over 2 %) are dropped: the 4:3 EXIF thumbnail of 3:2,
  16:9 and 1:1 photos has black bars (Canon, Nikon, Sony, GX7 1:1, LX3 16:9; seen in the grid,
  then measured). A centered crop was tried on paper and rejected: the bars are not exactly
  where the aspect ratio puts them (Nikon: 104 content rows, not 106.7), a dark line would stay.
- `image/tiff` is in `thumbs::can_make` (Nikon TIFF, 3FR; a TIFF without a preview is tried once
  and remembered as failed).
- `decode_file` reads 16 bytes and sniffs before reading the rest: Quick Look of a raw file
  whose preview is smaller than the screen no longer reads (and zeroes) the whole file first.
- Tests (104 in total): a TIFF-preview fixture (96x64 in strips of 7 rows, from `make.sh`),
  cut TIFF, crop in and out of bounds, `can_make`; one test over the raw samples, skipped when
  the folder is missing: every file and size gives a decodable preview of the size it says;
  3FR TIFF, no Canon TIFF, no barred EXIF thumbnail, GX8 crops, GX7 untouched. Release (5
  runs), ASan, TSan pass, also `nordend-snapshot` (grid and Quick Look over the samples, Quick
  Look over JPEG/WebP) under ASan and TSan; tidy and format clean.

**Measured** (docs/benchmarks.md): Quick Look of the 3FR 42 → 0.4 ms, Nikon TIFF 89 → 2.3 ms,
GX8 18 → 11 ms, ARW 16 → 13 ms; the ORF unchanged (51.6 ms). Normal-size (128) thumbnails of
3:2 and 1:1 photos got slower, since the barred EXIF thumbnail is skipped: Canon 5DS 9 → 372 ms,
1D X 9 → 143 ms, ARW 7.5 → 22 ms. Large (256), which the grid uses at scale 2, was already
that slow before.

**Decisions**
- Correct shape over speed for the normal size: a thumbnail with black bars is wrong forever (it
  is cached), the cost is paid once per file.
- TIFF previews are a last resort, behind JPEGs, because only some can be read and some are
  not images at all (Canon).

**Tested by the author**
- On screen, with a fresh cache over the raw samples (grid: every file has a thumbnail, no
  black bars, GX8 in its aspect ratio; Quick Look of the GX8 and the 3FR) and over ORF folders:
  "funziona tutto come hai detto". Step 5.4 closed.

**Open**
- Canon 5DS / 1D X: thumbnails and Quick Look decode the full-size JPEG (370 / 143 ms). Only a
  JPEG decoder with reduced-scale decoding would fix it (TurboJPEG 1/8: measured in 5.2 at half
  the cost for an OM-1 JPEG); not needed for the author's ORFs.
- Thumbnails already in `~/.cache/thumbnails` from before (barred, or GX8 uncropped) stay until
  the file changes: the freedesktop cache has no version. Deleting the folder remakes them.
- The 3FR shows at 320x240 in Quick Look: never enlarged, as every image.

---

## 2026-10-27 — Quick Look (ROADMAP 5.3)

**Done**
- `image::load_to_fit(file, w, h)`: the image to show in w x h physical pixels: an embedded
  preview when it is at least that large (or the file is raw), else the whole image; scaled
  first, turned after; reports the full image's size (exiv2's pixel size for previews).
  `EmbeddedPreview` now carries the main image's size.
- `views::QuickLook`, owned by `Browser`: Space (no modifiers) opens it on the cursor entry of
  the current folder (focused column, or the grid's). It follows the cursor: every other key goes
  to the view underneath (arrows, Page, Home/End, also Enter or Ctrl+1/2) and Quick Look shows
  where the cursor lands, closing when it is on nothing; Space, Escape or a click close it
  (Escape does not reach the view: a multiple selection survives). Over a scrim with a 140 ms
  fade: images and raw files fitted to the window and never enlarged past their pixels, rounded,
  with a shadow and a caption (name, full size, file size). Two stages: the cached thumbnail at
  once (stretched where the image will land), then the sharp image. The entries before and after
  are made in advance (nearby priority); everything else is cancelled. Other entries get an
  information card (icon, name, type, size, date); text previews are ROADMAP 5.5.
- `image::scale_to_fit` rewritten as a streaming area filter: no intermediate image (it was 83 MB
  for 3200x2400 → 2160x1620), an opaque fast path, one reciprocal per pixel: 84 ms → 27 ms.
  Thumbnails gain too (xx-large from an ORF preview: 26 → 16 ms).
- Textures dropped outside a draw are kept until the next draw, so GL deletes always happen with
  the context current.
- 4 new test cases (101 in total): open on the cursor, arrows follow, upright size, information
  card, Escape and Space close; Escape does not collapse a selection, Ctrl+Space stays
  selection; over the grid, a click closes; an image whose type is not known yet is shown once
  sniffed. Release (5 runs), ASan, TSan pass, also `nordend-snapshot` with Quick Look open over
  JPEGs and ORFs under ASan and TSan; tidy clean apart from reference members.

**Found by the tests**: `a.png` has no type from its name alone: shared-mime-info gives `*.png`
to both image/png (weight 50) and image/apng (40), so, as in GLib, the content decides and the
type arrives when the views sniff the visible rows. Quick Look decided only once whether an
entry was an image; now `follow()` checks again every frame (cheap, idempotent) and asks for the
sniffing of the shown entry and its neighbours itself.

**Measured** (docs/benchmarks.md): an OM-1 ORF fitted to a 2880x1620 screen in 51-55 ms (the
5.4 target is < 100 ms), a 20 MP JPEG in 212 ms (its preview is smaller than the screen, so the
whole image is decoded; the thumbnail shows meanwhile).

**Decisions**
- Quick Look follows the view's cursor rather than keeping its own position: one cursor, and
  closing leaves it on the last entry seen (as Finder).
- The sharp stage is made for the image area of the last draw (the window size), not the
  image's full size: no zoom yet.

**Tested by the author**
- On screen, in columns and grid, over ORF, WebP, JPEG and the raw samples: "tutto perfetto,
  fluidità ottima, caricamento fulmineo". Step 5.3 closed.

**Open**
- GIF and WebP animations show their first frame.
- No zoom or 1:1 view; a window made larger after opening keeps the image made for the old size
  until the next entry.

---

## 2026-10-27 — Thumbnails (ROADMAP 5.2)

**Done**
- Measured first (docs/benchmarks.md): TurboJPEG's DCT scaling only halves the cost of an OM-1
  JPEG (145 ms with wuffs, 75 ms at 1/8), while the files carry a 1600x1200 preview (JPEG) or a
  3200x2400 one (ORF) that decodes in 8 / 30 ms. With the author's OK: no libjpeg-turbo, exiv2
  now (planned for 5.4) to find the embedded previews. `nordend-bench-jpeg` stays as a dev tool,
  built only when libjpeg-turbo-devel is installed.
- `image::embedded_preview(path, min_side)`: exiv2's previews (EXIF thumbnail, raw previews),
  plus the Olympus / OM System maker-note preview that exiv2's PreviewManager does not list for
  JPEGs: at TIFF start + `Exif.MakerNote.Offset` + `Exif.OlympusCs.PreviewImageStart` (checked
  against exiftool on a JPEG and an ORF). Picks the smallest one big enough; reports the main
  image's orientation, which the previews share (they are stored as the sensor sees, checked on
  screen).
- `image::scale_to_fit` (area average, alpha weighted, 2x2 halving first), `image::encode_png`
  (our writer on zlib, level 1, per-row filter by minimum sum; tEXt chunks) and
  `image::png_text`. A shared JPEG segment walker gives EXIF and frame size.
- New module `src/thumbs` (already in DESIGN): the freedesktop cache. MD5 of the file URI (our
  RFC 1321 code), URI escaped exactly as GLib's `g_filename_to_uri` (checked with `gio`), sizes
  normal/large/x-large/xx-large, valid while Thumb::URI and Thumb::MTime match, written through
  a 0600 temporary file and a rename, directories 0700, failures remembered in
  `fail/nordend-<version>/` until the file changes, the cache never thumbnails itself. Source
  order: valid cached thumbnail, embedded preview big enough, whole image, else the largest
  preview (raw files). Scaled first, turned upright after. `thumbs::can_make(mime)`: the five
  decoded formats and the camera raw types.
- UI: `ui::Texture` (RGBA, premultiplied on upload, mipmapped) and `Renderer::draw_image`
  (textured quad cut to rounded corners by the box distance function, clipped like the rest).
- `views::Thumbnails`: thumbnails as textures for the views. Asked for per frame by what is
  drawn, made on workers (visible priority) through the cache, cancelled when no longer drawn,
  textures kept within 256 MB (least recently drawn dropped first); textures are created while
  drawing. The grid shows them in the icon's square at the cache size that covers the icon in
  physical pixels; the icon shows until the thumbnail is ready. The app uses
  `~/.cache/thumbnails`; `nordend-snapshot --thumbnails DIR` takes another cache (none by default).
- `nordend-thumb [--size ...] [--cache DIR] [--make] [--ppm OUT] FILE...`.
- Fixture `with-thumbnail.jpg` (480x320 with a 192x128 EXIF thumbnail, orientation 6); `make.sh`
  now writes PNGs without date chunks (reproducible). 10 new test cases (97 in total): scaling
  and alpha, PNG round trip with text, embedded preview and orientation, MD5 (RFC suite and the
  spec's example), GLib URI escaping, sizes, cache hit / miss on mtime change / preview or
  decode by size / permissions, failures remembered and retried, the cache not thumbnailing
  itself. Release, ASan and TSan pass, also `nordend-snapshot` making thumbnails on all workers
  under ASan and TSan; tidy clean apart from reference members.

**Measured** (docs/benchmarks.md): a thumbnail from an OM-1 JPEG in about 12 ms (large), from
an ORF in about 36 ms; a screenful of the grid in 0.15-0.4 s with an empty cache, 12-22 ms warm;
scrolling with thumbnails 0.62 ms per frame.

**Decisions**
- zlib level 1 for the thumbnail PNGs: 5 times faster than level 6 for files 13 % larger.
- Thumbnails are made only for the types listed in `thumbs::can_make`; other images (SVG, TIFF,
  ...) keep their icon rather than filling the failure cache.
- The thumbnail cache is the shared one (`~/.cache/thumbnails`), as the ROADMAP asks; no
  private cache.

**Tested by the author**
- On screen: thumbnails in the grid, scrolling, every cell size, switching views, the cache
  reused on the next start, `nordend-thumb` reading from the cache: "funziona tutto". Step 5.2
  closed.

**Open**
- `views::Thumbnails` has no unit test: it needs a GL context for its textures; it is covered by
  the snapshots (also under ASan/TSan).
- The Olympus maker-note path is covered by real files only (no small fixture carries one).
- `nordend-bench-dir` makes empty files: a folder of 10 000 real-looking images would stress the
  request/cancel path; not done yet.
- Thumbnails are not removed when their file is deleted (the spec leaves that to cleaners).
- Raw samples from rawsamples.ch (22 files, Canon, Fuji, Hasselblad, Nikon, Panasonic, Sony;
  kept by the author in `DATA/fotografia/raw_text_examples`, not in the repo): 20 of 22 get a
  thumbnail from their JPEG preview (5-25 ms at large). Found, for ROADMAP 5.4: (1) Hasselblad
  3FR carries only an uncompressed TIFF preview (320x240), which we skip; Canon 5DS has a TIFF
  preview of 556x370 next to its 50 MP JPEG, so "large" decodes the 50 MP one (366 ms): reading
  simple uncompressed TIFF previews would fix both. (2) Panasonic GX8 stores the 4:3 sensor
  preview for every aspect ratio and records the crop in tags (`Crop*`); the GX7 stores it
  already cropped. (3) `image/tiff` is not in `can_make` (the D800 TIFF has a 570x375 JPEG
  preview); 3FR is detected as `image/tiff` too.

---

## 2026-10-27 — Image decoding (ROADMAP 5.1)

**Done**
- New module `src/image` (`nordend::image`, added to DESIGN's architecture with the author's
  OK): `decode(bytes)` / `decode_file(path)` give RGBA pixels with straight alpha, the format and
  the EXIF orientation, or an error; never throw on bad input; thread-safe for the workers.
  `sniff()` tells the format from the first bytes. Images over 200 MP are refused before any
  pixel memory is allocated.
- PNG, JPEG, GIF with wuffs, vendored (author's OK) from google/wuffs `v0.4.0-alpha.12`, file
  unmodified in `third_party/wuffs`, listed in `THIRD_PARTY.md`. Built as its own static library
  with only the PNG/JPEG/GIF modules and the `wuffs_aux` API, without our warnings; decoding goes
  straight into the Image's vector (no copy).
- WebP with libwebp (still images, the first frame of animations through the demuxer, EXIF
  chunk) and JPEG XL with libjxl (first frame, orientation reported rather than applied, so all
  formats go through the same turn). New build dependencies: `libwebp-devel`, `libjxl-devel`.
- EXIF orientation: a small TIFF reader for the orientation tag (both byte orders, bounds
  checked) and `apply_orientation()` for the eight cases. wuffs does not report JPEG metadata
  (its `set_report_metadata` is empty for JPEG), so the JPEG segments before the image data are
  walked for the APP1 "Exif" one.
- `nordend-decode [--runs N] [--stored] [--ppm OUT] FILE...`: format, size, orientation, read
  and decode times; the PPM to look at the result.
- Fixtures in `tests/data/image` (a 48x32 six-block image in every format, an orientation-6
  JPEG, an animated WebP, a truncated JPEG), generated by `make.sh` there. 6 new test cases (87
  in total): formats, same pixels from every format, alpha, orientation applied or not, the
  eight orientations, EXIF in both byte orders and malformed, truncated/missing/too large files.
  Release, ASan, TSan pass (and `nordend-decode` under ASan on real 20 MP photos); tidy clean
  apart from reference members.

**Measured** (docs/benchmarks.md): a 20 MP OM-1 JPEG decodes in about 149 ms as stored, 218 ms
turned upright; PNG 229 ms, WebP 210 ms, JPEG XL 394 ms (one thread) at the same size.

**Decisions**
- The full-size EXIF turn (about 65 ms at 20 MP) stays out of the hot paths: thumbnails turn
  after scaling, Quick Look on the GPU. `DecodeOptions::apply_orientation = false` is there for
  that.
- wuffs's JPEG has no decoding at reduced size; libjpeg-turbo does (DCT scaling). Whether to
  use it for thumbnails is decided in 5.2, with a real measurement (ImageMagick suggests a large
  gain, *to be verified*).

**Tested by the author**
- Tests pass on their machine; `nordend-decode` on two OM-1 JPEGs: 3888x5184, orientation 8,
  214-218 ms; the decoded photo, opened in swayimg, stands upright with the right colors; every
  fixture decodes, the truncated JPEG fails cleanly. Step 5.1 closed.

**Open**
- JPEG XL decodes on one thread; libjxl's parallel runner could help Quick Look (5.3).
- No cancellation inside a decode: callers check their token before and after.
- EXIF orientation of PNG (eXIf chunk) comes through wuffs's metadata report; not tested with a
  real file yet.

---

## 2026-10-27 — Grid view (ROADMAP 4.4)

**Done**
- `views::Folder`: the per-folder state split out of `Column` (model, cursor, selection, anchor,
  sniffed types), with nothing about drawing. Views hold it by `shared_ptr`, so the same object
  passes between the columns and the grid: cursor, selection and sniffed types stay, nothing
  reloads. Where the cursor sits on screen is the view's job: the Folder only records how to
  bring it into sight (`take_reveal()`: glide after a move, jump after it lands or is asked for
  by name), and each view applies it at its next draw with its own geometry. `Column` is now
  only drawing, scrolling and hit testing.
- `views::GridView`: one folder as a grid of cells (icon and name, middle-ellipsized), filled row
  by row, stretched to the width, virtualized by rows like the list. Six sizes, 48 to 256 px
  (default 96): Ctrl+plus/minus, Ctrl+wheel, Ctrl+0; a zoom keeps the cursor (or the first
  visible cell) where it was on screen. Keys: arrows (Up/Down by a row, not past the first or
  last row), Page Up/Down, Home/End, the selection keys of the column view; Enter, Alt+Down or a
  double click open a folder, Backspace, Alt+Up or mouse back open the parent with the cursor on
  where we were. Plain Space stays free for Quick Look.
- `views::Browser`: owns both views; Ctrl+1 columns, Ctrl+2 grid. The grid takes the focused
  column's folder; the column view meanwhile is suspended (its folders' changes are not synced
  while hidden). Coming back, `ColumnView::show(folder)` brings the columns up to date and
  focuses the column holding that folder; if the grid went elsewhere, the columns unfold from
  the deepest column that leads to the grid's folder (its parent, when none does) and the grid's
  folder object becomes the last column as it is.
- Title bar and the item colors are shared by both views (`draw_helpers`, `item_style`).
- `nordend-snapshot --keys "Ctrl+2,..."` reaches the grid; `--bench` scrolls the grid when it
  is shown.
- 4 new test cases (81 in total): the grid shares the column's folder and the columns follow its
  changes; Enter/Backspace in the grid and the columns rebuilt around the grid's folder (also
  from above the first column); cell sizes and their limits. Release (10 runs), ASan, TSan
  (5 runs) pass; tidy clean apart from the reference members the views already had.

**Measured**: scrolling the 10 000-entry folder at 2880x1620, 0.33 ms median at the default size,
0.84 ms with the smallest cells all selected (docs/benchmarks.md).

**Decisions**
- Ctrl+1 / Ctrl+2 as in Nautilus (the Linux habit), not Finder's Cmd+1..4.
- The grid has no info pane: Quick Look (5.3) is its preview.
- A folder opened from the grid starts without a cursor (the first arrow puts it on the first
  cell); the columns put it on the first row when entering, because the next column shows it.
- Cells are at least 132 px wide, so a `P1234567.ORF` name fits at the smallest size.

**Tested by the author**
- On screen: switching columns ↔ grid with the selection kept, keys and clicks in the grid,
  zoom, touchpad and wheel scrolling, opening folders and going back: "all smooth", and the
  test suite passes on their machine. Step 4.4 closed, and with it phase 4's views.

**Open**
- The cell size is not remembered across sessions (a config entry, later).
- Keys pressed before the grid's first draw step one cell at a time (the row width is known at
  the draw): only the snapshot tool can do that.
- Thumbnails replace the icons in phase 5; names on one line for now (Finder uses two).

---

## 2026-10-26 — Selection (ROADMAP 4.3)

**Done**
- `views::Column` has a selection next to its cursor: a set of names plus an anchor, both kept by
  name like the cursor (they follow renames, lose deleted entries, and names that do not come
  back after a reload are dropped). Moving the cursor takes a `SelectMode`: replace (plain click,
  arrows), extend (Shift: from the anchor to the cursor), keep (Ctrl+arrows: cursor alone), toggle
  (Ctrl+click). Plus `select_all` (Ctrl+A), `collapse_selection` (Escape: back to the cursor's
  entry) and Ctrl+Space (toggle the cursor's entry, for non-contiguous selection from the
  keyboard). Plain Space is left free for Quick Look (5.3).
- The column invariant now reads "what column i has selected, when it is one entry": with several
  entries selected (or the cursor away from the selection) no column opens on the right and the
  info pane sums the selection up (count, folders and files, size of the files; folder sizes are
  not computed). The title shows "N of M selected".
- Drawing: selected rows are filled (primary in the focused column, quiet elsewhere); when the
  cursor is not the whole selection, a ring marks it.
- `nordend-snapshot --keys "Right,Shift+Down,Ctrl+a"` presses keys in the column view before the
  snapshot (xkb key names, Ctrl+/Shift+/Alt+ prefixes): offscreen checks of states that need
  input.
- 3 new test cases (77 in total): Shift ranges (grow, shrink back, Escape), Ctrl moves and
  toggles, Ctrl+A, plain Space ignored, toggling and ranges by row, selection across a rename and
  a deletion (inotify). Release, ASan, TSan pass (column tests 10 times in Release, 5 in TSan);
  tidy clean.

**Measured**: scrolling a 10 000-entry column at 2880x1620, 0.26 ms median with nothing selected,
0.52 ms with everything selected (docs/benchmarks.md).

**Decisions**
- A Shift range replaces the selection with anchor..cursor (Finder, Nautilus); Ctrl+click is the
  way to add to it.
- Selection lives in the focused column only in practice: clicking another column replaces its
  selection there; columns on the left keep theirs, which is the path to the focused one.
- No rubber-band (drag) selection: not in the ROADMAP; with the drag and drop of later phases.

**Tested by the author**
- On screen in the photo library: clicks with and without modifiers, keyboard ranges and toggles,
  Ctrl+A, Escape, the summary pane: "works by the book". Step 4.3 closed.

**Open**
- The summary counts what the listing holds; folder sizes would need a recursive walk (a worker
  job, later, if wanted).

---

## 2026-10-26 — Miller columns (ROADMAP 4.2)

**Done**
- `views::Column`: one folder as a virtualized list of icon, name and a chevron for folders,
  built from the 4.1 list (same scroller, same visible-rows drawing, same sniffing of visible
  entries). It has a cursor, kept by name: it follows its entry when rows move or the entry is
  renamed, is dropped when the entry goes, and can name an entry not loaded yet (pending: it
  lands when the entry arrives). Moving it glides the list just enough to keep it in view; a
  cursor that lands later jumps there, centered.
- `views::ColumnView`: Miller columns. One invariant does the work (`sync_after`): the column
  after column i always shows what column i's cursor is on (same folder: kept as is; another
  folder: a new column; a file or nothing: no column, the info pane shows the file). Keys and
  clicks only move cursors and focus. Up/Down, Page Up/Down, Home/End move the cursor (a folder
  opens at once in the next column); Right/Enter go into it; Left goes back one column (the
  column left behind stays open, without cursor), and from the first column opens the parent as
  a new first column, with the cursor on where we were (Backspace, Alt+Up, mouse back do the
  same). A click puts the cursor on a row (an empty area clears it).
- Resizing: drag a column's right edge (ew-resize cursor, the edge lights up), 140-800 px; a
  double click on the edge restores 260 px. A new column takes the width of the one it replaces,
  so a widened column stays wide while browsing.
- Horizontal scrolling: a second `ui::Scroller` for the row of columns, which glides by itself
  to show the focused column and what it opens. Touchpad (with inertia), tilt wheel and
  Shift+wheel scroll it by hand; a touchpad gesture locks to the axis it starts on and to the
  column it starts over, so a vertical swipe does not drift sideways. A parent column opened from
  the left slides in. Thin scrollbar at the bottom when the columns do not fit.
- Info pane, after the last column when its cursor is on a file: the file's icon (64 px), name,
  MIME type, size and date. It is the place the previews of phase 5 will take.
- `ui::Scroller::glide_to` and `destination()` (where a glide is heading: keeping a cursor in
  view while a key repeats must count from there, not from the current offset).
- `views::ListView` is gone: the columns replace it in the app and in `nordend-snapshot`
  (`--list` became `--columns DIR [--reveal PATH]`).
- 5 new test cases (74 in total): navigation with the arrows, Left from the first column, reveal,
  a folder renamed and then deleted under the cursor (inotify), `glide_to`. Release, ASan, TSan
  pass (column tests 10 times in Release, 5 in TSan); tidy clean.

**Measured**: scrolling a 10 000-entry last column with the whole view drawn, 0.19 ms median at
960x640, 0.23 ms at 2880x1620, as the list (docs/benchmarks.md).

**Decisions**
- Each column is its own DirectoryModel (its own watch and load), created when the cursor lands
  on a folder: holding Down through a hundred folders opens and cancels a hundred loads. Loads
  cancel cleanly and each costs well under a millisecond for ordinary folders; to measure on
  screen before adding any delay.
- No selection yet beyond the cursor (ROADMAP 4.3), and nothing opens files (6.1).

**Tested by the author**
- On screen: navigation, resizing, horizontal scrolling, info pane, and a folder created,
  renamed and removed from a terminal while shown: "everything works". Step 4.2 closed.

**Open**
- When the columns shrink (a cursor moved to a file in a left column), the horizontal offset is
  clamped to the new width at once instead of gliding: to judge on screen.
- The info pane shows the MIME type name (`image/x-olympus-orf`), not a readable description:
  shared-mime-info's comments live in the XML files, not parsed yet.

---

## 2026-10-26 — Virtualized scrolling list (ROADMAP 4.1)

**Done**
- `ui::Scroller`: scrolling physics with no clock of its own (time comes with input and
  frames, so tests replay it exactly). Wheel: 48 px per notch toward a target, gliding there
  (exponential approach, 50 ms time constant); notches during a glide add up. Touchpad: 1:1
  while the fingers move, then inertia from the release velocity (movement of the last 100 ms),
  decaying with a 330 ms time constant, stopping under 15 px/s or at an edge. Clamped to the
  content. Frames are requested only while a glide or inertia runs: idle stays at zero CPU.
- New module `views`. `views::ListView` shows a DirectoryModel: title (folder, item count,
  loading, error), column headers, rows of icon, name (middle ellipsis), size (SI units) and
  date ("2026-10-26 14:03", local), hover highlight, a thin scrollbar. Only the rows inside the
  viewport are drawn (`visible_rows`). Entries the name does not type are sniffed on a worker
  when they become visible (batches of 64, dropped when another folder opens): the "visible
  entries" half of ROADMAP 3.2. Double click on a folder opens it; Backspace, Alt+Up or the
  mouse back button go to the parent; arrows, Page Up/Down glide, Home/End jump.
- `nordend [DIR]` now opens DIR (default: home) in the list; `--test-scene` keeps the renderer
  scene. The 3.4 icon grid (`--icons`) is gone: the list replaces it.
- `nordend-snapshot --list DIR [--bench FRAMES]`: the list offscreen, and a scrolling benchmark
  (top to bottom at constant speed, glFinish after every frame). Also fixed: the first draft of
  the release velocity counted the first finger sample, overestimating by 20 % (caught by a
  test).
- ThreadSanitizer: Mesa's gallium GL thread reports races inside libgallium in every GL
  program (12 reports with the old test scene already). They are suppressed by a
  `__tsan_default_suppressions` entry for libgallium only; with it, the list benchmark and the
  tests report nothing in our threads.
- 6 new test cases (69 in total): wheel glide and accumulation, edges, inertia distance and
  stop, no coasting after a slow move or a pause, extent changes, size formatting, visible rows.
  Release, ASan, TSan pass; tidy clean.

**Measured** (offscreen, Release, glFinish per frame, 600 frames scrolling the whole list)

| List | Buffer | median | p95 | max |
|------|--------|-------:|----:|----:|
| 10 000 entries | 960x640 | 0.20 ms | 0.28 ms | 5.6 ms (first frame, glyph rasterization) |
| 10 000 entries | 2880x1620 (1920x1080 at 1.5) | 0.25 ms | 0.45 ms | 1.5 ms |
| 123 entries (photo folder) | 960x640 | 0.17 ms | 0.21 ms | 5.9 ms |

A 60 Hz frame has 16.7 ms: drawing the list takes about 1.5 % of it.

**Tested by the author**
- On screen: wheel, touchpad inertia, keys, opening folders: "the touch experience is
  magnificent", the keyboard handling "very smart". Step 4.1 closed. At the author's request the
  README gained a Features section, with scrolling as "Natural touch".

**Open**
- Text layouts are cached up to 8 192 and then the cache is cleared whole: fine at these numbers,
  to revisit (LRU) if long scrolls through huge folders show a hitch.

---

## 2026-10-26 — File-type icons (ROADMAP 3.4)

**Done**
- Icons are Tabler line icons, as in Noctalia (DESIGN: "Icone UI: Tabler"), drawn from an icon
  font and tinted by the palette. The font is found in the XDG data dirs: Nordend's own copy
  (`nordend/fonts/tabler-icons.ttf`, none yet) first, then Noctalia's
  (`noctalia/assets/fonts/noctalia-tabler.ttf`, the full set: 5 958 icons). Nothing is copied
  into the repository. Without a font Nordend runs without icons and says so in the log.
- Icons are looked up by glyph name in the font (`FT_Get_Name_Index`: the font keeps Tabler's
  names), so no codepoint table and no JSON parser.
- `FontSystem::add_font_file` (outside the fontconfig fallback chain: text never picks it),
  `glyph_by_name`, `glyph_layout` (one glyph with its bounding box centered in a size x size box:
  Tabler glyphs sit off-center in their em square). `ui::IconSet` finds the font, caches glyphs,
  draws by name; an unknown name draws "file".
- `fs::icon_name(db, entry)`, in order: kind (folder, folder-symlink, link-off for a dangling
  link, plug for devices/sockets/fifos); darktable `.xmp` sidecars (adjustments-horizontal:
  shared-mime-info has no type for them); types worth their own icon, matched with `is_a` so
  subtypes count (image/x-dcraw -> aperture for every camera RAW format, PDF, SVG, Markdown,
  SQLite, desktop files, source code and config formats -> file-code); then the freedesktop
  generic icon (`MimeDatabase::generic_icon`: the generic-icons file, else
  `<media>-x-generic`) mapped to Tabler (archives, office documents, spreadsheets,
  presentations, executables, fonts, disc images, images, video, audio, text). An empty file
  gets the icon of its name's type (its MIME type stays x-zerosize, as in GLib).
- `nordend --icons DIR`: the window shows DIR's entries in a grid, icon and name, live (the 3.3
  model), with entries the name does not type sniffed on a worker and redrawn. Not a view: no
  scrolling or selection (phase 4). `nordend-snapshot --icons DIR` draws the same offscreen.
- Fixed in `nordend-snapshot`: with a colors file (6th argument) the width, height and scale
  were ignored.
- 3 new test cases (63 in total): kinds, 21 file types on the system database, every icon name
  in use exists in the font (skipped without the font). Release, ASan, TSan pass; tidy clean.
- Checked offscreen at scale 1 and 1.5, default and Nord palettes: a sample folder with 28
  types, and `DATA/fotografia/liguria` (ORF, ORF.xmp, a subfolder).

**Tested by the author**
- `nordend --icons` on the photo folder and on the repository, live changes and a theme switch:
  "everything perfect, consistent and beautiful". Step 3.4 closed; phase 3 complete.

**Open**
- Proposal for ROADMAP 7.0 (other desktops, no Noctalia): ship a subset of Tabler (MIT) with
  Nordend, only the icons in use, in `nordend/fonts/tabler-icons.ttf`, listed in
  `THIRD_PARTY.md`. Needs the author's OK (copying third-party material).
- Special folders (home, Downloads, Pictures from xdg-user-dirs) could get their own icons in the
  views.

---

## 2026-10-26 — inotify watcher and directory model (ROADMAP 3.3)

**Done**
- `fs::DirectoryWatcher`: one inotify instance for every watched directory (the kernel caps
  instances per user, about 128, and Miller columns open several). Watch descriptors are shared
  when one directory is watched twice; events are delivered per directory, one batch per read,
  on the loop thread: created, deleted, moved_from/moved_to with the cookie, changed
  (close_write, attrib), directory_gone (delete_self, move_self, unmount), overflow.
- `fs::DirectoryModel`: what a view will show. `open()` watches first, then loads in chunks
  (3.1), so nothing that happens during the load is lost; changes seen meanwhile are re-checked
  when the load ends. Changed names are gathered for 30 ms, stat'ed together on a worker
  (`fs::read_entries`) and applied in one batch (`Listing::replace`: one erase pass, one merge):
  no I/O on the loop thread, and a burst is a few updates. Notifications (`ModelChange`) carry
  names, not indices: added, removed, updated, renamed (old, new) when moved_from/moved_to pair
  up, plus reset (open, error, reload) and loaded.
- Directory deleted or moved: the model empties with an error (ENOENT). Queue overflow: full
  reload. inotify watch limit reached (ENOSPC): the directory is still shown, without live
  updates, with a warning.
- `nordend-watch DIR` (not installed): loads a directory and prints changes live.
- 7 new test cases (60 in total): load and order, create / write / delete / mkdir, rename
  inside, moves out and in, a burst of 500 files, changes during the load (no loss, no
  duplicates), directory deleted, a directory that cannot be opened, two models sharing a watch.
  Ten Release runs without a failure (timing-based tests); ASan and TSan pass; tidy clean.

**Measured**
- 500 files created back to back: one notification. 200 files created by a shell loop
  (about 150 ms): five updates of 17-49 changes, one every 30 ms. 200 files removed with one
  `rm`: one update.

**Tested by the author** (`nordend-watch` on a test folder in `~`, steps one second apart)
- create -> `+`, rename -> `old -> new`, append -> `~`, mkdir -> `+`; 1000 files created by a
  fish loop: 32 updates of about 32 files, one every 30 ms, ending at 1002 entries; `rm file*`:
  one update of 1000 removals; moved out -> `-`; folder deleted -> error. Step 3.3 closed.
- A first attempt ran two watchers at once and pasted every command in one go: doubled,
  interleaved output, still correct. `nordend-watch` now lists names only for updates of up to
  10 changes and summarises bigger ones on one line.

---

## 2026-10-26 — MIME types (ROADMAP 3.2)

**Done**
- `fs::MimeDatabase`: our own reader of shared-mime-info's `globs2`, `magic` (binary format:
  indents, masks, word sizes, ranges), `aliases` and `subclasses`, from `$XDG_DATA_HOME/mime`
  and each `$XDG_DATA_DIRS/mime` (`__NOGLOBS__` / `__NOMAGIC__` honoured). No `mime.cache`, no
  library. Types are interned ids (`MimeId`); the database is immutable and shared by workers.
- Detection follows GLib's `g_content_type_guess` + xdgmime, rule for rule, so Nordend agrees
  with GTK apps and `gio`. Each rule below fixed a real disagreement found by comparing:
  - name first: a literal name wins; otherwise the longest matching suffix; full fnmatch patterns
    only without a suffix match; case-sensitive globs beat case-insensitive ones (`main.c` vs
    `*.C`); ties by weight, then pattern length, then file order (`*.json` -> application/json);
  - several types from the name, at any weight, mean the content decides (`*.py` with a
    `python3` shebang);
  - a magic match of priority >= 80 wins; a weaker one only breaks ties between name types,
    preferring the name type that is a subtype of it (`libEGL.so.1`: shared library, not
    executable);
  - text: no control byte but tab/LF/CR in the first 128 bytes, else GLib's looks_like_text;
  - empty regular files are `application/x-zerosize` whatever their name;
  - security: content that looks like a `.desktop` launcher in a file not named `*.desktop` is
    `text/plain`.
- The loader fills `Entry::mime` from kind and name only (no file opened); entries the name does
  not decide stay unknown and `detect()` reads their head (up to the magic extent, 18.7 KB here,
  capped at 64 KB) when a view needs them.
- `nordend-mime PATH...` (not installed) prints the detected types; `nordend-bench-dir` gained a
  "mime" column (types from names) and types entries in the async path.
- Suffix lookup made allocation-free (transparent hash) and limited to positions whose byte can
  start a suffix pattern: 5.9 -> 1.7 ms for 10k names.
- 8 new test cases (53 in total) on a synthetic two-directory database: globs, case, weights,
  `__NOGLOBS__`, aliases, subclasses, magic (children, mask, word size, range), detection order,
  loader types. Release, ASan, TSan pass; tidy clean.

**Measured**
- Agreement with `gio info -a standard::content-type`: **0 differences on about 4 750 files**
  (`/usr/bin`, `/usr/lib`, `/usr/lib/python3.14`, `/etc`, `/usr/include`, xkb symbols, fonts, a
  photo folder, and a sample of 387 files with 261 different extensions from `DATA`).
- Database load: 1.0 ms (1 231 globs, 507 magic sections).
- Types from names: 1.7 ms for 10k entries (warm, median of 20); 66 % of the generated folder is
  decided by name (the `.ORF.xmp` sidecars have no glob in shared-mime-info).
- By content, warm: 163 `.xmp` sidecars 7 ms; `/usr/bin` (1 475 files, nearly all without
  extension) 52 ms. This is why content detection is left to the views, for visible entries, on
  workers.

**Tested by the author**
- Tests green; `DATA/fotografia/liguria` (123 entries: ORF, ORF.xmp sidecars, a subfolder)
  compared file by file with `gio`: no difference. Step 3.2 closed.
- DESIGN updated with the author's OK: own parser of globs2 and magic, GLib's detection order.

---

## 2026-10-26 — Async directory loader (ROADMAP 3.1)

**Done**
- ROADMAP 2.6 (template PR to Noctalia) postponed to after 7.3: a builtin template makes sense for
  an installable app. The template keeps working from `void-noctalia`.
- `WorkerPool::submit_streaming`: work gets an `emit(value)`; values reach the loop thread in
  order, before `done`, with the same cancel / pool-gone rules. Both submit paths share `deliver`.
- New module `fs` (`nordend::fs`):
  - `Entry`: name (bytes as on disk), kind, `is_link`, size, mtime in ns, `hidden()`. A link takes
    its target's kind; a dangling link is `other`; an entry that vanishes during the read is skipped.
  - `read_directory`: `opendir`/`readdir` + `fstatat` per entry (a second one for links), chunks
    of 128 doubling up to 4096; cancellation checked every 64 entries and before every chunk.
  - `load_directory`: the same on a worker (priority visible), chunks and result on the loop thread.
  - Sorting: `natural_compare` (digit runs by value, ASCII case ignored, ties by bytes so the order
    is total), `SortOrder` (name / size / modified, descending, directories first and not reversed),
    `Listing`: each chunk sorted and merged (`inplace_merge`), `set_order` re-sorts.
    Known limit: no locale collation, accented letters sort after z.
- `nordend-bench-dir` (not installed): `--make N DIR` creates a camera-like folder (ORF, ORF.xmp
  sidecars, JPG, subfolders); a run measures readdir alone, readdir + stat, sort by name, and the
  async path to the first merged chunk and to the complete sorted list. Run 1 apart (may be cold),
  min / median / max over the rest.
- 12 new test cases (45 in total). Release, ASan, TSan pass (bench too); tidy clean.

**Measured** (warm cache, Release, 8 workers, 20 runs, median ms)

| Directory | Entries | readdir | + stat | sort | 1st chunk | complete |
|-----------|--------:|--------:|-------:|-----:|----------:|---------:|
| generated, tmpfs | 10 000 | 2.08 | 15.44 | 3.61 | 0.52 | 16.93 |
| `/usr/lib`, btrfs on NVMe | 1 862 | 0.51 | 4.49 | 0.49 | 0.77 | 5.19 |
| `/usr/bin`, btrfs on NVMe | 1 493 | 0.40 | 2.89 | 0.31 | 0.79 | 3.26 |
| `DATA/fotografia/vero`, ext4 on SSD | 327 | 0.13 | 0.52 | 0.07 | 0.40 | 0.75 |

- DESIGN target "10k folder: first frame < 50 ms, full list in background": the first chunk is
  ready in about 0.5 ms and the whole sorted list in about 17 ms, warm. stat is most of the
  cost (about 1.3 us per entry, more for links: `/usr/lib`); the sort is about 3.6 ms for 10k.
- `/usr/bin` run 1: 19 ms for readdir + stat against 2.9 ms afterwards: a cold inode cache costs
  much more than the work itself.

**Tested by the author** (10k generated files on the SATA SSD, ext4)
- Warm, median of 20: readdir 3.69, + stat 18.01, sort 3.47, 1st chunk 0.80, complete 19.51 ms.
- Cold (after drop_caches), run 1: + stat 79.09 ms. The tool then read synchronously before the
  async path, so the async columns were already warm: the bench now runs the async path first.
  Rerun cold: **1st chunk 1.72 ms, complete 81.27 ms** (warm median 0.75 / 19.59 ms). Disk
  latency stays off the first frame.
- Natural order in `--list` checked ("Session 2" before "Session 10", IMG_2, IMG_5, ... IMG_11).
- Results collected in `docs/benchmarks.md`, summary in the README (author's request).
- Step 3.1 closed.

---

## 2026-10-26 — Noctalia theme (ROADMAP 2.5)

**Done**
- Noctalia template `nordend` (in `void-noctalia/config/noctalia/templates/nordend`, user
  template entry in its `config.toml`): `mode` plus a `[colors]` table written by a `for` loop over
  every Noctalia token (`name = "#rrggbb"`), output `~/.config/nordend/colors.toml`. Nordend reads
  the roles it knows and ignores the rest, so the template never has to change when Nordend uses
  a new role: good for the upstream PR (2.6). Checked with `noctalia theme <image> -r in:out`: 79
  lines, 48 Material tokens + hover + terminal tokens, parsed by Nordend with no problem.
- How Noctalia writes outputs (5.3.0, `template_engine.cpp`, `renderFile`): only when the content
  changed, in place (`std::ofstream`, truncate + write), no temp file + rename.
- `theme::Palette`: 33 roles in Noctalia's names, plus dark/light. Defaults = Noctalia's own
  built-in "Noctalia" palette, dark, expanded the way Noctalia does it (values dumped with a
  scratch build of its `fixed_palette.cpp`; listed in `THIRD_PARTY.md`). `parse_palette` /
  `load_palette`: missing roles keep the default and are listed in one line; bad values
  (`#rrggbb`/`#rrggbbaa` only) are reported where they are; extra tokens are ignored.
- `theme::Theme`: loads at startup, reloads on change. A missing file means the default palette;
  a file that does not parse keeps the palette in use (no flash of default colors).
- `core::FileWatcher`: inotify on the parent directory (created if missing), so a file that does
  not exist yet, rename-replacement and removal are seen. `IN_CLOSE_WRITE`, not `IN_MODIFY`
  (which fires after the truncation, before the content). Events are coalesced: one reload 50 ms
  after the last event. A queue overflow counts as a change.
- `core::read_text_file`, shared by config and palette loading.
- The window clears to `surface`; test scene, button and status lines use palette roles.
  `nordend-snapshot` takes an optional colors file (default palette otherwise, so snapshots stay
  reproducible); both the default and a generated palette checked offscreen.
- 10 new test cases (33 in total): palette parsing, reload rules, watcher (coalescing, other
  files, rename, removal then rewrite, missing directory). Release, ASan, TSan pass; tidy clean.

**Tested by the author**
- Tests green. Template deployed, `noctalia msg config-reload` + `templates-apply` write
  `~/.config/nordend/colors.toml` (Nord). Palette changes in Noctalia reach the open window at
  once (seven reloads in one session).
- Broken file at startup: warning with line/column, default palette. With the window open:
  `templates-apply` restores Nord; a broken file keeps the current palette; removal gives the
  default palette; `templates-apply` again brings Nord back. Step 2.5 closed.
- `void-noctalia`: template and config entry committed there.

---

## 2026-10-26 — Config (ROADMAP 2.4)

**Done**
- `core::load_config` / `parse_config` (toml++ 3.4, Void package `tomlplusplus-devel`, linked as
  its shared library and included only in `config.cpp`). Path: `$XDG_CONFIG_HOME/nordend/config.toml`,
  else `~/.config/nordend/config.toml` (a relative XDG value is ignored, as the spec says).
- Reading never fails: missing file = defaults; syntax error = the whole file ignored; unknown
  key or section, wrong type, empty string or out-of-range value = reported, that key keeps its
  default, the rest applies. Every problem is one line `file:line:column: what; using X`, sorted
  in file order (toml++ tables iterate by key, so problems are collected and sorted).
- First keys, only things Nordend already uses: `font.family` (fontconfig pattern, default
  `sans-serif`), `pointer.double_click_ms` (100-2000, default 400), `pointer.double_click_distance`
  (0-50 logical px, default 4). The Seat constants became `Seat::set_multi_click()`.
- Command line: `--config FILE`, `--check-config` (problems on stderr, verdict on stdout, status 1
  if any problem; no Wayland connection), unknown options exit with status 2 and a usage text.
  Problems are also logged as warnings at startup.
- `docs/config.example.toml`: every key with its default; a unit test checks it parses clean and
  matches the built-in defaults, so it cannot drift.
- 12 new test cases (23 in total). Release, ASan and TSan pass; tidy and format clean.
- GCC 14 `-Wnull-dereference` fires inside libstdc++ for `std::string(istreambuf_iterator...)`:
  the file is read with a sized `read()` instead.

**Tested by the author**
- Tests green; `--check-config` on a missing file (status 0), the example (ok, 0) and a file with a
  second `[pointer]` table (toml++: "cannot redefine existing table", whole file ignored, 1).
- In the window: `family = "serif"` picks DejaVu Serif, `double_click_ms = 1500` turns two slow
  clicks into a double click; no warnings in the log. Step 2.4 closed.

---

## 2026-10-26 — Event loop and worker pool (ROADMAP 2.3)

**Done**
- `core::EventLoop`: one `poll()` over fd sources, timers (timerfd, one-shot or repeating), and
  `post()` from any thread (mutex-guarded queue + eventfd). Sources can be added/removed from
  inside callbacks (removal is deferred to the next iteration; a source removed earlier in the
  same dispatch is skipped). `FdOptions::before_poll` / `notify_always` carry libwayland's
  prepare_read / read_events / cancel_read protocol, now inside `Display::attach()`.
  `FdOptions` lives at namespace scope: GCC rejects a nested struct with default member
  initializers as a default argument of the enclosing class.
- `core::WorkerPool`: fixed threads (default half the hardware threads, at least 2: 8 here),
  three FIFO queues by priority (visible, nearby, prefetch), `CancelToken`; results are posted to
  the loop and delivered only if neither the job was cancelled nor the pool destroyed. Destruction
  drops queued jobs and joins running ones.
- `main.cpp`: temporary loop removed. SIGINT/SIGTERM blocked before any thread exists (so no
  worker can receive them), signalfd is a loop source; Window gained a close handler; Seat
  `attach()`es its key repeat timer.
- First unit tests (doctest, Void package `doctest-devel`: `doctest` alone ships only the
  license): 11 cases, 5043 assertions. Loop: post order from another thread, timers, removal from a
  callback, sources added from a callback, before_poll/notify_always. Pool: threads of work and
  delivery, priority order, cancel while queued / while running, destruction with 100 queued jobs,
  2000 jobs across all threads delivered exactly once. A use-after-free in a test (detached thread
  touching a destroyed blocker) was caught in review before the first run.
- Results: Release and ASan pass; ThreadSanitizer 5 runs out of 5 with no warning.
- ROADMAP: new step 7.0, compositor compatibility (cursor fallback, client-side decorations,
  checklist on Umbriel, niri, KDE, GNOME), after the author asked about niri.

**Tested by the author**
- The app behaves as before on the new loop: keys, pointer, Ctrl+Q, close. Step 2.3 closed.

**Open**
- Run on the author's niri VM: postponed by the author, to do before ROADMAP 7.0.

---

## 2026-10-26 — Pointer (ROADMAP 2.2)

**Done**
- `wl_seat` bound at v8 for `axis_value120`. `Seat` gets a `wl_pointer` when the seat has one:
  enter/leave/motion/button delivered at once as `PointerEvent`s (logical surface coordinates,
  modifiers); scroll axes, axis source, axis_stop and value120 gathered until `wl_pointer.frame`
  and delivered as one scroll event (surface pixels + wheel notches + touchpad flag + stop flag
  for future kinetic scrolling).
- Click counting: same button, within 400 ms and 4 px (Wayland shares no double-click setting;
  configurable in 2.4).
- Cursor shapes through `cursor-shape-v1` (bound v1; Umbriel offers v2), set again on every
  enter as the protocol requires. Its XML references `zwp_tablet_tool_v2`, so tablet-v2 glue is
  generated too. The listener fills `axis_relative_direction` (v9) and `warp` (v11) with null:
  libwayland 1.26's headers know them, but they are never sent to a v8 client.
- Test scene: a button with hover color + hand cursor and a click counter, and a status line
  with the last press (button, click count, position) or scroll.
- Verified by Claude: Release/ASan builds clean, tidy clean, no-display path.

**Tested by the author**
- Hover color + hand cursor, left/middle/right presses, multi-click counting (slow clicks or a
  moved pointer restart at 1), wheel: one notch = 15 px and value120 = 1.00 per notch.
  Step 2.2 closed.
- Click counts are unbounded (x10 on rapid clicking): consumers map 1/2/3 to their actions.
- Twice two presses arrived 13 ms apart (179.772 -> 179.785, 180.624 -> 180.637): faster than a
  human double click. Likely switch chatter in the mouse, not Nordend; if it shows up as
  accidental double-clicks in the views, consider a small debounce (< 30 ms) on presses.

---

## 2026-10-26 — Keyboard (ROADMAP 2.1)

**Done**
- `Display` binds the first `wl_seat` (v7, Umbriel offers v9). `Seat` (ui) gets a `wl_keyboard`
  when the seat has one, compiles the compositor's keymap (fd mapped `MAP_PRIVATE`, as required
  since seat v7) with xkbcommon, tracks modifiers, and turns key presses into `KeyEvent`s:
  keysym, UTF-8 text, Ctrl/Shift/Alt/Super, repeat flag. Control characters are not text.
- Dead keys / compose via `xkb_compose` with the locale's table (LC_ALL, LC_CTYPE, LANG).
- Key repeat from `repeat_info` (rate, delay) through a `timerfd`, polled by the temporary loop;
  at most 2 repeats per wake-up so a stalled loop does not burst-type; stops on release and on
  focus loss.
- Test scene shows the last key; Ctrl+Q quits (first keyboard command).
- Verified by Claude: Release/ASan builds clean, tidy clean, no-display path.

**Tested by the author** (Umbriel, layout 'Italian', repeat 25/s after 600 ms)
- è é ò ç à ° ù § + * from the Italian keys, Ctrl+Shift+A without text, arrows/F-keys/Escape
  without text. Repeat measured in the log: first repeat 600 ms after the press, then every
  40 ms (25/s), stops on release. Step 2.1 closed.

---

## 2026-10-26 — Text (ROADMAP 1.5)

**Done**
- `FontSystem` (ui): fontconfig fallback chain for a family (default `sans-serif`, like Noctalia:
  DejaVu Sans here, 184 fallbacks), text split into runs by font coverage, harfbuzz shaping per
  run with the whole text as context, FreeType rasterization (light hinting, grayscale AA) at the
  physical pixel size. Marks, ZWJ, variation selectors, skin-tone modifiers and tags stay in the
  font of the previous character. Emoji-presentation characters (or followed by VS16) prefer a
  color font: fontconfig ranks a monochrome symbol font above Noto Color Emoji, which gave
  grey emoji and a tofu skin-tone modifier before the fix.
- Own UTF-8 decoder (U+FFFD per invalid byte, same policy as harfbuzz) for itemization.
- Truncation at whole clusters: `Ellipsis::end` and `Ellipsis::middle` (Finder-style, keeps the
  extension of a file name).
- Layout cache (8192 entries, cleared when full). Color emoji strikes (CBDT, 109 px) are
  downscaled on the CPU with an area filter.
- Renderer: second pipeline for glyphs (textured instanced quads, coverage R8 + color RGBA8
  atlases 2048x2048, shelf packing, 1 px padding; when an atlas is full, pending glyphs are drawn
  and the cache starts over). Box/glyph batches flush on switch to keep paint order.
- Test scene: sizes 11-32, accents (precomposed and decomposed), emoji with ZWJ family and skin
  tone, CJK (Noto Sans CJK installed by the author), invalid UTF-8, end/middle truncation.
- LeakSanitizer: fontconfig keeps 320 bytes of global state after the first FcFontSort, constant
  with 1 or 5 calls and freed only by FcFini (standalone repro, fontconfig 2.17.1). Suppressed via
  `__lsan_default_suppressions` compiled into sanitizer builds only; FcFini is not called because
  it would break any other fontconfig user in the process.
- Checked by Claude on snapshots at 1, 1.25 (ASan), 1.5 (ASan), 2 (ASan): all samples correct,
  no sanitizer report.
- DESIGN: text stack is harfbuzz + FreeType + fontconfig, no pango/cairo.

**Tested by the author**
- Release and ASan on screen: all samples correct; ASan exit 0, only the fontconfig suppression
  used. Startup to font + GL ready: 38 ms (debug log timestamps). Step 1.5 closed: phase 1 done.

**Open**
- Not done yet: bidirectional reordering (RTL), subpixel glyph positioning, font family from
  config (ROADMAP 2.4).

---

## 2026-10-26 — Drawing primitives (ROADMAP 1.4)

**Done**
- `Renderer` (ui): batched, instanced quads; one shader evaluates the signed distance to a
  rounded box with per-corner radii in physical pixels. 1-pixel antialiasing from coverage,
  inside borders from an inner offset, soft shadows from a Gaussian falloff of the distance
  (erf approximation; exact on straight edges, close at corners). Premultiplied alpha. Up to
  4096 instances per draw call; the buffer is orphaned each flush.
- Logical-pixel API (`Rect`, `Corners`, `Color`, `BoxStyle`); box edges and border widths snap to
  whole physical pixels (a 1 px border is never thinner than one physical pixel).
- Clip stack via scissor (axis-aligned; rounded clipping not needed yet).
- `Window` owns a `Renderer` and calls a draw handler each frame; the temporary
  `NORDEND_SCALE_TEST` pattern is gone.
- Test scene (`src/app/test_scene.cpp`): solid and fractional rects, radii, per-corner radii, pill,
  borders (1, 2, 4 px, border-only), shadows (sigma 4/12/24), translucent overlap, nested clips,
  physical-pixel hairlines.
- `nordend-snapshot`: renders the test scene offscreen through a surfaceless EGL context into an
  FBO and writes a PPM. Lets Claude check rendering without touching the author's screen.
- Checked by Claude on snapshots at scale 1, 1.25 (ASan), 1.5, 2 (ASan): shapes, AA, borders and
  hairlines crisp; no sanitizer report. Found and fixed a paint-order bug in the scene (a wide
  shadow painted over the neighbouring card).

**Tested by the author**
- Release and ASan on screen, with resizing: everything works, exit 0, no sanitizer report.
  Step 1.4 closed.
- Side finding: switching Noctalia to light does not recolor open foot windows. Not an override:
  Noctalia rewrites `~/.config/foot/themes/noctalia` (light palette, in `[colors-dark]`), but foot
  reads its config only at startup; its runtime switch (SIGUSR1/2) only toggles between the
  already-loaded `[colors-dark]`/`[colors-light]`. New foot windows get the new colors. Nordend
  must do better: live reload of `colors.toml` (ROADMAP 2.5).

---

## 2026-10-26 — Fractional scaling (ROADMAP 1.3)

**Done**
- Umbriel's globals listed with a scratch registry dump: it offers `wp_fractional_scale_manager_v1`,
  `wp_viewporter`, `wp_cursor_shape_manager_v1` v2, `zwp_text_input_manager_v3`, `wl_compositor`
  v5 (so no `preferred_buffer_scale`), among others. Author's monitor: DP-3, 2560x1440@59.95,
  scale 1.
- `Display` binds viewporter and fractional-scale (optional). `Window` keeps the scale as
  numerator/120, renders a physical-size buffer (logical x scale, rounded half away from zero)
  and sets the viewport destination to the logical size on every frame.
- Temporary `NORDEND_SCALE_TEST=1`: 1-physical-pixel line pattern to check that the buffer maps
  1:1 onto the screen. Removed when ROADMAP 1.4 brings a real test scene.

**Tested by the author** (Umbriel `[output.DP-3] scale = ...`, `NORDEND_SCALE_TEST=1`)
- 1.25: 2048x1152 logical -> 2560x1440 buffer fullscreen; 1.5: 837x878 -> 1256x1317 (1255.5 rounded
  half away from zero); 2.0: 624x638 -> 1248x1276. Lines crisp, no moiré. Step 1.3 closed.
- Interactive resize logs frames ~16.7 ms apart: one frame per refresh at 60 Hz, as designed.
- A shimmer of the 1-px line pattern while resizing from a left corner: the pattern is anchored to
  the moving left edge; an optical effect of the test pattern, re-check with real content in 1.4.
- Umbriel config (void-noctalia + deployed): `[appearance.title_bar] mode = "on"` for every window;
  the Nordend-only `title_bar = "forced"` rule removed as redundant.

**Open**
- No integer-scale fallback (`wl_output.scale` / `preferred_buffer_scale`) for compositors without
  fractional-scale: those render at scale 1. Add it if Nordend ever targets such a compositor.

---

## 2026-10-26 — EGL rendering and frame pacing (ROADMAP 1.2)

**Done**
- `EglContext`: platform display from the wl_display, opaque RGB888 config, OpenGL ES 3.0
  context (ES 3 for instancing and R8 textures later; DESIGN updated).
- `Window` renders with `wl_egl_window` + EGL surface; the shm buffer and the `wl_shm` binding
  are gone.
- Frame pacing: `eglSwapInterval(0)` so swaps never block the loop; at most one frame in flight,
  the next one waits for the frame callback; frames only when dirty (configure or
  `request_redraw`). Configure acks are applied by the next frame, so interactive resize runs at
  display rate.
- EGL attribute tables use `// clang-format off`: clang-format cannot lay out key/value tables.
- Verified by Claude: Release and ASan builds without warnings, tidy/format clean, no-display path.

**Tested by the author**
- "Everything works, no lag": interactive resize now smooth. Step 1.2 closed.
- GL: AMD Radeon RX 6650 XT (radeonsi, navi23, ACO), OpenGL ES 3.2, Mesa 26.2.5, EGL 1.5.
- Startup to GL context ready: 30 ms (from the debug log timestamps; not yet a proper benchmark).

**Open**
- A hidden window gets no frame callbacks, so a configure arriving while hidden is acked but
  committed only when it becomes visible again. Fine for now; revisit if Umbriel complains.

---

## 2026-10-26 — First Wayland window (ROADMAP 1.1, in progress)

**Done**
- Module layout as in quark++: `src/<module>/include/nordend/<module>/`, one static library per
  module. `core`: stderr logger, level from `$NORDEND_LOG`. `ui`: `Display` (connection, registry,
  globals bound at capped versions), `Window` (xdg-shell toplevel, server-side decoration request,
  configure/resize, close), temporary `ShmBuffer` for a solid background until EGL (1.2).
- wayland-scanner generates xdg-shell and xdg-decoration glue at build time.
- `WlPtr<T>`: RAII for proxies. The scanner's `*_destroy` helpers are `static inline`; used as
  template arguments they gave the owning types internal linkage (`-Wsubobject-linkage`), so the
  deleters are out-of-line overloads in `wl_ptr.cpp`.
- Temporary event loop in `main.cpp`: libwayland prepare_read/poll protocol + signalfd, so
  Ctrl+C/SIGTERM exit through every destructor.
- Verified by Claude: build without warnings, clang-tidy and clang-format clean, no-display error
  path (exit 1), SIGTERM path (exit 0).

**Mistake**
- A test meant to run without a display unset `WAYLAND_DISPLAY`; libwayland falls back to
  `wayland-0`, so a window opened on the author's screen (closed with SIGTERM). Rule added to
  CLAUDE.md: use `WAYLAND_DISPLAY=nordend-test-none`.

**Tested by the author on Umbriel**
- Window, server-side decorations, resize, close button: OK.
- No title bar by default: Umbriel is a scrolling tiler; `Mod+W` (`column-toggle-tabbed` in the
  author's Umbriel config) shows the tab with the "Nordend" title. Expected, not a bug.

- ASan/UBSan build (after installing `libsanitizer-devel`): maximize, fullscreen, interactive
  resize as a floating window with a forced title bar, close button: no sanitizer report, exit 0.
  Step 1.1 closed.
- Umbriel config (in void-noctalia): window rule `match.app_id = "^nordend$"`, `title_bar = "forced"`
  for testing; works.

**Measured: resize lag**
- The author saw lag while interactively resizing (Firefox does not lag). Cost of one
  `ShmBuffer` at 2560x1440 (memfd + ftruncate + mmap + fill + munmap, 50 runs, scratch benchmark):
  **14.1 ms Release, 41.5 ms ASan -O0**. A new buffer is made for every configure during a resize,
  with no frame pacing, so in the ASan build that is ~24 fps at best.
- Not fixing the shm path, which is temporary: ROADMAP 1.2 replaces it with EGL
  (`wl_egl_window_resize` + GPU clear) and frame callbacks. Re-measure resize there.

---

## 2026-10-26 — CMake skeleton (ROADMAP 0.1, 0.3)

**Done**
- DESIGN approved by the author.
- `CMakeLists.txt`: C++20, Release by default, `compile_commands.json`, `-Werror` with a strict
  warning set (conversion, sign, old-style casts, null deref, ...), `NORDEND_SANITIZER` and
  `NORDEND_CLANG_TIDY` options. wayland-client, wayland-egl, egl, glesv2 and xkbcommon found via
  pkg-config (not linked yet).
- `src/app/main.cpp`: exits 0, answers `--version`.
- Verified: Release build with no warnings, exit 0, `--version`, clang-format clean, clang-tidy clean.

**Open**
- Sanitizer build fails to link: `libasan` / `libubsan` runtimes not installed. Needed before
  ROADMAP 1.1 (package `libsanitizer-devel`, to be verified).

---

## 2026-10-26 — Project kickoff

**Done**
- Surveyed the installed stack of Noctalia (wayland-client, EGL/GLESv2, harfbuzz/pango, toml++,
  Luau, fzy, wuffs, libwebp, libjxl; MIT) and Umbriel (wlroots 0.20, Lua 5.4; MIT).
- Drafted `docs/DESIGN.md`; created `docs/ROADMAP.md`, `docs/buglog.md`, this devlog, `README.md`
  and `.gitignore`.
- Initialized git (`main`, no remote), MIT license, `.editorconfig`; copied the quark++ style
  (`.clang-format`, `.clang-tidy`, `.clangd`, `scripts/format.sh`) with includes regrouped for `nordend/`.
- Toolchain: clang-format / clang-tidy 21.1.8 installed (`clang-tools-extra`); `.clang-format` parses.
  The default Void mirror (Helsinki, `repo-default`) timed out on HTTPS while ping and port 80 worked:
  switched to `repo-de` via `/etc/xbps.d/00-repository-main.conf`.

**Decisions**
- C++20, CMake, stack aligned with Noctalia.
- Finder-style column view (Miller columns) + grid.
- Server-side decorations.
- `nordend` color template proposed upstream to Noctalia as a PR.
- Future scripting in Luau.
- External editor (helix) moved to the end: not a must-have.
- Languages: code, comments, commits, README, buglog and devlog in English; DESIGN and ROADMAP in Italian.
- Claude is credited in the README as copilot, not listed as a commit co-author. `CLAUDE.md` stays
  out of the repository.
- Nordend's code is written from scratch in C++; copying from Noctalia/Umbriel only case by case,
  always announced to the author first and tracked in `THIRD_PARTY.md`.
- GitHub: private `pankaspe/nordend-fm`; repo-local noreply email (account email privacy).

**Open**
- DESIGN approval (ROADMAP 0.1).
