# Using rummage

[Documentation](README.md) · [File operations](file-operations.md) · [Shortcuts](shortcuts.md)

Start `rummage` in a Wayland session, or pass a directory such as
`rummage ~/Pictures`. **F1** shows the active keyboard bindings and **Ctrl+K** opens
the command palette. Both reflect configured actions and shortcuts.

## The window

The sidebar contains places, bookmarks, Trash and devices. The breadcrumb above the
files opens any parent in the current path. The status bar shows counts, selection,
free space, operation progress and messages, plus view buttons. The information
pane shows a preview and metadata for the selected item; expand More details for
permissions, owner/group, links and folder counts.

**Ctrl+B** toggles the sidebar. **F6** moves keyboard focus into it and back;
arrows choose a place, Enter opens it and Escape returns to the files.
**Ctrl+I** toggles the information pane in grid/details views. Drag its edge to
resize it (240–720 logical pixels).

## Choose a view

| View | Shortcut | Behavior |
| --- | --- | --- |
| Columns | Ctrl+1 | Selecting a folder opens the next column; Left returns through the hierarchy |
| Grid | Ctrl+2 | Icons/thumbnails with labels; useful for pictures and videos |
| Details | Ctrl+3 | Name, size, modified date and type; folders can expand inline |

Switching views keeps the current location and selection. Columns can be resized
by dragging their edges; double-click an edge to reset its width. In details,
drag header dividers to resize columns and click a header to sort by it; click
again to reverse.

In every view, right-click and use **Sort By** for name, size, modified or type and
ascending/descending order. Sort order belongs to the current tab and is saved. Folders
are kept together; size sorting keeps folder names ordered, and unknown types
sort after known types in ascending type order.

Use **Ctrl+plus**, **Ctrl+minus**, or **Ctrl+wheel** to zoom, and **Ctrl+0** to reset.
Grid sizes range from 48 to 512 px, constrained by available width. Columns and
details share a five-step list zoom; grid has its own zoom. Zoom is per tab.

### Expand folders in details

Click a folder's chevron or press **Right** to expand it. Right again moves into
its first child. **Left** collapses a folder or selects the parent row. **Enter**
or a double-click opens the folder as the new location; **Backspace** goes above
the tree root.

Expanded directories load on demand and follow filesystem changes, sorting and
hidden-file settings. Selection stays within one parent folder, including Shift
ranges, Ctrl+A and rubber-band selection. Expansion survives switching tabs, but
resets on navigation or switching views and is not saved across restarts.

## Navigate and select

Enter or a double-click opens a folder, or opens a file in its default application.
Backspace or Alt+Up goes to the parent. Alt+Left/Right and mouse side buttons go
back/forward through location history. Clicking the breadcrumb also navigates.

Arrow keys move the cursor; Page Up/Down moves by a screen and Home/End reaches
the first/last item. Grid Left/Right moves between cells; columns and details use
those keys for hierarchy navigation.

| Gesture | Selection |
| --- | --- |
| Click | One item |
| Ctrl+click or Ctrl+Space | Toggle an item |
| Shift+click or Shift+movement | A range |
| Ctrl+movement | Move the cursor without selecting |
| Ctrl+A | All visible entries in the current selection scope |
| Escape | Return to one entry, or cancel the active interaction |
| Drag from empty space | Rubber-band selection; Ctrl adds to it |

A drag beginning on an icon or name starts a file drag instead. Rubber-band
selection scrolls when you move beyond the view's top or bottom. Selection and
cursor track entries through live directory updates.

**Ctrl+H** toggles hidden files in the current tab. Hidden entries are dimmed when shown;
when hidden, they are excluded from Select All and operations on that selection.

## Tabs and persistence

**Ctrl+T** opens a tab, **Ctrl+W** closes the current tab (except the last), and
**Ctrl+Tab / Ctrl+Shift+Tab** cycles tabs. **Alt+1…9** selects a numbered tab.
The tab bar appears when there is more than one tab. Click to select, use its ×
or middle-click to close, and use + to open another.

Middle-click a folder to open it in a background tab. If it is already part of a
selection, the selected folders open in background tabs. The context menu offers
Open in New Tab as well.

Each tab keeps its location, view, zoom, sort order, hidden visibility, filename
filter and session history. The sidebar and information pane are shared by the
window. New tabs inherit view/zoom/sort/hidden settings and start with an empty
filter. Pane/tab restoration is described in [saved state](configuration.md#saved-state).

## Two persistent browsing panes

- **F3** or **Two panes** splits the browser. On first use, the second pane starts at the current
  location and receives focus. Each pane has its own tabs, breadcrumb, selections, history,
  scrolling, view modes, zoom, sort order, hidden-file setting and filename filters.
- **Tab** switches panes while browsing. Clicking a pane's header focuses it without disturbing
  its selection. Hovering or scrolling the other pane does not move keyboard focus. The active
  header has an accent and an “Active” label; inactive selections remain visible in subdued colors.
- **Ctrl+T**, **Ctrl+W**, **Ctrl+Tab**, **Ctrl+Shift+Tab** and **Alt+1…9** operate in the active
  pane. The last tab in each pane stays open. New tabs inherit view, zoom, sorting and hidden-file
  visibility, and start with an empty filename filter.
- Drag the divider to resize; double-click it to equalize. **Top / bottom** changes orientation,
  and **Swap panes** exchanges their positions while keeping focus with the same content.
  Column view works inside either pane, just like Grid and Details.
- **Ctrl+F5** copies selected files to the other pane's current folder; **Ctrl+F6** moves them.
  The command palette and **Panes** context submenu show the destination path and direction.
  In expanded Details views, the destination is the breadcrumb's folder, not a selected child.
  Transfers capture both ends when invoked and use the existing conflict handling, sidecar
  support, progress and window-wide undo. Clipboard copy/paste keeps its usual behavior.
- **Open in Other Pane** opens a selected folder in a new tab there. **Show Same Folder in Other
  Pane** opens the current location there. Both focus the destination without replacing its tabs.
  These commands and layout controls are available in the command palette and **Panes** submenu.
- **F3** again shows the active pane full-width, retaining the hidden pane's tabs and selections.
  Cross-pane transfers are unavailable until both panes are visible again. Reopening rummage
  without a folder restores both tab sets, active tabs, focused pane, orientation, divider and
  whether the second pane was hidden. Older single-pane sessions remain readable.
- One sidebar, information panel and transfer status serve the window. The information panel
  follows the active pane; **Ctrl+I** hides it. Each pane also shows its own selection count.
  The file picker stays single-pane. Text fields and dialogs retain their normal Tab handling.

## Filter the current view

The toolbar's Filter field matches filename substrings, ignoring ASCII case, in
columns, grid and details. **Ctrl+Shift+F** focuses it; Enter keeps the filter and
returns focus to files. Escape or Clear removes it. It persists when switching
views/folders and combines with picker type restrictions. New Folder clears the
filter before starting a rename. Use Ctrl+F for recursive search instead.

## Commands and finding files

**Ctrl+K** opens the command palette. Type parts of a command name; letters match
in order, so `nf` can find New Folder. It also offers places and tabs, shows current
shortcuts, and lists recently used commands first when the query is empty.
Only applicable commands appear. Use arrows or Page Up/Down, Enter to run, and
Escape or Ctrl+K to close. Open With opens an application list within the palette.

**Ctrl+F** searches filenames and paths recursively under the current folder.
This is fuzzy path search, not file-content search. A match in a filename ranks
above one spanning its parent directories. Enter reveals a result in the browser,
Space previews it, and Shift+Enter opens it in its default application.

Search excludes hidden entries unless shown, does not follow symlink directories
or cross into other mounted filesystems, and stops at 300,000 entries with a
message. Closing the search releases its index.

## Places, bookmarks and devices

Home folders follow XDG user-directory settings. **Ctrl+D** bookmarks the current
folder; Add to Sidebar in the context menu bookmarks selected folders. Dragging
folders onto the BOOKMARKS heading or the end of its list also adds them. A
right-click removes a bookmark. rummage shares the GTK bookmarks file and keeps
unrelated entries and labels; reordering/renaming bookmarks is not implemented.

With udisks2 available, unmounted internal partitions and removable disks appear
in the sidebar without mounting themselves. Click to mount and open, with
authorization requested when required; use the context menu to mount, unmount or eject. Ejecting an external
disk unmounts its partitions and powers it off when supported. Busy or read-only
devices report errors in the status bar. Encrypted-device unlocking is not
implemented. Already mounted volumes can still appear without udisks2.

## Sidebar zoom and properties

Use Ctrl+wheel over the sidebar, or F6 then Ctrl+plus/minus/0, to adjust its zoom
independently of file lists. Its size is remembered across restarts. Status bar
text has its own [configuration setting](configuration.md#settings-reference).

**Alt+Enter** or Properties in the context menu shows metadata for one entry,
or the current folder from the background menu. Metadata is read-only. Folders
also offer Change Icon; see [folder icons](appearance.md#folder-icons-and-properties).

## Preview or work with files

Press **Space** for Quick Look; use arrows to walk through files and Space/Escape
to close it. See [previews](previews.md) for image/text support and video limits.
See [file operations](file-operations.md) for conflicts, clipboard, drag/drop,
sidecars and undo, or [actions](actions.md) to add external commands.
