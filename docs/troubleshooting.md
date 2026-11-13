# Troubleshooting

[Documentation](README.md) · [Installation](install.md) · [Configuration](configuration.md)

Start by identifying the binary and validating the settings it reads:

```sh
command -v rummage
rummage --version
rummage --about
rummage --check-config
```

An installed release and this checkout can both say 0.5.0 while exposing different
features. Include the commit and whether the source has local changes in reports.

## The build fails or tests are missing

Check [build dependencies](install.md#build-dependencies), including recent Wayland protocol XML
files, libheif 1.23+, libde265 and Little CMS 2. FFmpeg 6.1+ development libraries are needed only with `RUMMAGE_MEDIA=ON`. `nix develop` supplies
the tree's declared dependency environment.

If CMake reports doctest missing, it skips the main test targets. A `ctest` result
with no tests is not a pass. Install doctest, configure again and inspect
`ctest --test-dir build -N`. D-Bus integration tests need `dbus-run-session`.

Offscreen tests still need fonts, MIME data and working EGL/GLES. Host allocator
and filesystem differences can affect tests; the Nix package documents its
specific accommodations. See [development](development.md#tests).

## The window does not open

Run from a terminal inside the intended Wayland session:

```sh
RUMMAGE_LOG=debug rummage ~/Pictures 2> /tmp/rummage-debug.log
```

Check that `WAYLAND_DISPLAY` and `XDG_RUNTIME_DIR` belong to that session and that
an EGL/OpenGL ES 3.0 driver is available. X11-only sessions are unsupported.
For Nix builds on non-NixOS, verify host graphics-driver integration. The log
should distinguish a Wayland connection failure from graphics/font initialization.

For title-bar/cursor problems try `RUMMAGE_FORCE_CSD=1` or
`RUMMAGE_FORCE_XCURSOR=1` as described in [compatibility](compatibility.md).

## Configuration changes do not apply

Run `--check-config` with the same `--config FILE` argument as the application.
A missing config is valid and uses defaults; inspect the reported path. Keep one
table per `[keybindings]` or `[context_menu]`, with complete `[[actions]]` blocks.

Fonts require a restart. Desktop icon-theme discovery also happens at startup;
explicit icon-theme edits reload live. Invalid actions/menus/bindings retain the
prior valid set on reload, so a still-visible old menu can mean a rejected edit.
Read stderr and compare with the [current example](config.example.toml), not an old
personal copy. `--config` does not relocate `colors.toml` or saved state.

## Actions or menus are missing

1. Validate the file with `rummage --check-config`.
2. Check `enabled`, the selection rule and MIME filters. `one` is the default;
   multiple files require `many`, and every selected item must match.
3. Confirm the executable is on the application's `PATH`, or use an absolute path.
   Missing programs are dimmed in menus and omitted from the palette.
4. Check menu overrides. A specific context overrides common `items`; an empty
   array hides that context. Omitting `{ remaining = true }` hides unlisted items.
5. Use a normal browser folder: Trash excludes actions, and picker mode excludes
   external actions. Menu layout changes do not remove palette/shortcut access.

Use `action:<id>` in layout references and custom binding names. Define custom
actions before adding references to them. Placeholder arguments must be whole
arguments and compatible with the selection/destination rule.

Completion notifications track the launched process only. Keep rummage open;
check for a notification service if status messages appear without popups. A GUI
launcher can exit before delegated work finishes. See [actions](actions.md).

## Archive extraction fails

Install an applicable backend (unrar, 7zz/7z, File Roller, unzip, gzip or tar), and
check the status message. Multiple selections need a backend for every archive.
7zz/7z and unrar can ask for passwords; other CLI backends need Open With in
an archive application for encrypted input. The persistent error dialog has Copy Details.
Keep rummage open while extraction runs. Inspect a retained `(extracting)` directory
after failure instead of assuming the archive completed.

## Previews are unavailable

Check `rummage --about` for the base engine. An installation needs both
`libexec/rummage/rummage-preview` and `share/rummage/engines/base.toml`; in a build
tree they sit beside the application and in `engines/` respectively. Copying only
the main binary loses discovery. A protocol mismatch or missing helper is logged.

Check [supported formats](previews.md). Not every raw file has a readable embedded
preview. HEIC needs libheif’s libde265 decoder, loaded before the preview sandbox.
Images exceeding decoder
limits, damaged inputs or helper timeouts produce a reason rather than a picture.
Video stills need the optional media helper and compatible FFmpeg libraries.
Check engine registration and restart after installation; see [previews](previews.md).

Use debug logging to distinguish discovery, sandbox and decode failures. A kernel
without Landlock can still use seccomp; failure to establish seccomp stops the
helper. Do not use the sandbox reporting mode as a permanent workaround.

For a stale thumbnail, first verify the underlying file changed and has the
expected modification metadata. The shared thumbnail cache also records failures;
remove only the relevant cached entry if investigating cache behavior, bearing in
mind other file managers use the same cache.

## Colours, icons or text are wrong

An existing `colors.toml` overrides desktop colours. Confirm Noctalia generated
the file, or move it aside to try the settings portal. Re-selecting a Noctalia
theme triggers template output. Check [appearance](appearance.md) for paths.

Use `icon.theme = "builtin"` to distinguish theme-loading problems from rendering
problems. Confirm an explicit theme is installed and that custom icon paths are
absolute. A desktop dock may cache the accent application icon. For text, check
Fontconfig resolution with `fc-match sans-serif` and `fc-match monospace` and
restart after font changes.

If all content is translucent, check the compositor's window-opacity rule.
rummage's `background_opacity` affects backgrounds only; blur is a compositor effect.

## Applications do not use the picker

First test `rummage --pick ~/Pictures`. This establishes standalone picker startup,
not portal routing. Then check the [portal setup](file-picker.md):

- rummage's `.portal` and D-Bus service files must be discoverable.
- The active desktop's FileChooser preference must select `rummage`.
- Desktop-specific portal configuration can take precedence over common settings.
- The application must actually use the portal; its own dialog remains its own.

On systemd desktops, inspect frontend logs with
`journalctl --user -u xdg-desktop-portal.service -b`. After configuration changes,
log in again or restart the portal frontend when no dialog/screen-sharing session
is active, then restart the requesting application. Preserve other backends for
screenshots, screen sharing and settings. The folder MIME association does not
control file dialogs.

## File operations or devices fail

Read the status message for missing permissions, space, read-only mounts or a busy
device. Trashing needs a writable trash location on the applicable filesystem;
rummage leaves files in place if it cannot create one. An external program's
changes cannot be undone through rummage.

Device operations need udisks2 and the desktop's authorization facilities.
Encrypted-device unlocking is not implemented. Close applications using a device
before retrying an unmount/eject. For unexpected restored tabs or layout, reset
only the [saved state](configuration.md#saved-state) after closing rummage.

## Prepare a useful report

Include the exact reproduction steps, expected and actual result, view/mode,
source revision/package, `--about`, relevant configuration and stderr. Note
compositor, scaling and whether hardware or software rendering is in use. For
format bugs include a minimal shareable sample, camera/encoder details and whether
the error is in thumbnails, the information pane or Quick Look.

Logs, paths, screenshots and sample files may contain private information; review
them before posting. Historical upstream issues are recorded in
[the bug log](buglog.md), with status as of their entries rather than a live tracker.
