# Third-party material

Code or data taken from other projects, with origin and license. Libraries linked as system
packages are not listed here (see `docs/DESIGN.md`).

| What | Where in Nordend | Origin | License |
|------|------------------|--------|---------|
| wuffs, the single-file C library (`release/c/wuffs-v0.4.c`, version 0.4.0-alpha.12+4048.20261020), unmodified; Nordend compiles only its PNG, JPEG and GIF decoders and the `wuffs_aux` image API | `third_party/wuffs/` (with `LICENSE-MIT` and `LICENSE-APACHE`) | [google/wuffs](https://github.com/google/wuffs), tag `v0.4.0-alpha.12` (commit `7f70dda`), sha256 of the file `458066457347bd6d9bc3c906aa8292bea4488577727569ddd9436fdd89d5f04d` | MIT or Apache-2.0 |
| Tabler Icons webfont 3.50.0: the outline font `tabler-icons.ttf` and the filled `tabler-icons-filled.ttf`, cut down with fonttools' `pyftsubset` (glyph names kept, no layout features) to the 47 and 1 icons listed in `icons.txt` and `icons-filled.txt`; glyphs otherwise unmodified. Made by `scripts/make-icon-font.sh`; compiled into the program by `cmake/embed.cmake`. sha256 of the subsets: `ece67a34b942319273ebb0171836dfdf560f04c938551f434b834887ad3906cf` (outline), `5cdccdcab00e5d84ed212712b2601b38240e1821b30b36d70427ff0561f9845c` (filled) | `third_party/tabler-icons/` (with the upstream `LICENSE`) | [tabler/tabler-icons](https://github.com/tabler/tabler-icons), npm package [`@tabler/icons-webfont`](https://www.npmjs.com/package/@tabler/icons-webfont) 3.50.0 (tag `v3.50.0`), sha256 of the tarball `1afa9b3764607de419b4eeff296f2328361a5ba605ebd35812832f61ddbf070e` | MIT |
| Default palette: the 33 color values of Noctalia's built-in "Noctalia" palette, dark mode, as Noctalia expands it | `src/theme/include/nordend/theme/palette.hpp` (`Palette` defaults) | [noctalia-dev/noctalia](https://github.com/noctalia-dev/noctalia) v5.3.0, `src/theme/builtin_palettes.cpp` expanded by `src/theme/fixed_palette.cpp` | MIT |
