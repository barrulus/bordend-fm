# Upstream bug log

Bugs found in **Noctalia Shell** and **Umbriel** while building Nordend.
Each confirmed bug becomes an upstream issue or PR (github.com/noctalia-dev).

Status: `suspected` → `confirmed` → `reported` → `pr-open` → `fixed` (or `wontfix` / `not-a-bug`)

## Template

```
### NB-000 — <short title>
- Project: noctalia | umbriel
- Version: <exact version / commit>
- Status: suspected
- Found: YYYY-MM-DD, while working on <roadmap step>
- Repro:
  1. ...
- Expected:
- Actual:
- Notes / suspected cause:
- Upstream: <issue or PR link>
```

## Entries

### NB-001 — Umbriel title bars don't follow the Noctalia theme
- Project: noctalia
- Version: 5.3.0 (builtin `umbriel` template)
- Status: fixed
- Found: 2026-10-26, while working on ROADMAP 4.2
- Repro:
  1. Umbriel with `[appearance.title_bar] mode = "on"`, Nordend open (server-side decorations).
  2. Switch theme in Noctalia.
- Expected: the title bar takes the theme's colors, like the borders.
- Actual: it stays Umbriel's default dark grey; `~/.config/umbriel/noctalia.toml` has no
  `[colors.title_bar]` section.
- Notes / suspected cause: the template didn't write `[colors.title_bar]`, which Umbriel reads
  for title bar colors (`src/config/fields_general.cpp`). Already fixed upstream before we got
  there: `65b34e5` and `5f864d4`, released in Noctalia 5.4.0; our package was bumped and the
  author verified it (void-noctalia BUG-025).
- Upstream: https://github.com/noctalia-dev/noctalia/commit/65b34e5 (no issue or PR needed)
