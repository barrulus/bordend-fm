# Keyboard shortcuts

[Documentation](README.md) · [Using rummage](usage.md) · [Configuration](configuration.md)

These are defaults. **F1** (or `?`) shows the active bindings, including custom
ones. Editors, dialogs, Quick Look and file pickers have their own input context;
a shortcut's meaning follows the focused control.

## Views, navigation and search

| Keys | Action | Configurable ID |
| --- | --- | --- |
| Ctrl+1 / Ctrl+2 / Ctrl+3 | Columns / grid / details | `columns_view`, `grid_view`, `details_view` |
| Ctrl+I | Toggle information pane in grid/details | `toggle_info` |
| Alt+J / Alt+K | Scroll the information pane's text page down / up by three lines | `scroll_preview_down`, `scroll_preview_up` |
| Alt+Page Down / Alt+Page Up | Scroll the information pane's text page by a page | `scroll_preview_page_down`, `scroll_preview_page_up` |
| Ctrl+B | Toggle sidebar | `toggle_sidebar` |
| F6 | Focus sidebar / return to view | `focus_sidebar` |
| Ctrl+H | Show/hide hidden files | `toggle_hidden` |
| Ctrl+D | Bookmark current folder | `add_bookmark` |
| Ctrl+K | Command palette | `command_palette` |
| Ctrl+Shift+F | Filter filenames in the view | `filter_names` |
| Ctrl+F | Find files under current folder | `find_files` |
| Space | Quick Look | `quick_look` |
| F1 or ? | Shortcut overlay | `show_shortcuts` |
| Menu or Shift+F10 | Context menu | `context_menu` |
| Alt+Left / Alt+Right | History back / forward | `back`, `forward` |
| Enter | Open selected file/folder | Fixed |
| Backspace or Alt+Up | Parent folder | Fixed |
| Alt+Down | Open in grid/details | Fixed |
| Right / Left | Column hierarchy; expand/collapse/parent in details | Fixed |
| Arrows, Page Up/Down, Home/End | Move through entries | Fixed |
| Ctrl+plus / Ctrl+minus / Ctrl+wheel | Increase/decrease zoom | Fixed |
| Ctrl+0 | Reset zoom | Fixed |
| Ctrl+Q | Quit | Fixed |

## Panes

| Keys | Action | Configurable ID |
| --- | --- | --- |
| F3 | Toggle two panes / active pane full-width | `toggle_split` |
| Tab | Switch active pane while browsing | `switch_pane` |
| Ctrl+F5 | Copy selection to the other pane | `copy_other_pane` |
| Ctrl+F6 | Move selection to the other pane | `move_other_pane` |
| No default | Swap pane positions | `swap_panes` |
| No default | Change side-by-side / top-bottom orientation | `orient_panes` |
| No default | Open selected folder in the other pane | `open_other_pane` |
| No default | Show current folder in the other pane | `same_other_pane` |

Tab retains its normal focus behavior in text fields and dialogs. The picker stays
single-pane. Cross-pane transfers require both panes to be visible. Tabs and file
commands below operate in the active pane.

## Tabs and selection

| Keys | Action | Configurable ID |
| --- | --- | --- |
| Ctrl+T | New tab | `new_tab` |
| Ctrl+W | Close current tab, except the last | `close_tab` |
| Ctrl+Tab / Ctrl+Shift+Tab | Next / previous tab | `next_tab`, `previous_tab` |
| Alt+1…9 | Select numbered tab | Fixed |
| Ctrl+A | Select all in the current scope | `select_all` |
| Ctrl+Space | Toggle selected item | `toggle_selection` |
| Shift+movement | Extend selection | Fixed |
| Ctrl+movement | Move without selecting | Fixed |
| Escape | Back to one entry/cancel current interaction | Fixed |

Expanded details-tree selections stay within a single parent folder. Mouse
selection gestures are covered in [Using rummage](usage.md#navigate-and-select).

## Files

| Keys | Action | Configurable ID |
| --- | --- | --- |
| Alt+Enter | Properties / folder icon selection | `properties` |
| F2 | Rename | `rename` |
| Ctrl+Shift+N | New folder | `new_folder` |
| Ctrl+C / Ctrl+X / Ctrl+V | Copy / cut / paste | `copy`, `cut`, `paste` |
| Delete | Trash; permanent delete when already in Trash | `trash` |
| Shift+Delete | Delete permanently with confirmation | `erase` |
| Ctrl+Z / Ctrl+Shift+Z | Undo / redo | `undo`, `redo` |
| F4 | Open terminal in folder | `open_terminal` |

Copy Location, Extract Here and Extract To have no default shortcuts. Bind their
IDs `copy_location`, `extract_here`, `extract_to` or `action:<id>` explicitly.

## Context-specific keys

| Context | Keys |
| --- | --- |
| Quick Look | Space/Escape closes; arrows change file |
| Text Quick Look | Home/End, Page Up/Down scroll; Ctrl+A selects text; Ctrl+C copies selection; Shift+wheel scrolls horizontally |
| Palette/search | Arrows/Page Up/Down choose; Enter runs/reveals; Escape closes |
| File search | Space previews; Shift+Enter opens in application |
| Rename editor | Enter accepts, Escape cancels, Ctrl+arrows moves words, Ctrl+V pastes, Ctrl+Z / Ctrl+Shift+Z edits undo history |
| Conflict dialog | R replaces, S skips, Enter keeps both, A applies to rest, Escape stops |
| Permanent-delete dialog | Enter confirms, Escape cancels |
| Context menu | Up/Down chooses, Right enters a submenu, Left/Escape backs out, Enter runs |
| File picker | Ctrl+Enter accepts; Tab/Shift+Tab cycles controls; Escape closes transient UI before cancelling |

## Override bindings

Add to one `[keybindings]` table in `config.toml`:

```toml
[keybindings]
rename = "Ctrl+r"
select_all = "Ctrl+g"
toggle_selection = "Ctrl+j"
erase = ""
copy_location = "Ctrl+Shift+c"
```

Each entry replaces all default chords for that action. An empty string disables
it, and an omitted entry retains defaults. Use modifiers `Ctrl`, `Shift`, `Alt`,
`Super` and XKB key names; key names are case-insensitive, modifier spellings are
as shown. Use `Ctrl+plus`, not `Ctrl++`, for a literal plus key.

Custom actions use quoted keys such as `"action:compress" = "Ctrl+Alt+z"`; first
define the action using the [actions guide](actions.md). Fixed navigation,
range-selection, numbered-tab, quit and editor/dialog keys cannot be reassigned
through this table. Choose chords that do not collide with them.

Unknown IDs, invalid chords and conflicting bindings reject the binding set.
Actions, menus and bindings reload together; the prior valid set survives an
invalid reload. Run `rummage --check-config` and check F1 after editing.
