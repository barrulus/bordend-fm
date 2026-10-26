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

---

*Benchmarks designed, run and written up by Claude (Opus), Nordend's copilot; hardware runs and
cold-cache measurements by the author.*
