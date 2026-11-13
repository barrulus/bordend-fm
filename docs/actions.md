# Actions and context menus

[Documentation](README.md) · [Configuration](configuration.md) · [File operations](file-operations.md)

## Define an action

Add `[[actions]]` tables to `~/.config/rummage/config.toml`. Actions appear in the context
menu and command palette (**Ctrl+K**) when their selection and MIME rules match. Programs
are optional dependencies: install the ones your actions use. Missing programs are dimmed
in the context menu and omitted from the palette.

This example compresses one or more selected files or folders. rummage asks for the ZIP
filename and destination, calls **7-Zip directly**, then notifies you when the command exits:

```toml
[[actions]]
id = "compress"
label = "Compress to ZIP…"
argv = ["7z", "a", "-tzip", "-spd", "{destination}", "--", "{files}"]
selection = "many"
placement = "menu"
destination = "file"
destination_name = "Archive.zip"
notify_on_completion = true
```

Use `7zz` instead of `7z` if that is your installed executable. `-spd` treats wildcard
characters in selected filenames literally. `7z a` updates an existing archive; choose a
new filename to create a separate archive. No Python or shell wrapper is needed.

The same picker works with other commands. These examples require **ImageMagick** and
**GNU cp**, respectively:

```toml
[[actions]]
id = "resize-image"
label = "Resize Image to Half Size…"
argv = ["magick", "{file}", "-resize", "50%", "{destination}"]
mime = ["image/*"]
selection = "one"
destination = "file"
destination_name = "resized.png"
notify_on_completion = true

[[actions]]
id = "copy-to"
label = "Copy To…"
argv = ["cp", "--archive", "--", "{files}", "{destination}"]
selection = "many"
destination = "directory"
notify_on_completion = true
```

## Destination dialogs

`destination = "file"` opens rummage's save dialog; `"directory"` selects an existing
folder. `destination_name` is an optional literal suggested filename for the file picker.
The picker starts in the current folder, uses the action's label as its title, and passes
the selected absolute path as `{destination}`. Cancelling launches nothing. The selection
and working directory are captured before the picker opens. rummage does not create the
output: the command does, with its own overwrite behavior. The save dialog confirms an
existing file; commands such as `cp` and `magick` can replace output. External operations
are not part of rummage's Undo history.

Omit `destination` and `{destination}` when the application already supplies its own dialog;
for example, File Roller can use
`argv = ["file-roller", "--default-dir", "{directory}", "--add", "{files}"]`.
A destination picker and `{destination}` must be configured together.

## Arguments and matching

`argv` is an argument array, with no shell interpretation, globbing, or environment/tilde
expansion. Placeholders must occupy whole arguments after the executable; do not add shell
quotes around them. Filenames containing spaces remain single arguments.

| Placeholder | Value |
| --- | --- |
| `{file}` | One absolute selected path; requires `selection = "one"` |
| `{files}` | Each selected absolute path as a separate argument; requires `"one"` or `"many"` |
| `{directory}` | The current folder, also used as the command's working directory |
| `{destination}` | The absolute path chosen by rummage's destination picker |

`selection` defaults to `"one"` (exactly one item). `"many"` allows one or more items;
`"background"` requires none; `"any"` allows any number, including none. Use `{directory}`
or fixed arguments with `"background"`/`"any"`, not `{file}` or `{files}`. Without `mime`,
all file types are allowed, but multi-selection still requires `selection = "many"`.
MIME filters accept exact types and patterns such as `"image/*"`; **every selected item**
must match at least one filter. Folders use `"inode/directory"`.

## Completion notifications

`notify_on_completion = true` enables a desktop notification and status message on success
or failure; it defaults to false. rummage sends notifications itself, without `notify-send`.
The option applies to external actions: exit code 0 means success, other exit codes or
signals mean failure. Cancelling the picker or failing to launch sends no completion notice;
launch errors still appear in the status bar. Keep rummage open to receive notifications.
A desktop notification service is needed for popups; the status message still works without
one. Completion follows the launched process, so a GUI launcher that delegates work and
exits early cannot report when that delegated work finishes.

## Arrange and nest menus

Actions default to the **Actions** submenu. `placement = "menu"` puts an action directly
above that submenu. A `[context_menu]` layout can instead arrange built-in commands and
custom actions into nested groups, in the order you choose. For the three actions above:

```toml
[context_menu]
items = [
  { id = "open" },
  { id = "open_with" },
  { id = "action:compress" },
  { label = "My Actions", items = [
    { label = "Images", items = [ { id = "action:resize-image" } ] },
    { label = "Files", items = [ { id = "action:copy-to" } ] },
  ] },
  { separator = true },
  { remaining = true },
]
```

Use `action:<id>` for actions and built-in IDs such as `copy`, `rename`, `sort`, and
`open_with` for other commands. A group has `label` and nested `items`; groups can nest
further. Add `label` to an ID reference to rename its menu entry, or `separator = true`
to place a line above it. Inapplicable items and empty groups disappear.

`{ remaining = true }` keeps unlisted items in their default grouping. Use it at most once
per layout, at any depth; omit it to hide unlisted items. Referencing `open_with`, `new_file`,
`sort`, or `actions` keeps the parent's remaining children, including dynamically discovered
applications/templates. Moving a child elsewhere removes it from the original group. Each
ID can appear only once per layout. Explicit layout references override action placement;
menu customization does not change command-palette or shortcut availability.

| Layout | Context |
| --- | --- |
| `items` | Optional common fallback for contexts without a specific override |
| `entry` | Files or folders targeted in an ordinary folder |
| `background` | Empty space in an ordinary folder, without a target |
| `trash` | Trash entries or empty space in Trash |
| `sidebar` | Devices, bookmarks, and the Trash sidebar item |

Specific arrays override `items`. Without either, rummage uses that context's built-in menu;
an empty array hides its menu. The [example config](config.example.toml) includes the
**complete default layouts for all four contexts**, their separators and submenu contents,
as well as the available IDs and additional custom-action examples. Default reference arrays
and the custom `items` example are alternatives: keep only the overrides you intend to use
and only one `[context_menu]` table.

Copy Location, Extract Here, and Extract To are built-in actions. Extraction is filtered to
applicable archive selections. Missing tools show setup guidance when invoked.
Supported backends include unrar, 7-Zip, File Roller, unzip, gzip and tar. Copy Location cannot be replaced. Override an extraction preset by its ID to change
its label, placement, or enabled state; providing `argv` makes it an external command.
Actions are unavailable in Trash, and external actions are unavailable in file-picker mode.

Action IDs can also be bound in the existing `[keybindings]` table, for example
`"action:compress" = "Ctrl+Alt+z"`; define the action before enabling its binding. Actions,
keybindings, and menu layouts reload together. Invalid changes retain the previous valid
set on reload. Validate edits with `rummage --check-config` or
`rummage --config /path/to/config.toml --check-config`.

The maintained reference is [config.example.toml](config.example.toml) in this repository.
It is also installed at `share/rummage/config.example.toml`. Updating rummage does not replace
your personal configuration; compare it with the reference when enabling new features.

## Built-in extraction behavior

The automatic presets prefer unrar for RAR, then 7zz/7z for ZIP/RAR/7z. Other
choices include File Roller, unzip for ZIP, gzip for single gzip files and tar
for tar archives, with 7zz/7z as fallbacks. All these programs are optional. Compressed tar is unpacked in two passes
when using 7-Zip without tar.

Command-line extraction creates a new archive-named directory, or a decompressed file
for a single compressed input. Existing output is retained and the new output gets a
numbered sibling. Sources remain in place. Extract To opens rummage's folder picker;
File Roller supplies its own dialogs when it is the selected backend.

Keep rummage open until command-line extraction finishes. Failure or interruption
can leave partial output in a visible `(extracting)` directory. The 7zz/7z and
unrar backends prompt for passwords, including encrypted headers;
cancelling leaves no output. Other CLI backends do not prompt; use Open With for
those encrypted formats. Failures remain in a scrollable dialog with Copy Details,
diagnostics and any retained output location. Extraction is not added to the file-operation undo history.

## Action fields

| Key | Default | Meaning |
| --- | --- | --- |
| `id` | Required for each table | Unique letters, digits, `_` or `-`; also identifies a preset override |
| `label` | Required for a new action | Visible command name |
| `argv` | Required for a new external action | Executable and literal arguments; placeholders are whole arguments |
| `selection` | `"one"` | `one`, `many`, `background` or `any` |
| `mime` | No filter | Exact types or `type/*`; every selected item must match |
| `placement` | `"actions"` for custom actions | `actions` submenu or top-level `menu`; layouts can override this |
| `execution` | `"handoff"` | The supported external execution mode |
| `enabled` | `true` | Disable a definition without deleting it |
| `destination` | `"none"` | `file` for a save path, `directory` for an existing folder |
| `destination_name` | Empty | Literal suggested filename for `destination = "file"` |
| `notify_on_completion` | `false` | Status message and desktop notification when the launched process exits |

Preset overrides retain omitted preset fields. Copy Location cannot be replaced.
See the [annotated reference](config.example.toml) for all default context layouts
and menu IDs. See [troubleshooting](troubleshooting.md#actions-or-menus-are-missing)
when an action does not appear.
