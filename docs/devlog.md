# Devlog

Work log, newest first. Each entry: what was done, decisions, measurements, open points.

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
