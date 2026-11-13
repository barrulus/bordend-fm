# Configuration

[Documentation](README.md) · [Appearance](appearance.md) · [Actions](actions.md) · [Shortcuts](shortcuts.md)

rummage uses TOML files rather than a settings window. Every configuration key is
optional. The first normal launch creates the complete reference when no config
exists. Existing files are never replaced. You can keep a small file with only
your changes, or copy the
[annotated example](config.example.toml) from this checkout.

## Files and locations

| File or directory | Default location | Ownership |
| --- | --- | --- |
| Settings | `~/.config/rummage/config.toml` | Edited by you |
| Palette | `~/.config/rummage/colors.toml` | Written by Noctalia or edited by you |
| Window/tab state | `~/.local/state/rummage/state.toml` | Written by rummage |
| Thumbnail cache | `~/.cache/thumbnails/` | Shared freedesktop cache |
| Bookmarks | `~/.config/gtk-3.0/bookmarks` | Shared with GTK file managers |
| User folder definitions | `~/.config/user-dirs.dirs` | Desktop/XDG user-directory setup |
| Accent application icon | `~/.local/share/icons/hicolor/scalable/apps/rummage.svg` | Managed by rummage when enabled |

The corresponding XDG base-directory variables replace the usual defaults:
`XDG_CONFIG_HOME`, `XDG_STATE_HOME`, `XDG_CACHE_HOME` and `XDG_DATA_HOME`.
`--config FILE` selects an alternative settings file. It does not relocate the
palette, saved state or caches; those still use their normal XDG locations.

From the repository root, to create a configuration when none exists:

```sh
mkdir -p "${XDG_CONFIG_HOME:-$HOME/.config}/rummage"
cp -i docs/config.example.toml "${XDG_CONFIG_HOME:-$HOME/.config}/rummage/config.toml"
rummage --check-config
```

`cp -i` asks before overwriting an existing file. Alternatively, create a minimal file:

```toml
[appearance]
background_opacity = 0.85

[icon]
theme = "system"

[keybindings]
rename = "Ctrl+r"
```

## Settings reference

| Key | Default | Accepted value / behavior |
| --- | --- | --- |
| `appearance.background_opacity` | `1.0` | Number from 0 to 1; background only |
| `status_bar.text_size` | `12.0` | Number from 8 to 32 logical pixels; applies live |
| `font.family` | `"sans-serif"` | Fontconfig pattern for interface text |
| `font.monospace` | `"monospace"` | Fontconfig pattern for text previews |
| `pointer.double_click_ms` | `400` | Integer from 100 to 2000 milliseconds |
| `pointer.double_click_distance` | `4` | Integer from 0 to 50 logical pixels |
| `icon.theme` | `"system"` | Desktop theme, installed theme name, or `"builtin"` |
| `icon.follow_accent` | `true` | Write the application icon in the current accent colour |
| `icon.places` | No overrides | Sidebar-purpose keys mapped to icon names or absolute paths |
| `keybindings` | Built-in bindings | Action names mapped to one chord; empty string disables |
| `actions` | Built-in registry only | Array of custom commands or extraction-preset overrides |
| `context_menu` | Built-in menus | Optional common or context-specific menu layouts |

See [appearance](appearance.md) for all place-icon keys, [shortcuts](shortcuts.md)
for binding names, and [actions](actions.md) for action fields and menu layouts.
The [example file](config.example.toml) supplies commented recipes and full default
menus. Its commented recipes are opt-in, not enabled defaults.

TOML table placement matters: use one `[keybindings]` table, one `[context_menu]`
table, and one `[[actions]]` table per action. A field below a table heading belongs
to that table until the next heading. Do not paste a binding into an action table.

## Validation and live reload

```sh
rummage --check-config
rummage --config /path/to/config.toml --check-config
```

Validation runs without opening a Wayland window. Status 0 means no problems;
status 1 means configuration or binding problems. A missing config is valid and
uses defaults, so verify the printed path if a check unexpectedly succeeds.

| Change | When it applies |
| --- | --- |
| Background opacity, status text size, pointer timing, icon theme/place icons/accent behavior | Live |
| Actions, menu layout and keybindings | Live, accepted together as a valid set |
| Interface and monospace font | Next application start |
| `colors.toml` palette | Live |
| Desktop icon-theme discovery | At startup; explicit `icon.theme` edits reload live |

At startup, invalid scalar values use defaults and unknown keys are reported.
A TOML syntax error discards the file. Invalid bindings fall back to the default
binding set. During reload, a file that cannot parse leaves settings in use; bad
scalar values in an otherwise parsed file can revert those settings to defaults.
Invalid actions, menus or bindings retain the previous valid action/menu/binding
set. Diagnostics go to stderr; use `--check-config` before relying on an edit.

Deleting the config restores defaults on reload. The example file and your personal
copy are not automatically synchronized by updates.

## Saved state

rummage remembers window size/maximization, pane layout and divider, sidebar zoom,
info-pane settings and command-palette history. Each pane retains its tabs,
locations, active tab, views, zoom, sorting, hidden visibility and filename filters.
Back/forward history and details-tree expansion are not saved across restarts.

Without a directory argument, multiple tabs or a previously created second pane
restore both pane sessions, including which pane is active and whether one is
hidden. Missing locations are skipped; an empty restored pane falls back to Home.
A single tab that has never been split starts at Home. An explicit directory
starts there instead. Picker windows do not overwrite browser state.

State is application-managed, not an extra settings API. To reset it, close all
rummage windows, move `state.toml` aside and restart. Keep the copy to restore the
old layout. Resetting state does not change settings, bookmarks or your files.

## Migration from Nordend

A first normal launch without `--config` copies existing Nordend settings, palette
and state into rummage's XDG directories only where absent, preserving originals
and existing rummage files. Config checks, explicit-config runs and picker/portal
launches do not migrate. Commands and paths inside copied config stay literal;
update old executable names yourself. See [migration details](install.md#upgrading-from-nordend).
