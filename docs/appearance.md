# Appearance

[Documentation](README.md) · [Configuration](configuration.md)

## Colours

rummage uses `$XDG_CONFIG_HOME/rummage/colors.toml` (normally
`~/.config/rummage/colors.toml`) when present.
Otherwise it follows light/dark style and accent through the desktop settings
portal. Without either source it uses its built-in palette. Palette changes reload
live. The [Noctalia template](../data/noctalia/rummage) shows the palette keys;
font choices and transparency belong in `config.toml`.

## Noctalia colours

Install rummage, then copy its template into Noctalia's user template directory:

```sh
mkdir -p ~/.config/noctalia/templates ~/.config/rummage
cp /usr/share/rummage/noctalia/rummage ~/.config/noctalia/templates/
```

For a user install use `~/.local/share/rummage/noctalia/rummage`; from a checkout
use `data/noctalia/rummage`; for a local Nix build use
`result/share/rummage/noctalia/rummage`. Adjust these paths if using a custom
`XDG_CONFIG_HOME` or install prefix.

Add to Noctalia's `config.toml`:

```toml
[theme.templates.user.rummage]
input_path = "templates/rummage"
output_path = "~/.config/rummage/colors.toml"
```

Select a theme in Noctalia, even the current one, to generate the file. Later theme
changes update rummage live. `--config` changes the settings file only; it does not
change the palette path.

## Fonts

In rummage's `config.toml`:

```toml
[font]
family = "sans-serif"
monospace = "monospace"
```

Values are Fontconfig patterns, for example `"Inter:weight=medium"` or
`"JetBrains Mono"`. rummage shapes text with HarfBuzz and uses font fallback.
Restart after changing fonts. Check resolution with `fc-match 'Inter'` if the
chosen family does not appear.

## Icon themes

```toml
[icon]
theme = "candy-icons"
follow_accent = true
```

`theme = "system"` discovers the desktop theme from GTK/KDE/qt6ct settings at
startup. An installed name selects it explicitly; `"builtin"` uses the bundled
Tabler icons. Theme icons may be SVG or PNG. Inherited themes and built-in icons
provide fallbacks. Editing this setting reloads live; desktop theme changes require
a restart for discovery.

SVG theme icons use rummage's in-process FreeType-based SVG renderer. This is an
icon subset, not full SVG document/browser support; see the
[renderer validation policy](svg-renderer-validation.md).

### Places icons

```toml
[icon.places]
home = "builtin:home"
documents = "folder-documents"
pictures = "/home/you/.local/share/icons/my-pictures.svg"
```

Supported keys are `home`, `desktop`, `documents`, `downloads`, `music`, `pictures`,
`videos`, `filesystem`, `trash` and `trash_full`. They follow the purpose of an XDG
folder even when its path/name changes. `trash` covers both states unless
`trash_full` overrides the full state.

Values accept a theme icon name, `builtin:<name>`, or an absolute SVG/PNG path.
Built-in names are listed in [icons.txt](../third_party/tabler-icons/icons.txt) and
[icons-filled.txt](../third_party/tabler-icons/icons-filled.txt); filled names use
the `-filled` suffix. Theme names need a theme other than `builtin`. Missing icons
fall back to the usual icon. Changes reload live.

### Application accent icon

With `icon.follow_accent = true`, rummage writes an accent-coloured application
icon under `$XDG_DATA_HOME/icons/hicolor/scalable/apps/rummage.svg` (normally
`~/.local/share/icons/...`). It updates when the accent changes. Turning the setting
off removes rummage's generated override, or restores the installed icon for a
user-prefix install. An unrelated user icon at that path is left alone. Docks may
cache the previous icon until they refresh.

## Folder icons and properties

With system/named themes, folder icons use `Icon=` under `[Desktop Entry]` in the
folder's `.directory` file, shared with Dolphin. Values can be theme names,
absolute SVG/PNG paths or `./` paths relative to that folder. Standard XDG folders
use purpose-specific icons without an override; missing icons fall back. The
`builtin` theme retains Tabler icons. KDE's `EmptyIcon` behavior is not supported.
Reopen the parent folder after changing its icon externally.

Use **Alt+Enter** or **Properties** for a folder, then **Change Icon…**. Search the
paged theme icon grid or enter/paste a custom icon name/path. Default removes the
override. Choices are staged until Apply; Cancel leaves disk unchanged. Saving
preserves other `.directory` fields and refreshes open views. Trash properties
are read-only. Tab changes controls; arrows/Page Up/Down browse icons; Enter
chooses and Escape backs out or closes. Short windows scroll the content.

## Text size and sidebar zoom

Set `[status_bar]` with `text_size = 15` for larger status text (8–32 logical pixels,
12 by default). It reloads live and the bar grows to fit. Sidebar zoom is independent:
Ctrl+wheel over it, or F6 followed by Ctrl+plus/minus/0. File-list zoom remains per tab.

## Transparency

```toml
[appearance]
background_opacity = 0.70
```

This changes only backgrounds; text, icons and thumbnails remain solid. `1.0` is
opaque and `0.0` transparent. Keep compositor window opacity at `1.0`, since a
compositor opacity rule fades all content. rummage supplies transparency; any blur
comes from the compositor. The setting reloads live.

## Scale, decorations and pointers

Fractional scaling uses the compositor's fractional-scale and viewporter protocols.
The compositor can supply server-side decorations; otherwise rummage draws its own
title bar and resize edges. Cursor shapes use cursor-shape-v1 where available,
with an Xcursor fallback. See [compatibility](compatibility.md) for protocols,
fallbacks and diagnostic environment variables.
