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
| Window/tab/folder state | `~/.local/state/rummage/state.toml` | Written by rummage |
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
| `startup.mode` | `"session"` | `"home"`, `"folder"`, `"last"`, or `"session"`; next launch |
| `startup.folder` | `"~"` | Absolute path or `~/` path, used with mode `"folder"` |
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
| Startup folder/mode | Next application start |
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

View mode, sort key/direction, row size, grid icon size and details-column widths
are remembered per folder. Revisiting a folder restores them; a folder without
saved settings initially inherits the current view. Sorting one tab does not
change other tabs. Existing tabs retain their own view and zoom until navigation;
when the same folder is used in several tabs, the last-used settings are saved
for future visits.

The folder history is capped at **512 entries**, with the least recently used
removed first. It stores paths and small settings records, never directory listings
or thumbnails, and adds no folder files, scanning or watchers. State snapshots
share the folder records until a setting or recency changes. Writes use the existing
one-second debounce on a worker and flush on exit. Settings are keyed by path; a renamed or moved folder gets a new entry.

Starting with an explicit directory always opens that directory. Otherwise,
`startup.mode` controls startup:

| Mode | Behavior |
| --- | --- |
| `"session"` (default) | Restore saved tabs and panes (including a hidden pane); otherwise open Home |
| `"home"` | Open Home in one tab and one pane |
| `"folder"` | Open `startup.folder` in one tab and one pane |
| `"last"` | Open the last active browser folder in one tab and one pane, including after a single-tab session |

```toml
[startup]
mode = "folder"
folder = "~/Projects"
```

For last-location startup, use `mode = "last"`. Missing configured/last folders
fall back to Home; session restore skips vanished tabs and uses Home for an empty restored pane. Trash does not replace the
last ordinary location. Picker windows use their requested location and do not
write browser state. Paths support `~` and `~/`, without environment-variable or
shell expansion.

State is application-managed, not an extra settings API. To reset it, close all
rummage windows, move `state.toml` aside and restart. Keep the copy to restore the
old layout. Resetting state does not change settings, bookmarks or your files.

## Migration from Nordend

A first normal launch without `--config` copies existing Nordend settings, palette
and state into rummage's XDG directories only where absent, preserving originals
and existing rummage files. Config checks, explicit-config runs and picker/portal
launches do not migrate. Commands and paths inside copied config stay literal;
update old executable names yourself. See [migration details](install.md#upgrading-from-nordend).
