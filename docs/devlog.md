# Devlog

Work log, newest first. Each entry: what was done, decisions, measurements, open points.

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
