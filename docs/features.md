# Nordend: every feature

Everything Nordend does today, by area. Numbers are measurements from
[benchmarks.md](benchmarks.md), taken on a Ryzen 7 5700G with an RX 6650 XT; how each feature came
to be is in the [devlog](devlog.md), and what comes next is in the [roadmap](ROADMAP.md).

## What it is

- A file manager written from scratch in C++20 for Wayland: its own OpenGL ES renderer for
  boxes, text, icons and images. **No GTK, no Qt, no GLib.**
- Part of the Noctalia family (Noctalia Shell, the Umbriel compositor): it wears the Noctalia
  theme and the same line icons, and Umbriel draws its title bar. Elsewhere (GNOME, weston)
  Nordend draws its own.
- One window thread, a pool of workers for everything that touches the disk, and **zero CPU when
  nothing moves**: no frame is drawn unless something changed (0 ticks in 30 s, measured).

## Three views of the same folder

- **Columns** (Ctrl+1), as in Finder: each folder you pick opens in the next column, and the last
  column shows the selected file. Left from the first column climbs to the parent. Columns are
  resized by dragging their edge (a double click on the edge resets it), and the row slides
  sideways by itself to keep the focused column in sight.
- **Grid** (Ctrl+2): large icons and thumbnails. Ctrl+plus, Ctrl+minus or Ctrl+wheel resize the
  cells from 48 to 256 px, and Ctrl+0 goes back to the usual size.
- **Details** (Ctrl+3): a table of name, size, date and type. Click a title to sort by it and
  click again to reverse; drag a column's edge to widen it. By size, folders keep their name
  order; by type, unknown types come last. The order you choose is used in the other views too.
- **One folder, three ways of looking at it.** Switching views keeps the cursor, the selection and
  the file types already detected. The status bar has a button for each view.

## Moving and selecting

- **Natural scrolling.** On a touchpad the list follows the fingers 1:1, keeps gliding when they
  lift, slows down smoothly and stops softly at the ends. A mouse wheel glides instead of jumping,
  and quick turns add up. The arrows and Page Up/Down glide; Home and End jump.
- **Keyboard first:** arrows, Page Up/Down, Home and End; Enter, Right or Alt+Down to go in;
  Backspace, Left or Alt+Up to go back. The cursor lands on the folder you came from.
- **History:** Alt+Left and Alt+Right, or the mouse's back and forward buttons, go through the
  folders opened (the last 100), each with the cursor where it was left. Opening a folder after
  going back drops the ones ahead; a folder deleted meanwhile is skipped, with a message.
- **Hidden files out of the way:** dotfiles are hidden at start; Ctrl+H shows them (dimmed) in
  every view and hides them again. Hidden, they are never selected, so Ctrl+A and Delete touch
  only what you see, and the status bar says how many there are ("12 items, 3 hidden"). A hidden
  folder you open (from a bookmark, say) stays in sight on the way to it.
- **Selection as you expect it:** click, Ctrl+click and Shift+click; Shift+arrows for a range,
  Ctrl+arrows and Ctrl+Space to pick here and there, Ctrl+A for all, Escape for one again.
- **The selection follows its files:** cursor and selection are kept by name, so they stay on a
  file that another program renames or that moves in the order.
- **Every shortcut on one card:** F1 or "?" lists them all, from the same table the code runs on,
  and a test checks that the card and the behaviour agree.

## Photos first

- **Thumbnails in a fraction of a second.** Nordend reads the previews that cameras already store
  inside their raw files instead of developing the raw. A screenful of photos is ready in
  0.15-0.4 s the first time and in 12-22 ms from the cache. One thumbnail takes 12 ms from an
  OM-1 JPEG and 36 ms from an ORF.
- **Raw files from many cameras:** OM System and Olympus ORF (including the maker-note preview),
  Canon CR2, Nikon NEF and TIFF, Sony ARW, Fujifilm RAF, Panasonic RW2 (cropped to the shot's
  aspect ratio), Hasselblad 3FR. Each is shown upright, in the shape it was shot, without the
  black bars some cameras put on their thumbnails.
- **Images:** PNG, JPEG and GIF (wuffs), WebP (libwebp), JPEG XL (libjxl), with EXIF orientation.
- **Shared freedesktop thumbnail cache:** thumbnails go where GNOME, KDE and other apps look for
  them, byte-compatible, so they are made once for all of them. Files that fail are remembered.
- **Quick Look** (Space): the photo fills the window, raw files included. An ORF fitted to a
  2880x1620 screen takes 51-55 ms. The arrows walk the folder without closing it, and the next
  photo is already loading. Text files open on a page you can scroll, in your monospace font
  (darktable sidecars included); any other file shows what it is.
- **The info pane** beside every view shows the preview and what the file is: size, pixel size,
  dates (including the birth time), type, and for photos the shot itself (camera, lens,
  exposure, aperture, ISO, focal length, when) with your darktable rating from the `.xmp`
  sidecar. "More details" adds permissions, owner and group, link target and item count. In the
  grid and the details Ctrl+I hides it, and its edge can be dragged from 240 to 720 px.
- **darktable-aware:** sidecars get their own icon, show your rating, and follow their photo when
  it is renamed.

## Working with files

- **Opens in the right app.** Enter or a double click opens a file in its default application,
  chosen exactly as GTK apps choose it: 0 differences from `gio` on all 1 232 file types and
  aliases. Nordend implements the freedesktop specs itself, without `xdg-open`:
  - several files for the same app start one instance (five photos, one darktable);
  - terminal apps such as Helix open in a terminal (`$TERMINAL`, `xdg-terminal-exec`, foot,
    kitty, ghostty, alacritty, wezterm...);
  - the new window comes up in front, through an xdg-activation token;
  - apps run detached: no zombies, and closing Nordend leaves them open;
  - an executable is never run: it opens in the app for its type;
  - the status bar says what happened ("Opening 5 files with Darktable…"), in red when there is
    no app for a type or the app fails at once.
- **Rename in place** (F2): the name becomes a field where it is, in all three views, with the
  name selected but not its extension. Typing works with accents and compose. Enter keeps the
  name, Escape gives up, and a click elsewhere keeps it. The usual editing keys work, including
  words with Ctrl+arrows and selection with Shift; Ctrl+V pastes the clipboard's text (its first
  line); Ctrl+Z undoes and Ctrl+Shift+Z redoes (a word typed, characters deleted one after
  another, or a paste is one step; up to 100).
- **New folder** (Ctrl+Shift+N): "New Folder" (or "New Folder 2"...), ready to be named.
- **Never overwrites:** renames use `RENAME_NOREPLACE`, so an existing file is never replaced,
  even one that appears in the meantime. A taken or invalid name is refused with a message and
  the field stays open for another try.
- **Photo-safe renames:** renaming `P1.ORF` also renames `P1.ORF.xmp` and the duplicates
  `P1_01.ORF.xmp`, `P1_02.ORF.xmp`… If any of their new names is taken, nothing moves.
- **Copy and move** (Ctrl+C or Ctrl+X, then Ctrl+V), in any of the three views:
  - a move on one disk is a rename: instant, whatever the size;
  - a copy runs as fast as `cp` (3.1 GB of photos in 1.39 s, warm cache, against 1.39 s), by
    the kernel's `copy_file_range`, on a thread of its own, so the window never waits;
  - the status bar shows "Copying 163 files · 2.1 GB of 3.4 GB · 180 MB/s · 8 s left", a
    progress line and an × to stop; what arrived is selected afterwards;
  - nothing half-copied ever carries a real name: each file is written under a hidden temporary
    name and renamed when complete, and a folder is built whole the same way. Stopping leaves
    nothing behind;
  - moving off another disk (an SD card, a USB drive) deletes the original only after its copy is
    flushed to disk;
  - dates (to the nanosecond), permissions and symbolic links are kept, and darktable sidecars
    travel with their photo, even under a new name;
  - cut files fade until pasted (Escape keeps them where they are);
  - a taken name asks on a card: Replace (the old file goes only once the new one is complete;
    folders are merged), Skip, Keep both ("P1 2.ORF", sidecars included), for this one or for
    the rest; pasting into the same folder keeps both by itself;
  - refused before anything moves: a folder into itself, or more than the free space.
- **Undo** (Ctrl+Z, in any view and in Quick Look) the last operation on files, and redo it
  (Ctrl+Shift+Z):
  - a rename gets its old name back, darktable sidecars included; a move goes back where it came
    from, across disks too; a Delete is restored; a restore goes back to the trash;
  - copies and new folders or files go to the trash, never deleted, so even an undo can be
    undone; a new folder only while it is empty, a new file only while it is as it was made;
  - a permanent delete cannot be undone, and the status bar says so; what changed meanwhile
    (a file another program moved) is skipped and said;
  - the status bar says what happened: "Undone: “Lake.ORF” is “P1.ORF” again", "Undone: 3
    items moved to the trash", "Redone: “P1.ORF” back in album".
- **Drag and drop:** a press on an entry moved a few pixels drags it (the whole selection when it
  is in it), with its icon and name under the pointer. It drops onto a folder in any view (marked),
  the empty part of a column or view (that folder), a step of the path above or a place in the
  sidebar; the Trash trashes. On the same disk it moves, onto another it copies; Ctrl copies,
  Shift moves. It goes through the same engine as a paste: sidecars, questions, progress, Ctrl+Z.
  A folder never goes into itself, and dropping where the files already are does nothing. Other
  programs receive the files as file URIs and paths (Firefox, terminals), only to copy: Nordend
  never deletes its files because another program says it took them. Files dragged from another
  program (Firefox's downloads) are copied; a web address is not a file. Near the top or bottom of
  a list the drag scrolls it. Opening a folder by hovering over it comes after the first release.
- **The system clipboard**, with other programs:
  - files copied or cut in Nordend paste in other programs: a file manager, Firefox (an upload, a
    message), a terminal or an editor (their paths); Nordend offers them as
    `x-special/gnome-copied-files`, `text/uri-list` (each file URI as GLib writes it) and text,
    and says "cut" the GNOME and the KDE way;
  - files copied or cut in another program paste in Nordend with Ctrl+V: copied, or moved when
    the other program cut them, through the same engine (sidecars, conflicts, progress); only
    files on this computer are taken;
  - read and written through pipes the window's loop watches: a program that does not answer
    never freezes Nordend (it is given up after 3 s);
  - when another program copies something, the files Nordend cut stop fading: Ctrl+V pastes what
    is on the clipboard now. A cut pasted once empties the clipboard.
- **The Trash** in the sidebar (its icon full or empty):
  - every trash as one folder: the home's and each disk's (`.Trash-1000` on a data disk), by
    original name; the same name trashed twice shows as "a.txt" and "a.2.txt";
  - the info pane adds where it came from and when it was deleted; previews, thumbnails and
    Quick Look work as anywhere else; it follows what other programs trash or restore, live;
  - Restore (right click) puts things back where they were, makes their folder again if it is
    gone, asks on the card when the name is taken (a folder is never merged), and brings a
    photo's darktable sidecars back with it;
  - Delete (or "Delete Permanently…" in the menu) deletes the selection for good, a photo with
    its trashed darktable sidecars; "Empty Trash…" (on the trash's empty space, or a right click
    on Trash in the sidebar) empties every trash at once, orphaned `.trashinfo` files and
    `directorysizes` included; both after the confirmation card;
  - nothing is made, renamed or pasted there, and files are not opened from the trash.
- **Context menu**: a right click (or a two-finger tap on the touchpad) in any of the three views
  or on the info pane, the Menu key or Shift+F10:
  - on files and folders: Open, Open With, Copy, Cut, Paste, Rename (one entry), Move to Trash,
    Delete Permanently;
    on a folder's empty space: New Folder, New File, Paste; what cannot be done now is dimmed,
    and each item shows its shortcut;
  - a right click on an entry selects it first, unless it is part of the selection already;
  - Open With lists the applications for the file's type exactly as GTK apps do (0 differences
    from `gio mime` on all 1 232 types and aliases), the default one first, and opens every
    selected file with the one chosen;
  - New File makes an empty file, or a copy of one of your templates (`XDG_TEMPLATES_DIR`),
    never over an existing file, with the name ready to edit;
  - drawn by Nordend in your theme's colors, it stays whole near the window's edges, and the
    keyboard works in it: arrows, Enter, → into a submenu, ← and Escape back.
- **Empty files open as their name says**: a new, empty `notes.txt` opens in your text editor
  (GLib calls any empty file `application/x-zerosize`, which no application opens).
- **Trash** (Delete), the freedesktop Trash specification 1.0, in any of the three views and in
  Quick Look:
  - files on the home's disk go to `~/.local/share/Trash`, files on other disks to that disk's
    own trash (`.Trash-1000` at its top, or `.Trash/1000`): a rename, never a copy, so trashing
    is instant (1 001 items in 25 ms, `gio trash` 70 ms); where no trash can be made, the files
    stay and the status bar says why;
  - other trash programs (`trash-list`, `trash-restore`, gio, file managers) see what Nordend
    trashed and restore it to its place: the `.trashinfo` files are the same as GLib's, odd
    names included;
  - darktable sidecars go with their photo, each restorable on its own;
  - the cursor moves on to the next entry; the status bar says what went ("“P2.ORF” with 1
    darktable sidecar moved to the trash") or what could not;
  - trashed folders are listed in `directorysizes` with their size, so trash tools need not
    measure them again.
- **Delete permanently** (Shift+Delete, in any view and in Quick Look; Delete in the Trash):
  - first a card says what goes: "Delete “P1.ORF” permanently?", "With its darktable sidecar ·
    21.0 MB", and for folders how many files and folders they hold in all; the size is added up
    on a worker, the card is there at once; Enter (or the red Delete) deletes, Escape (or Cancel)
    keeps everything;
  - on the transfer thread, with progress in the status bar and its × to stop; links are never
    followed, read-only folders of yours are opened up to be emptied; darktable sidecars go with
    their photo, and a photo that stays keeps them;
  - 10 101 files and folders in 0.20 s on ext4 (`rm -rf` 0.17 s: the difference is the first
    pass that counts them for the progress bar).
- **Clear errors:** a read-only folder, permission denied or a missing file shows a message in the
  status bar, never a crash.

## Finding your way

- **Sidebar:** your home and its folders (Downloads, Pictures… in your language, from
  `user-dirs.dirs`), the file system, your GTK bookmarks (shared with other file managers) and
  the mounted disks. A disk appears as soon as it is mounted and disappears when it is unmounted.
  Ctrl+B hides the sidebar, and F6 moves the keyboard into it and back.
- **Disks:** through udisks2, a disk plugged in appears among the devices, quieter, without
  mounting itself (encrypted ones come later); a click mounts it and opens it, and a disk that
  mounts read-only says why (an NTFS disk not closed properly) and what fixes it. The eject icon of
  an external disk unmounts every partition on it and powers it off, taking the view home first if
  it shows that disk; "in use" says to close what is open there. The right click mounts, unmounts
  and ejects. The list follows disks coming and going; nothing waits on udisks2.
- **Favourites:** Ctrl+D adds the folder shown, "Add to Sidebar" the folders selected (or the
  folder, from its empty part), and folders dragged onto the "BOOKMARKS" title or below the last
  place are added at the end, a line showing where; dropping onto a place still moves into it.
  A right click on a favourite removes it. They are the GTK bookmarks: other file managers and
  the Open dialogs of other apps (Firefox's) show them too. The file is written all at once on a
  worker, keeping what Nordend does not touch (remote bookmarks, labels, order); a folder already
  there is not added twice. Reordering and renaming come after the first release.
- **Breadcrumb:** the folder's path as a row of clickable steps, starting from your home. A long
  path is shortened from the left.
- **Status bar:** item count, selection, free space on the disk (updated after a copy into the
  folder), the three view buttons and "?". Messages appear here for a few seconds.

## Always current

- **Live folders:** files created, removed, changed or renamed by any program show up at once
  (inotify). A thousand-file copy arrives in a few smooth updates, not a thousand.
- **File types exactly as GTK sees them:** the freedesktop shared-mime-info database, read by
  name first and by content only when needed (and only for what is on screen). 0 differences from
  `gio` on about 4 750 files.
- **Live theme:** change the palette in Noctalia and Nordend follows instantly; elsewhere, switch
  the desktop between light and dark or pick another accent colour and Nordend follows too.

## Looks

- Noctalia's colors and the same line icons as Noctalia (Tabler), tinted by the theme; camera
  raw files and darktable sidecars have their own icons. The icons are compiled into the
  program (a 14 KB subset of the official Tabler font), so they show on any desktop.
- Sharp at any scale: fractional scaling (1, 1.25, 1.5, 2…), boxes snapped to the pixel grid.
- Text shaped by HarfBuzz, with font fallback for accents, emoji and CJK. Your font family and
  monospace font come from `config.toml`.
- **Its own title bar where the compositor draws none** (GNOME, weston): the title in the middle,
  minimize, maximize (restore when maximized) and close at the right, in the theme's colors;
  drag it to move the window, double-click to maximize, right-click for the compositor's window
  menu; the window's edges and corners resize it, with the matching arrows (not when maximized,
  nor on an edge tiled against the screen). Buttons the compositor cannot honour are left out.
  `NORDEND_FORCE_CSD=1` asks any compositor for it (the compositor may still decorate).
- Pointer shapes from the compositor's cursor theme (cursor-shape-v1): a resize arrow on every
  draggable edge, a hand on what a click acts on (the status bar's buttons, a transfer's ×, the
  path's crumbs, a disk's eject icon, "More details", the buttons of a question), the text cursor
  over a name being edited. Where the compositor has no cursor-shape-v1 (GNOME, weston), Nordend
  draws them itself from the xcursor theme (`XCURSOR_THEME`, `XCURSOR_SIZE`, sharp at any
  scale); `NORDEND_FORCE_XCURSOR=1` does so on any compositor.

## Fast, and measured

| What | Time |
|------|-----:|
| First entries of a 10 000-file folder (warm / cold cache) | 0.8 ms / 1.7 ms |
| The whole folder loaded and sorted (warm / cold) | 19.5 ms / 81 ms |
| A scrolling frame at 2880x1620 (columns / grid / details) | 0.26 / 0.30 / 0.36-0.50 ms |
| Sorting 10 000 entries by size, date or type | 1-3 ms |
| Quick Look of an ORF, fitted to 2880x1620 | 51-55 ms |
| Reading every installed app to choose one | 0.5 ms |
| Copying 342 photos, 3.1 GB, warm cache (Nordend / `cp -a`) | 1.39 s / 1.39 s |
| CPU while nothing moves | 0 |

A 2.5 MB binary with 18 direct libraries; 25 MB of its own memory at start, 54 MB after browsing
163 raw photos (143 MB resident with the shared libraries): decoded photos go back to the system.

## Settings

- `~/.config/nordend/config.toml`: font family, monospace font, double-click time and distance
  ([example](config.example.toml)). `nordend --check-config` reports every problem with its
  line.
- **Settings panel** (Ctrl+, or the gear in the status bar): the fonts and the double click,
  changed in place. Only the value's own line in `config.toml` changes: comments, order and
  unknown keys stay, a missing key goes into its section. A value out of range is refused with
  the same message `--check-config` gives, and a file that no longer parses is never
  overwritten. The double click changes at once, the fonts at the next start (the panel says
  so). `config.toml` is followed while Nordend runs, also when edited by hand. The panel also
  tells where the colours come from (Noctalia, the desktop, Nordend's own).
- **Choosing a font:** while a font is changed, the installed families are listed under it,
  narrowed as you type (case aside), the generic names first and, for the monospace font, the
  fixed-width families ("mono"); arrows or a click pick one. A name that is not installed is
  refused with "choose one from the list" ("No installed font matches" when nothing does); a
  fontconfig pattern with properties ("Inter:weight=medium") is taken as written.
- Colors come from Noctalia (`~/.config/nordend/colors.toml`, written by Noctalia's `nordend`
  template). Without the file, Nordend follows the desktop through the freedesktop portal (GNOME,
  Plasma): light or dark, and the accent colour for the selection, icons and links, with
  neutral surfaces and text kept readable (WCAG AA contrast, tested for 15 accents). Without a
  portal either, a built-in default palette is used.
- **Remembered from one start to the next** (`~/.local/state/nordend/state.toml`, written by
  Nordend): the view, the details' order and column widths, hidden files shown or not, the
  sidebar and the info pane (shown, width, "More details"), the window's size and whether it was
  maximized. Not the last folder (Nordend opens Home, or the folder given) nor the back/forward
  history. A broken file only means the defaults.

## Standards it follows

freedesktop thumbnails, shared-mime-info, MIME applications associations, Desktop Entry, XDG
base and user directories, GTK bookmarks; Wayland xdg-shell, xdg-decoration, fractional-scale,
viewporter, cursor-shape and xdg-activation.

## Next

An "About" section in the settings, then benchmarks, a week of daily use and the first
release. See the
[roadmap](ROADMAP.md).
