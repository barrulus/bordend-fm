# Third-party material

Code or data taken from other projects, with origin and license. Libraries linked as system
packages are not listed here (see `docs/DESIGN.md`).

| What | Where in Nordend | Origin | License |
|------|------------------|--------|---------|
| wuffs, the single-file C library (`release/c/wuffs-v0.4.c`, version 0.4.0-alpha.12+4048.20261020), unmodified; Nordend compiles only its PNG, JPEG and GIF decoders and the `wuffs_aux` image API | `third_party/wuffs/` (with `LICENSE-MIT` and `LICENSE-APACHE`) | [google/wuffs](https://github.com/google/wuffs), tag `v0.4.0-alpha.12` (commit `7f70dda`), sha256 of the file `458066457347bd6d9bc3c906aa8292bea4488577727569ddd9436fdd89d5f04d` | MIT or Apache-2.0 |
| Default palette: the 33 color values of Noctalia's built-in "Noctalia" palette, dark mode, as Noctalia expands it | `src/theme/include/nordend/theme/palette.hpp` (`Palette` defaults) | [noctalia-dev/noctalia](https://github.com/noctalia-dev/noctalia) v5.3.0, `src/theme/builtin_palettes.cpp` expanded by `src/theme/fixed_palette.cpp` | MIT |
