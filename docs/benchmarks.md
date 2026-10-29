# Benchmarks

Nordend's performance targets ([DESIGN](DESIGN.md), "Obiettivi di performance") are measured
here, not assumed. Every number below comes from a tool in this repository, so it can be run
again. Newest results first in each section; the [devlog](devlog.md) tells the story around them.

## Machine

| | |
|---|---|
| CPU | AMD Ryzen 7 5700G (8 cores, 16 threads) |
| RAM | 16 GB |
| System disk | Samsung SSD 980 500 GB (NVMe), btrfs |
| Data disk | SPCC Solid State Disk 1 TB (SATA), ext4 |
| OS | Void Linux (glibc), kernel 6.18 |
| Build | GCC 14.2, CMake Release (`-O3`) |

## Binary

### 2026-10-29

After ROADMAP 6.1 (default applications): 2 254 728 bytes; 1 918 968 bytes after `strip`
(+275 KB, mostly `std::format` and `std::filesystem` code at `-O3`); 18 direct libraries, the
same as before.

### 2026-10-28

`build/nordend`, CMake Release (`-O3`), GCC 14.2, x86-64, after ROADMAP 5.11: 1 979 504 bytes;
1 677 048 bytes after `strip`. `readelf -d` lists 18 direct libraries (libwebp, libwebpdemux,
libjxl, libexiv2, libz, libwayland-egl, libwayland-client, libEGL, libGLESv2, libxkbcommon,
libharfbuzz, libfontconfig, libfreetype, libtomlplusplus, libstdc++, libm, libgcc_s, libc);
`ldd` resolves 37 in all.

## Memory

### 2026-10-28

The app running on screen (Umbriel, RX 6650 XT, radeonsi), started on a source folder in the
columns, 69 s after start (`/proc/PID/status` read by the author, `smaps_rollup` and `smaps`
summed per file):

| | |
|---|---:|
| Resident (VmRSS) | 117 068 kB |
| Private, anonymous (heap, glyph atlases, listings) | 32 796 kB |
| File-backed (libraries, fonts) | 84 264 kB |
| Proportional (PSS: shared pages divided among their users) | 52 438 kB |

The largest file-backed pieces: libLLVM 51.4 MB and libgallium 15.3 MB (Mesa's driver, the same
pages for every OpenGL process), libexiv2 3.5 MB, libstdc++ 2.5 MB, the `nordend` binary 1.5 MB.
A folder of photos in the grid holds more (thumbnail textures live on the GPU, *to be
measured*).

## Directory loading (ROADMAP 3.1)

**Target:** a 10k-file folder draws its first frame in < 50 ms; the full list loads in the
background.

**Tool:** `nordend-bench-dir` (`src/tools/bench_dir.cpp`).

```sh
./build/nordend-bench-dir --make 10000 DIR   # camera-like folder: ORF, ORF.xmp, JPG, subfolders
./build/nordend-bench-dir --runs 20 DIR      # warm: median of runs 2..20
sync; echo 3 | sudo tee /proc/sys/vm/drop_caches
./build/nordend-bench-dir --runs 5 DIR       # cold: the "run 1" row
```

Columns, in milliseconds:

- **1st chunk:** from the request to the first chunk (128 entries) sorted into the list on the UI
  thread, through the worker pool. This is what bounds the first frame.
- **complete:** the same request until the whole directory is in the list, sorted.
- **readdir:** reading the names alone (baseline).
- **+stat:** names plus type, size, date and link target for each entry, on one thread.
- **sort:** sorting all entries by name (natural order, directories first).

### 2026-10-26: 10k files on the SATA SSD, warm cache

20 runs, 8 worker threads.

| ms | readdir | +stat | sort | 1st chunk | complete |
|----|--------:|------:|-----:|----------:|---------:|
| min | 3.65 | 17.44 | 3.44 | 0.66 | 18.77 |
| **median** | **3.69** | **18.01** | **3.47** | **0.80** | **19.51** |
| max | 4.08 | 19.69 | 3.69 | 0.92 | 21.79 |

### 2026-10-26: 10k files on the SATA SSD, cold cache

After `drop_caches`, run 1 (the async path runs first, so its columns are the cold ones; the
others are warm by then).

| ms | 1st chunk | complete |
|----|----------:|---------:|
| cold | **1.72** | **81.27** |
| warm (median, same session) | 0.75 | 19.59 |

An earlier cold run, with the synchronous read first, gave **79.09 ms** for readdir + stat: the
same cost without the worker pool and the sorted merge.

### 2026-10-26: other folders, warm cache

Median of 20 runs.

| Folder | Entries | readdir | +stat | sort | 1st chunk | complete |
|--------|--------:|--------:|------:|-----:|----------:|---------:|
| generated 10k, tmpfs | 10 000 | 2.08 | 15.44 | 3.61 | 0.52 | 16.93 |
| `/usr/lib`, NVMe btrfs | 1 862 | 0.51 | 4.49 | 0.49 | 0.77 | 5.19 |
| `/usr/bin`, NVMe btrfs | 1 493 | 0.40 | 2.89 | 0.31 | 0.79 | 3.26 |
| a photo folder, SATA SSD ext4 | 327 | 0.13 | 0.52 | 0.07 | 0.40 | 0.75 |

### Reading the numbers

- The first chunk is ready in under 1 ms warm: the 50 ms target leaves plenty of room for
  layout and drawing.
- `stat` is most of the cost: about 1.4 µs per entry warm on the SSD, more for symbolic links
  (`/usr/lib`), which need a second `stat` for their target.
- A cold cache multiplies the cost of the whole folder by about 4 (19.6 → 81 ms for 10k), but
  the first chunk still arrives in 1.7 ms: chunked loading keeps disk latency off the first
  frame, and the rest arrives while the view is already on screen.
- Sorting 10k names is about 3.5 ms; merging chunks as they arrive costs little on top
  (complete ≈ +stat + 1.5 ms).

## List scrolling (ROADMAP 4.1)

**Target:** scrolling at the monitor's full rate with no dropped frame (DESIGN), in folders of
any size.

**Tool:** `nordend-snapshot --list DIR --bench FRAMES out.ppm [w h scale]` (replaced by
`--columns` with ROADMAP 4.2, see below): draws the list
offscreen while scrolling it from top to bottom at constant speed over FRAMES frames (about 17
new rows per frame for 10k entries), waiting for the GPU (`glFinish`) after each one.

### 2026-10-26

| List | Buffer | median | p95 | max |
|------|--------|-------:|----:|----:|
| 10 000 entries | 960x640 | 0.20 ms | 0.28 ms | 5.6 ms |
| 10 000 entries | 2880x1620 (1920x1080 at scale 1.5) | 0.25 ms | 0.45 ms | 1.5 ms |
| 123 entries (a photo folder) | 960x640 | 0.17 ms | 0.21 ms | 5.9 ms |

The maximum is the first frame, when glyphs are rasterized for the first time. Only visible
rows are drawn, so the cost does not depend on the folder size: a 60 Hz frame (16.7 ms) leaves
the list about 1.5 % of it.

## Column view scrolling (ROADMAP 4.2)

**Tool:** `nordend-snapshot --columns PARENT --reveal DIR --bench FRAMES out.ppm [w h scale]`:
opens the columns down to DIR, then scrolls the last column (DIR) from top to bottom over FRAMES
frames, drawing the whole view (title, every visible column) and waiting for the GPU after each
frame, as for the list.

### 2026-10-26

| Last column | Buffer | median | p95 | max |
|-------------|--------|-------:|----:|----:|
| 10 000 entries (2 columns on screen) | 960x640 | 0.19 ms | 0.28 ms | 6.2 ms |
| 10 000 entries (2 columns on screen) | 2880x1620 (1920x1080 at scale 1.5) | 0.23 ms | 0.44 ms | 1.2 ms |

Same cost as the single list: the columns draw only their visible rows, and the maximum is
again the first frame's glyph rasterization.

### 2026-10-26: with a selection (ROADMAP 4.3)

Same benchmark, before and after `--keys "Right,Ctrl+a"` (all 10 000 entries selected: every
visible row gets a filled box). Two runs each, at 2880x1620 (1920x1080 at scale 1.5).

| Last column | median | p95 | max |
|-------------|-------:|----:|----:|
| nothing selected | 0.26 ms | 0.45 ms | 1.5 ms |
| all 10 000 selected | 0.52 ms | 0.62-0.68 ms | 2.0 ms |

Selected rows cost their fill (pixels, not entries): the worst case is 3 % of a 60 Hz frame.
(The unselected median is 0.26 ms here against 0.23 ms in the first run: same code path, run
to run spread.)

## Grid scrolling (ROADMAP 4.4)

**Tool:** `nordend-snapshot --columns PARENT --reveal DIR --keys "Right,Ctrl+2[,...]" --bench
FRAMES out.ppm [w h scale]`: switches to the grid on DIR (with `Ctrl+minus`/`Ctrl+plus` for
other sizes, `Ctrl+a` to select everything), then scrolls it from top to bottom over FRAMES
frames, drawing the whole view and waiting for the GPU after each frame.

### 2026-10-27

The generated 10 000-entry folder, 600 frames at 2880x1620 (1920x1080 at scale 1.5), two runs
each (the spread between runs is in the ranges).

| Icon size (cells on screen) | Selected | median | p95 | max |
|-----------------------------|----------|-------:|----:|----:|
| 96 px, default (12 x 8) | nothing | 0.32-0.33 ms | 0.57-0.60 ms | 1.3-3.1 ms |
| 96 px | all 10 000 | 0.74-0.75 ms | 0.85-0.89 ms | 1.6 ms |
| 48 px (14 x 11) | nothing | 0.48 ms | 0.71-0.72 ms | 1.7-4.0 ms |
| 48 px | all 10 000 | 0.84 ms | 0.96-0.98 ms | 1.6-2.5 ms |
| 256 px (6 x 4) | nothing | 0.25-0.26 ms | 0.36 ms | 2.3 ms |

The cost follows the cells on screen (an icon, a name, a box when selected), not the folder
size. The worst case, the smallest cells all selected, is about 5 % of a 60 Hz frame.

## Image decoding (ROADMAP 5.1)

**Tool:** `nordend-decode [--runs N] [--stored] FILE...`: reads each file, then decodes it N
times from memory and prints the median (the first run is not counted). `--stored` skips the
EXIF turn.

### 2026-10-27: 20 MP photos, warm cache

OM-1 JPEGs from the photo library (5184x3888, 4:2:2, 9-14 MB, EXIF orientation 8), and one of
them converted with ImageMagick to the other formats (after the turn: 3888x5184).

| What | Size | Decode |
|------|-----:|-------:|
| JPEG, wuffs, 20 files, as stored | 9-14 MB | median 149 ms (133-159) |
| JPEG, wuffs, turned upright (orientation 8) | 13.4 MB | 218 ms |
| PNG, wuffs | 27.5 MB | 229 ms |
| WebP lossy (q90), libwebp | 2.8 MB | 210 ms |
| JPEG XL lossy (q90), libjxl, one thread | 2.8 MB | 394 ms |
| GIF, wuffs (2592x1944) | 2.9 MB | 12 ms |
| WebP from the library (2048x1536 and 1536x2048) | 0.2-0.7 MB | 16-33 ms |

Reading a 13 MB JPEG from the SATA SSD with a warm cache: 10-37 ms.

- The EXIF turn of a full 20 MP image costs about 65 ms (80 MB read and written, even in 32x32
  tiles). The views will not pay it at full size: thumbnails turn after scaling down, Quick Look
  can turn with the texture coordinates.
- A rough comparison for the thumbnails of ROADMAP 5.2: ImageMagick, which decodes JPEG with
  libjpeg-turbo, takes 284 ms for this file in full and 97 ms with DCT scaling to 1/8
  (`-define jpeg:size=648x486`). Its 16-bit float pipeline inflates both numbers; what
  libjpeg-turbo scaling gains on its own is *to be verified* with a real benchmark.

### 2026-10-27: JPEG for thumbnails, wuffs against TurboJPEG scaling and embedded previews

**Tool:** `nordend-bench-jpeg [--runs N] FILE...` (built when `libjpeg-turbo-devel` is
installed): the same file from memory with wuffs at full size and with TurboJPEG at 1/1, 1/2,
1/4, 1/8 (DCT scaling), to RGBA, no EXIF turn.

20 OM-1 JPEGs (5184x3888, 4:2:2, 3-14 MB), median of the per-file medians:

| Decoder | Output | Decode |
|---------|--------|-------:|
| wuffs | 5184x3888 | 145 ms |
| TurboJPEG 1/1 | 5184x3888 | 108 ms |
| TurboJPEG 1/2 | 2592x1944 | 96 ms |
| TurboJPEG 1/4 | 1296x972 | 90 ms |
| TurboJPEG 1/8 | 648x486 | 75 ms |

Scaling saves little: in these high-quality files the entropy (Huffman) decoding of every
coefficient dominates, and it is the same at every scale.

What the files already carry (extracted with exiftool, decoded with wuffs):

| Embedded image | Size | Bytes | Decode |
|----------------|------|------:|-------:|
| OM-1 JPEG, Olympus maker-note preview | 1600x1200 | 375 kB | 7.8 ms |
| OM-1 JPEG, EXIF thumbnail (IFD1) | 160x120 | 9 kB | 0.1 ms |
| OM-1 ORF, Olympus maker-note preview | 3200x2400 | 1.0 MB | 30 ms |

## Thumbnails (ROADMAP 5.2)

### 2026-10-27: making one thumbnail

Inside `thumbs::Cache::make`, one file, warm cache, median of 3 (a scratch timing program
around the same calls):

| Source | Size | Find preview | Decode | Scale | Turn | PNG | Total |
|--------|------|-------------:|-------:|------:|-----:|----:|------:|
| OM-1 JPEG, 1600x1200 preview | large (256) | 1.1 ms | 4.6 ms | 3.5 ms | 0 | 2.6 ms | ~12 ms |
| OM-1 JPEG, 1600x1200 preview | xx-large (1024) | 1.2 ms | 5.0 ms | 11.1 ms | 1.6 ms | 36 ms | ~55 ms |
| OM-1 ORF, 3200x2400 preview | large (256) | 1.8 ms | 19.4 ms | 12.8 ms | 0 | 2.0 ms | ~36 ms |
| OM-1 ORF, 3200x2400 preview | xx-large (1024) | 1.9 ms | 19.2 ms | 25.8 ms | 1.7 ms | 29 ms | ~78 ms |

Two changes came from these numbers: zlib level 1 instead of 6 (PNG of an xx-large thumbnail
171 ms → 36 ms, files about 13 % larger) and halving 2x2 before the area filter (3200 → 1024:
42 ms → 26 ms). Finding the preview with exiv2 (open, metadata, preview list) costs 1-2.5 ms.
With ROADMAP 5.3 the area filter became streaming (below): the xx-large scales are now 6.0 ms
(JPEG preview) and 15.6 ms (ORF preview).

### 2026-10-27: a screenful of thumbnails in the grid

**Tool:** `nordend-snapshot --columns DIR --keys "Ctrl+2[,Ctrl+plus...]" --thumbnails CACHE`:
logs "thumbnails ready in N ms", from the first grid frame to the last thumbnail drawn (made on
the worker pool, 8 threads on the Ryzen 7 5700G). 1600x1000 at scale 1.5.

| Folder | Cells (thumbnails) | Empty cache | Warm cache |
|--------|--------------------|------------:|-----------:|
| OM-1 JPEGs | 96 px, large (69) | 253 ms | 21 ms |
| OM-1 JPEGs | 192 px, x-large (23) | 142 ms | 22 ms |
| OM-1 ORF + sidecars + WebP | 96 px, large (35) | 405 ms | 12 ms |
| OM-1 ORF + sidecars + WebP | 192 px, x-large (12) | 189 ms | 13 ms |

Scrolling the grid with thumbnails (`--bench 600`, 90 JPEGs, warm, 2880x1620): 0.62 ms median,
0.76 ms p95, against 0.32 ms with icons only; one draw call per thumbnail.

## Quick Look (ROADMAP 5.3)

### 2026-10-27: an image fitted to the screen

`image::load_to_fit(file, 2880, 1620)` (1920x1080 at scale 1.5, minus nothing), best of 4, warm
cache: the preview when it is at least as large as the screen (or the file is raw), else the
whole image; scaled, then turned.

| File | Shown | From | Time |
|------|-------|------|-----:|
| OM-1 ORF (5 files) | 2160x1620 or 1620x2160 | 3200x2400 preview | 51-55 ms |
| OM-1 JPEG, 20 MP | 1620x2160 | whole image (preview 1600 < 2880) | 212 ms |
| Sony A7 II ARW | 1616x1080 | preview | 17 ms |
| Nikon D800 NEF | 2427x1620 | preview (full size) | 248 ms |
| WebP 1536x2048 | 1215x1620 | whole image | 56 ms |

Quick Look shows the cached thumbnail at once and this image when it arrives; the entries
before and after are made in advance.

The area filter had to change for this: 3200x2400 → 2160x1620 took 84 ms, because the
horizontal pass filled an intermediate buffer of input rows x output width (83 MB here). Now it
streams (each input row scaled across once, accumulated into the output row it belongs to), with
an opaque fast path and one reciprocal per pixel: 27 ms. The ORF went from 107-110 ms to 51-55 ms.

## Status bar (ROADMAP 5.11)

### 2026-10-28: scrolling with the status bar

Same folder and command as for the breadcrumb, sidebar shown.

| View | Median | p95 |
|------|-------:|----:|
| Details | 0.42-0.50 ms | 0.58-0.60 ms |
| Grid | 0.30-0.33 ms | 0.51-0.52 ms |
| Columns | 0.26 ms | 0.33-0.47 ms |

The details' median jumps between about 0.36 and 0.50 ms from run to run; five runs of the code
before this step gave the same two values (0.36, 0.50, 0.50, 0.37, 0.50 ms; GPU clock states,
*to be verified*). The p95 is unchanged: the bar costs nothing measurable. The free space is
read with one `statvfs` on a worker when the folder or its listing changes, never per frame.

## Breadcrumb (ROADMAP 5.10)

### 2026-10-27: scrolling with the breadcrumb

The same 10 000-entry folder and command as for the sidebar (sidebar shown), three runs each.
The breadcrumb measures its steps' text every frame (cached layouts).

| View | Median | p95 |
|------|-------:|----:|
| Details | 0.36-0.37 ms | 0.55-0.58 ms |
| Grid | 0.31-0.33 ms | 0.50-0.51 ms |
| Columns | 0.25-0.26 ms | 0.34-0.36 ms |

The same as without it (sidebar section below): nothing measurable.

## Sidebar (ROADMAP 5.9)

### 2026-10-27: scrolling with the sidebar

A 10 000-entry folder (`nordend-bench-dir --make 10000`), 600 frames at 2880x1620
(`nordend-snapshot --columns DIR --keys "Ctrl+3|Ctrl+2|Right[,Ctrl+B]" --bench 600 out.ppm
1440 810 2`), three runs each; the sidebar lists 9 places.

| View | Sidebar | Median | p95 |
|------|---------|-------:|----:|
| Details | shown | 0.35-0.50 ms | 0.54-0.58 ms |
| Details | hidden | 0.29-0.30 ms | 0.51-0.52 ms |
| Grid | shown | 0.30-0.32 ms | 0.50-0.51 ms |
| Grid | hidden | 0.27 ms | 0.47-0.50 ms |
| Columns | shown | 0.24-0.25 ms | 0.33-0.34 ms |
| Columns | hidden | 0.18-0.19 ms | 0.26-0.27 ms |

The sidebar costs about 0.03-0.06 ms per frame: its background, a dozen rows of text and icons.

The mount table, polled for POLLPRI (scratch C program and a `PlacesWatcher` in a user and
mount namespace, `unshare -rm`): idle, poll() times out with no events; a mount wakes it once
(POLLPRI|POLLERR, then quiet again), and the watcher reports the new mount point and, after
`umount`, its removal.

## Info pane preview (ROADMAP 5.8)

### 2026-10-27: walking a folder of ORF files

**Target** (ROADMAP 5.8): walking a folder of ORF files with the arrows, the thumbnail appears
without slowing the walk.

`/run/media/pankaspe/DATA/fotografia/vero` (163 OM-1 ORF, with sidecars and JPEGs), at
2880x1620 (`nordend-snapshot --columns DIR --reveal FIRST.ORF [--keys Ctrl+2|Ctrl+3]
--thumbnails CACHE --walk 120 out.ppm 1440 810 2`): Down once per frame, the event loop run
for about 1 ms between frames so that the pane's summaries and thumbnails arrive, frames
timed alone. "Cold": a new thumbnail cache.

| View | Cache | Median | p95 | Max |
|------|-------|-------:|----:|----:|
| Columns | cold | 0.41 ms | 0.62 ms | 1.6 ms |
| Grid | cold | 0.46 ms | 1.30 ms | 3.3 ms |
| Details | cold | 0.58 ms | 1.33 ms | 2.3 ms |
| Columns | warm | 0.47 ms | 1.19 ms | 3.2 ms |
| Grid | warm | 0.55 ms | 1.39 ms | 3.7 ms |
| Details | warm | 0.61 ms | 1.10 ms | 3.3 ms |

The highest frames upload a 1024 px thumbnail texture. Behind the frames, once the cursor
stops on an entry (five ORFs, best of 5 / one run each, warm file cache):

| Step | Time |
|------|-----:|
| `preview::summarize` (stat, owner, EXIF, sidecar) | 3.7-4.6 ms |
| xx-large thumbnail made (`nordend-thumb --size xx-large --make`) | 107 ms |
| xx-large thumbnail read from the cache | 9.5-10 ms |

## Info pane (ROADMAP 5.7)

### 2026-10-27: scrolling with the pane open

The same 10 030-entry folder as for the details view, 600 frames at 2880x1620
(`nordend-snapshot --columns DIR --keys "Ctrl+3[,Ctrl+I]" --bench 600 out.ppm 1440 810 2`),
three runs each:

| View | Pane | Median | p95 |
|------|------|-------:|----:|
| Details | open | 0.44-0.46 ms | 0.52-0.54 ms |
| Details | closed | 0.33-0.45 ms | 0.50-0.53 ms |
| Grid | open | 0.28 ms | 0.46 ms |
| Grid | closed | 0.30 ms | 0.48 ms |

The median moves between about 0.33 and 0.46 ms from run to run with or without the pane (GPU
clock states, *to be verified*); the p95 stays at about 0.5 ms. The pane is drawn once per frame
and costs nothing measurable.

## Details view (ROADMAP 5.6)

### 2026-10-27: sorting and scrolling 10 000 entries

**Target** (ROADMAP 5.6): a 10k-file folder sorted by size or date scrolls without stutter.

A folder of 10 000 sparse files of random sizes (0-30 MB) and dates, five camera-like
extensions, plus 30 folders, made by a scratch Python script (`truncate` and `utime`).
`nordend-snapshot --columns DIR --keys "Ctrl+3" --sort FIELDS --bench 600 out.ppm 1440 810 SCALE`:
the sort is timed around `DetailsView::sort_by`, then the table is scrolled from top to bottom
over 600 frames, each waiting for the GPU.

| Order | Sort | 1440x810 median / p95 / max | 2880x1620 median / p95 / max |
|-------|-----:|----------------------------:|-----------------------------:|
| Name (as loaded) | — | 0.31 / 0.50 / 1.7 ms | 0.33 / 0.49 / 1.8 ms |
| Size | 1.1 ms | 0.31 / 0.50 / 3.3 ms | 0.33 / 0.51 / 1.6 ms |
| Date, newest first (two clicks) | 1.1 + 2.0 ms | 0.44 / 0.51 / 1.9 ms | 0.45 / 0.53 / 1.6 ms |
| Type | 3.2 ms | 0.31 / 0.52 / 3.6 ms | 0.33 / 0.53 / 1.5 ms |

The maximum is the first frame (glyphs rasterized). Sorting by type compares type names
(string comparisons); by size or date, numbers.

## Text previews (ROADMAP 5.5)

### 2026-10-27: reading and scrolling text

`preview::read_text` and `preview::read_info`, best of 7, warm cache (a scratch program linking
`libnordend_preview.a`):

| File | Read | Result |
|------|-----:|--------|
| Log file, 1.6 MB (first 256 KB) | 0.67 ms | 3 196 lines |
| C++ source, 22 kB | 0.06 ms | 495 lines |
| `/usr/bin/ls` (as text) | 0.02 ms | binary: card |
| Folder of 10 000 files (`read_info`) | 4.3 ms | 10 000 entries |

Scrolling Quick Look's text page from top to bottom over 600 frames
(`nordend-snapshot --columns DIR --reveal FILE --keys space --bench 600 out.ppm 1440 810 SCALE`),
each frame waiting for the GPU:

| Text | Screen | Median | p95 | Max |
|------|--------|-------:|----:|----:|
| Log, 3 196 lines | 1440x810 | 0.45 ms | 0.64 ms | 3.2 ms |
| Log, 3 196 lines | 2880x1620 | 0.82 ms | 1.03 ms | 4.0 ms |
| darktable XMP, 486 lines | 2880x1620 | 0.69 ms | 0.95 ms | 2.6 ms |

The maximum is the first frame, when the monospace glyphs are rasterized. Each benchmark frame
moves about 5 lines, so most frames shape new lines: scrolling by hand costs less.

## Raw previews (ROADMAP 5.4)

### 2026-10-27: other cameras (rawsamples.ch, 22 files)

22 files from rawsamples.ch (not in the repository; the test over them runs when
`NORDEND_RAW_SAMPLES` names their folder), warm cache, Release.

**What the files carry** (`exiv2 -pp`, then the TIFFs extracted with `exiv2 -ep` and looked at):
every one has a 160x120 EXIF thumbnail, and a JPEG of 1536-8688 px except the Hasselblad 3FR
(only an uncompressed 320x240 TIFF). Canon's TIFF previews (CR2, 384x256 to 668x448, 8 or
16 bits) are sensor data without white balance or tone curve: green and dark, not usable. The
EXIF thumbnail is always 4:3: for 3:2, 16:9 and 1:1 photos it has black bars (6-8 rows, or 20
columns; measured with `magick -crop ... -format %[fx:mean]`).

**Quick Look**, `image::load_to_fit(file, 2880, 1620)`, best of 4 (a scratch program linking
`libnordend_image.a`):

| File | Shown | Whole file read first | Head read first |
|------|-------|---------------------:|----------------:|
| OM-1 ORF (preview large enough) | 1620x2160 | 51.9 ms | 51.6 ms |
| Hasselblad 3FR (TIFF preview) | 320x240 | 42.1 ms | 0.4 ms |
| Nikon D800 TIFF | 570x375 | 89.2 ms | 2.3 ms |
| Panasonic GX8 16:9 (cropped) | 1920x1082 | 17.6 ms | 10.8 ms |
| Sony A7 II ARW | 1616x1080 | 16.4 ms | 12.6 ms |
| Nikon D800 NEF (preview large enough) | 2427x1620 | — | 265 ms |
| Canon 5DS CR2 (8688x5792 JPEG) | 2430x1620 | — | 379 ms |

When the preview is smaller than the screen, `decode_file` is tried on the raw file first (it
could be an image format) and fails; it allocated and read the whole file before finding out
(55 MB for the 3FR, 112 MB for the TIFF). Now it reads 16 bytes first and stops on a format it
does not decode. The NEF and the CR2 decode a full-size JPEG preview (wuffs has no reduced-scale
JPEG decoding).

**Thumbnails**, `nordend-thumb --make --size SIZE`, a fresh cache each run, best of 3:

| File | normal (128), before | normal, after | large (256) |
|------|--------------------:|--------------:|------------:|
| Canon 5DS CR2 | 8.9 ms | 372 ms | 376 ms |
| Canon 1D X CR2 | 9.0 ms | 143 ms | 146 ms |
| Sony RX10 II ARW | 7.5 ms | 22 ms | 24 ms |
| Panasonic GX7 1:1 RW2 | 4.3 ms | 33 ms | 37 ms |
| Nikon D800 TIFF | 10.0 ms | 9.8 ms | 11.8 ms |
| Hasselblad 3FR | no thumbnail | 2.9 ms | 5.5 ms |
| Panasonic GX8 16:9 RW2 | 4.2 ms (4:3) | 4.2 ms | 30 ms |

The normal size got slower for 3:2 and 1:1 photos: their EXIF thumbnail has black bars and is
now skipped, so the next preview is decoded (for the Canons, the full-size JPEG: wuffs cannot
decode a JPEG at a reduced scale). The large size, which a 96 px cell uses at scale 2, already
decoded it before. A grid of the 22 files at scale 1 is ready in 401 ms instead of 48 ms the
first time; from the cache, nothing changes.

## MIME detection (ROADMAP 3.2)

**Tools:** `nordend-bench-dir` ("mime" column: types from names, as the loader does) and
`nordend-mime PATH...` (full detection, content included, timings on stderr).

### 2026-10-26: warm cache

| What | Entries | Time |
|------|--------:|-----:|
| Load the shared-mime-info database (1 231 globs, 507 magic sections) | | 1.0 ms |
| Types from names, generated 10k folder (median of 20) | 10 000 | 1.7 ms |
| Types from content, `.xmp` sidecars of a photo folder | 163 | 7 ms |
| Types from content, `/usr/bin` (mostly no extension) | 1 475 | 52 ms |

Names decide most files for free; reading heads costs about 35-45 µs per file even warm, so the
views will ask for content-based types only for visible entries, on workers.

Correctness, against `gio info -a standard::content-type`: 0 differences on about 4 750 files.

## Default applications (ROADMAP 6.1)

**Tool:** `nordend-open` (`--default TYPE...` prints the chosen desktop ID; the time to read the
applications goes to stderr).

### 2026-10-29

| What | Time |
|------|-----:|
| Read every desktop entry, mimeapps.list and mimeinfo.cache (21 entries, 5 runs) | 0.42-0.48 ms |

Correctness, against `gio mime TYPE` (GLib 2.88.3): every type in `/usr/share/mime/types` and
every alias in `/usr/share/mime/aliases`, 1 232 names, 564 of them with an application:
0 differences.

```sh
(cat /usr/share/mime/types; cut -d' ' -f1 /usr/share/mime/aliases) | sort -u > all.txt
build/nordend-open --default $(cat all.txt) 2>/dev/null > ours.txt
while read t; do
  d=$(gio mime "$t" | sed -n 's/^Default application for “[^”]*”: //p')
  printf '%s\t%s\n' "$t" "${d:--}"
done < all.txt > gio.txt
diff ours.txt gio.txt
```

---

*Benchmarks designed, run and written up by Claude (Opus), Nordend's copilot; hardware runs and
cold-cache measurements by the author.*
