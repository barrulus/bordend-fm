# File operations

[Documentation](README.md) · [Using rummage](usage.md) · [Actions and archives](actions.md)

File operations are available through shortcuts, context menus and the command
palette. Errors and progress appear in the status bar. Right-clicking an entry
selects it unless it is already part of the selection.

## Open files and terminals

Enter or a double-click opens files using desktop MIME associations. Open With
lists associated applications, default first, and can open the selection with a
chosen application. Several files for the same app can share one launch. An
executable file is opened by its associated application; selecting it does not
implicitly execute it.

Applications run independently of rummage. Terminal applications use `$TERMINAL`,
`xdg-terminal-exec`, or a discovered emulator. **F4** opens a terminal in the
folder under the cursor, otherwise the current folder. The context menu also
offers Open in Terminal. Launch failures appear in the status bar.

## Create and rename

**Ctrl+Shift+N** creates a folder with its name ready to edit. The background
context menu's **New File** creates an empty file or copies a template from your
XDG Templates directory. It does not overwrite an existing file.

**F2** renames one entry in place, initially selecting the name without its
extension. Enter or a click elsewhere accepts; Escape cancels. Text editing
supports compose input, word movement, selection, clipboard paste and local
undo/redo. Paste uses the first line of clipboard text. While editing a name,
Ctrl+Z edits that text rather than undoing a file operation.

## Copy, move and conflicts

**Ctrl+C / Ctrl+X** copies/cuts the selection; **Ctrl+V** pastes into the current
folder. Cut entries fade until pasted or cancelled with Escape. File-manager
clipboard formats allow exchange with other desktop applications; only local
file paths/URIs are accepted for transfers.

Transfers run outside the UI thread, with progress and a stop button. Copies use
temporary names before publishing completed output. A cross-filesystem move
removes its source only after its copy is flushed. Dates, permissions and symbolic
links are preserved, and supported darktable sidecars travel with the photo.
Moving a folder into itself or copying more than the available space is refused.

When a destination name exists:

| Choice | Result |
| --- | --- |
| Replace | Replace after the new file is complete; directories are merged |
| Skip | Keep the destination and omit that source |
| Keep both | Give the incoming item a numbered sibling name |
| Apply to the rest | Reuse the selected policy for later conflicts |

Pasting into the source folder automatically keeps both. Cancellation stops work;
items already completed can remain. Temporary handling does not make a multi-file
operation one atomic transaction. Check the status message for partial failures.

## Drag and drop

Drag an entry by its icon or name. A selected entry drags its selection. Targets
include folders in each view, view background, breadcrumb segments and sidebar
places; dropping onto Trash trashes the files.

Within rummage, the default is move on the same filesystem and copy across
filesystems. Ctrl requests copy; Shift requests move. It uses the transfer engine,
including conflict handling, sidecars, progress and undo. Dropping into the same
location has no effect; a folder cannot be dropped inside itself.

External drops into rummage copy local files. Drops out of rummage offer file URIs
and paths for copying; rummage does not delete its source because another program
claims it moved it. Web URLs are not local files. Dragging near a view's edge
scrolls; hovering does not automatically open a folder.

## Trash and restore

**Delete** moves files to freedesktop Trash. Files on the home filesystem use the
home trash; other filesystems use their own trash where permitted. If a trash
cannot be created, the files remain and rummage reports why; it does not silently
replace trashing with permanent deletion.

The sidebar's **Trash** combines entries across the discovered trash locations.
It shows original names, original paths and deletion dates, with previews. Files
cannot be opened, created, renamed or pasted there.

Right-click **Restore** to return an entry to its original path, recreating missing
parent directories and asking about name conflicts. Restore does not merge a
conflicting directory. Supported trashed sidecars return with their photo.

## Permanent deletion

**Shift+Delete** permanently deletes from a normal folder. **Delete** in Trash
also means permanent deletion. **Empty Trash** is available from its background
or sidebar menu and covers the discovered trash locations.

These actions ask for confirmation. Folder contents are counted for the size and
progress display. Deletion runs in the background and can be stopped, but files
already deleted are gone. Symbolic links are deleted without following them.
Permanent deletion cannot be undone.

## Undo and redo

**Ctrl+Z** undoes the last supported operation; **Ctrl+Shift+Z** redoes it.

| Operation | Undo behavior |
| --- | --- |
| Rename | Restore the previous name, including associated sidecars |
| Move | Move back, including across filesystems |
| Trash | Restore |
| Restore | Trash again |
| Copy | Trash the copies |
| Create folder/file | Trash it only if still empty/unchanged as appropriate |
| Permanent delete | Cannot undo |
| External action or archive extraction | Not recorded in file-operation undo |

Undo is session history, not a backup system. Changes made meanwhile can prevent
an inverse operation; rummage skips affected items and explains the result. Undoing
a replacement does not imply recovery of overwritten destination data.

## darktable sidecars

Recognized darktable `.xmp` sidecars follow their photo through rename, copy, move,
trash and restore, including conflict-generated names. This is file handling,
not a darktable database integration. The current photo reader does **not** import
darktable star ratings; older documents described behavior removed in 0.4.0.

For compression, extraction, image conversion and other external tools, see
[actions and context menus](actions.md). External commands own their output and
overwrite behavior; they do not inherit the built-in transfer engine's guarantees.
