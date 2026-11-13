# Desktop compatibility

[Documentation](README.md) · [Installation](install.md) · [File picker](file-picker.md)

rummage requires a Linux Wayland session with xdg-shell and EGL/OpenGL ES 3.0.
Optional protocols and services provide desktop integration with the fallbacks
below. The test matrix later on is a **dated record**, not a current certification
of every compositor/version. Those sessions were not rerun for this documentation update.

## Protocols and fallbacks

| Feature | Protocol/service | Without it |
| --- | --- | --- |
| Title bar | xdg-decoration in server-side mode | rummage draws client-side decorations |
| Cursor shapes | cursor-shape-v1 | Xcursor theme fallback using `XCURSOR_THEME` / `XCURSOR_SIZE` |
| Fractional scaling | fractional-scale-v1 and viewporter | No fractional-scale path; renders at scale 1 |
| Desktop clipboard and drag/drop | wl_data_device_manager | No cross-application file exchange through that protocol |
| Focus for launched applications | xdg-activation-v1 | Applications may open without focus |
| Desktop colours | Settings portal, `org.freedesktop.appearance` | Built-in palette unless `colors.toml` is supplied |
| Picker parent attachment | xdg-foreign-v2 | Independent picker window |
| Picker modal hint | xdg-dialog-v1 | Parenting can work without the modal hint |
| Device mount/eject | udisks2 | No udisks2 operations; mounted-volume discovery remains separate |
| Action popups | Desktop notifications D-Bus service | Completion status still appears in rummage |

The built-in icon font is compiled into the application; external themes are
optional. Application file dialogs require separate
[FileChooser backend selection](file-picker.md), not just a Wayland protocol.

`RUMMAGE_FORCE_CSD=1` requests the client-side title bar (the compositor decides
whether to honor it). `RUMMAGE_FORCE_XCURSOR=1` forces the cursor fallback. These
are useful for testing paths that a compositor would normally bypass.

## Check a new desktop session

Run the original checklist below and also check current additions:

- Desktop colour changes with no palette override, and an explicit icon theme.
- File-picker standalone mode, application portal selection, parent attachment,
  cancellation and concurrent requests.
- Custom-action notifications and external launch focus.
- Multiple outputs/scales, keyboard navigation and clipboard interoperability.

Record the exact compositor/version, GPU/driver, scale, build revision and date.
Mark untested paths explicitly; successful rendering alone does not test input.

## Historical protocol expectations

| | Umbriel | GNOME | Plasma | niri | weston |
|---|---|---|---|---|---|
| Title bar | the compositor's | rummage's | the compositor's | rummage's (niri asks for client-side unless `prefer-no-csd`) | rummage's |
| Cursor | cursor-shape-v1 | cursor-shape-v1 (recent GNOME) | cursor-shape-v1 | cursor-shape-v1 | xcursor |

## Checklist per session

For each desktop: build in the VM (or on the host for Umbriel), then `./build/rummage ~` and:

1. It starts, with the icons in the sidebar, the views and the status bar.
2. Title bar: the title shows; drag moves the window; double click maximizes and restores;
   minimize, maximize, close work; right click opens the window menu (where there is one).
3. Edges and corners resize the window, with the matching arrow; not when maximized.
4. The cursor shows, and turns into a double arrow on the column dividers.
5. Copy a file in rummage, paste it in the desktop's file manager, and back.
6. Drag a file from rummage into another app, and from another app into rummage.
7. Enter on a photo, a PDF, a text file opens them in their apps, which get the focus.
8. At a fractional scale (125 %, 150 %), text and icons are sharp.

## Recorded results: 2026-10-30 and 2026-10-31

| Step | Umbriel | GNOME | Plasma | niri | weston (headless) |
|------|---------|-------|--------|------|-------------------|
| 1 start, icons | yes | yes | yes | yes | yes (2026-10-30, screenshot) |
| 2 title bar | Umbriel's, one bar | rummage's; move, double click, buttons, window menu: yes | KWin's (screenshot), one bar | rummage's | drawn (screenshot); no pointer to try it |
| 3 resize | yes | yes | yes | yes | no pointer |
| 4 cursor | yes (cursor-shape-v1) | yes, double arrow on dividers | yes | yes | no pointer |
| 5 clipboard | yes, with foot and a second rummage | yes, both ways with Nautilus | yes, both ways with Dolphin | yes, with foot and a second rummage | no clipboard global |
| 6 drag and drop | yes, with foot and Firefox | yes, both ways with Nautilus | yes, both ways with Dolphin | yes, with foot and Firefox | no seat |
| 7 open files | yes, focused | yes, apps from `mimeapps.list` (imv, zathura, Okular), focused | yes, focused | yes, focused | — |
| 8 fractional scale | yes | yes, after enabling `scale-monitor-framebuffer` | yes | not tried (nested: the host window's scale) | no fractional-scale |

Umbriel: the author's desktop (pinned commit), tested by the author on 2026-10-31.

GNOME: 48.7 (mutter 48.7) in the Void VM `rummage-dev` (virtio without 3D, Mesa llvmpipe),
tested by the author on 2026-10-31; 249 tests passed in that VM run. The initial
pass used rummage's default colours; the later desktop-colour test below supersedes
that limitation. Fractional scaling
in GNOME 48 is experimental:
`gsettings set org.gnome.mutter experimental-features "['scale-monitor-framebuffer']"`, then log
in again.

Plasma: 6.7.5 (KWin 6.7.5) in the same VM, tested by the author on 2026-10-31.
That test predates the installed desktop entry and recorded a generic Wayland icon.
Current installs include `rummage.desktop` and the matching app icon.

Desktop colours (without `colors.toml`): light/dark and the accent followed live on GNOME and
Plasma, tested by the author on 2026-10-31; Umbriel keeps Noctalia's colours.

niri: 26.04, nested in Umbriel on the host (in the VM niri needs virgl, BUG-017 of
void-noctalia), minimal config without `prefer-no-csd`, tested by the author on 2026-10-31.

Headless weston offers no `wl_seat`: it shows the window, not how it reacts to the pointer.
