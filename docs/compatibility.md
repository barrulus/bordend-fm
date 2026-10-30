# Compatibility

How Nordend behaves outside Noctalia and Umbriel (ROADMAP 7.4). One column per desktop, filled
in by the author's tests; "expected" is what the protocols each compositor offers should give,
*to be verified* until tested.

## What depends on the compositor

| Need | Protocol | Without it |
|------|----------|------------|
| Title bar | `xdg-decoration` in server-side mode | Nordend draws its own (client-side) |
| Cursor shapes | `cursor-shape-v1` | Nordend draws them from the xcursor theme (`XCURSOR_THEME`, `XCURSOR_SIZE`) |
| Sharp fractional scaling | `fractional-scale-v1` + `viewporter` | renders at scale 1 |
| Copy and paste, drag and drop with other apps | `wl_data_device_manager` | inside Nordend only |
| Focus for the apps it opens | `xdg-activation-v1` | they may open without focus |

Icons need nothing: the Tabler subset is compiled into the program.

Two variables try Nordend's own paths on a compositor that would not need them:
`NORDEND_FORCE_CSD=1` asks for client-side decorations through xdg-decoration (the compositor
has the last word), `NORDEND_FORCE_XCURSOR=1` draws the cursor from the xcursor theme.

## Expected

| | Umbriel | GNOME | Plasma | niri | weston |
|---|---|---|---|---|---|
| Title bar | the compositor's | Nordend's | the compositor's | Nordend's (niri asks for client-side unless `prefer-no-csd`) | Nordend's |
| Cursor | cursor-shape-v1 | cursor-shape-v1 (recent GNOME) | cursor-shape-v1 | cursor-shape-v1 | xcursor |

## Checklist per session

For each desktop: build in the VM (or on the host for Umbriel), then `./build/nordend ~` and:

1. It starts, with the icons in the sidebar, the views and the status bar.
2. Title bar: the title shows; drag moves the window; double click maximizes and restores;
   minimize, maximize, close work; right click opens the window menu (where there is one).
3. Edges and corners resize the window, with the matching arrow; not when maximized.
4. The cursor shows, and turns into a double arrow on the column dividers.
5. Copy a file in Nordend, paste it in the desktop's file manager, and back.
6. Drag a file from Nordend into another app, and from another app into Nordend.
7. Enter on a photo, a PDF, a text file opens them in their apps, which get the focus.
8. At a fractional scale (125 %, 150 %), text and icons are sharp.

## Results

| Step | Umbriel | GNOME | Plasma | niri | weston (headless) |
|------|---------|-------|--------|------|-------------------|
| 1 start, icons | | | | | yes (2026-10-30, screenshot) |
| 2 title bar | | | | | drawn (screenshot); no pointer to try it |
| 3 resize | | | | | no pointer |
| 4 cursor | | | | | no pointer |
| 5 clipboard | | | | | no clipboard global |
| 6 drag and drop | | | | | no seat |
| 7 open files | | | | | — |
| 8 fractional scale | | | | | no fractional-scale |

Headless weston offers no `wl_seat`: it shows the window, not how it reacts to the pointer.
