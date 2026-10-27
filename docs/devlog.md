# Devlog

Work log, newest first. Each entry: what was done, decisions, measurements, open points.

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
