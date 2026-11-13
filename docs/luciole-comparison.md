# Nordend and Luciole: what we can share and learn

[Documentation](README.md) · [Feature overview](features.md) · [Roadmap](roadmap.md)

This is a dated comparative source review, not a continuously maintained feature
matrix or a promise of planned work. Use the current topic guides for Nordend's
behavior; local paths and revisions below identify the review environment.

Reviewed on 2026-11-12 against the supplied conversation and both local working trees:

- Nordend: `841d54f`, including the existing uncommitted additions.
- Luciole: `df108d8` on `main`, at `/home/barrulus/dev/luciole`.

This is a source review, not a runtime reproduction of the reported bugs. The
latest pass checked the changed implementations, documentation, and regression
test sources; it did not rebuild Luciole or run its tests. The conversation's
referenced screenshots were not supplied.

There is a useful exchange here: Nordend can share desktop integration and
rendering validation; Luciole can teach us about discoverable features and broader
file-management workflows.

## What Nordend can teach Luciole

### File picker and XDG FileChooser portal

Nordend has an entire file picker backed by its normal browser views, sidebar,
and previews. It provides OpenFile, SaveFile, and SaveFiles through an XDG
FileChooser portal backend, with single/multiple file and folder selection,
initial locations and names, glob/MIME filters, application-provided choices,
overwrite confirmation, and cancellation. Picker windows leave the file
manager's saved tabs and window state alone.

The integration includes D-Bus activation, concurrent requests and request
cancellation, Wayland parent-window attachment, standalone command-line picker
modes, and a NixOS module that selects Nordend for FileChooser while preserving
other portal backends. Applications using the portal can therefore use Nordend
for file selection across the desktop.

No equivalent picker mode or FileChooser backend was found in Luciole's source,
documentation, or packaging at the reviewed revision. This is a substantial
capability to share: the browser/picker separation, request/result protocol,
portal lifecycle, packaging, and regression fixtures. The implementation is
present in Nordend's working tree, including the uncommitted additions noted
above.

The tests cover real browser selection and overwrite behavior, plus backend
responses, URI escaping, filters/options, concurrent requests, cancellation,
malformed input, and child failures on a private D-Bus session. They were
inspected, not rerun for this comparison. Current limits include independent
windows for unsupported parent handles and MIME filtering based on entry
classification/filename rules rather than content inspection.

References:

- [Picker setup and behavior](file-picker.md).
- [Picker UI](../src/views/src/file_picker.cpp) and
  [request/result protocol](../src/core/include/rummage/core/file_picker.hpp).
- [Portal backend](../src/platform/src/file_picker_portal.cpp) and
  [NixOS integration](../nix/module.nix).
- [Picker tests](../tests/file_picker_test.cpp) and
  [portal integration tests](../tests/file_picker_portal_test.cpp).

### Missing Wine in Open With

There is a strong lead: Luciole applies `g_app_info_should_show()` to both
suggested and “other” applications. That can exclude `NoDisplay=true` MIME
handlers. Nordend's association lookup distinguishes unavailable/hidden entries
without this application-menu filter.

`NoDisplay` explicitly permits MIME-handler use. This is a likely explanation,
pending examination of Posty's Wine desktop entry; it is not a confirmed
reproduction of that user's failure.

References:

- Nordend: [association lookup](../src/platform/src/applications.cpp).
- Luciole: `src/platform/file_launcher.cpp`, particularly the `append` filter.
- [Desktop Entry specification](https://specifications.freedesktop.org/desktop-entry/latest/recognized-keys.html).
- [GIO should_show documentation](https://docs.gtk.org/gio/method.AppInfo.should_show.html).

### Shift+Delete

Confirmed in the checkout: Luciole consumes Shift+Delete outside Trash without
doing anything; permanent deletion is restricted to Trash. Nordend has the
complete workflow: confirmation, background counting, progress, cancellation,
and symlink handling.

References:

- Nordend: [documented file operations](file-operations.md).
- Luciole: `src/app/page/file_keyboard_controller.cpp` and
  `src/browser/action_registry.cpp`.

### SVG correctness and validation

Share Nordend's original fixtures, CSS/`currentColor` cases, gradients, masks,
alpha handling, resource limits, and native-size review method. Luciole already
uses librsvg, so adopting our renderer is not necessary to benefit. Its separate
tinting policy should preserve authored colors and distinguish symbolic icons
from full-color artwork.

Our strongest claim is extensive semantic, visual, performance, and fuzz
validation—not universal pixel parity. The accepted validation policy explicitly
supersedes that requirement. Fuzz inputs and file/color pixel comparisons are
different evidence; keep those numbers separate when presenting the work.

References:

- Nordend: [accepted validation policy](svg-renderer-validation.md),
  [SVG fixtures](../tests/data/svg), and [validation history](devlog.md).
- Luciole: `src/imaging/svg_rasterizer.cpp`,
  `src/thumbnails/file_icon_cache.cpp`, and `src/ui/browser/file_icon.cpp`.

### Dialogs that retain work

Luciole creates centered dialogs through its popup machinery and closes them on
popup dismissal. Nordend's confirmation card lives within the main rendering
surface. That offers a useful model for dialogs that should survive focus
changes. Menus and forms need different dismissal behavior.

This identifies a relevant architectural difference; compositor-specific focus
behavior still needs runtime verification.

References:

- Nordend: [confirmation card](../src/views/src/confirm_dialog.cpp).
- Luciole: `src/app/page/browser_page.cpp` (`showCenteredDialog` and `showPopup`)
  and `lib/wayland/popup_surface.cpp`.

### Trash and session continuity

Nordend's native freedesktop Trash implementation offers a
dependency-independent reference and interoperability tests. We also restore
tabs across restarts; Luciole's browsing documentation explicitly says session
restoration is unfinished. Both are concrete things to share.

References:

- Nordend: [Trash implementation](../src/fs/src/trash.cpp),
  [Trash tests](../tests/trash_test.cpp), and [features](features.md).
- Luciole: `docs/user/browsing.md` and `docs/user/file-operations.md`.

## What Nordend can learn from Luciole

### Visible terminal preferences

Luciole offers a terminal setting that applies live across folder terminals,
Run in Terminal, and terminal-based application handlers. It supports more
terminal variants and reports explicit missing or malformed preferences.
Nordend has fallback selection but could borrow this visible configuration.

References:

- Nordend: [terminal implementation](../src/platform/src/exec.cpp).
- Luciole: `src/platform/terminal_command.cpp`,
  `tests/program_launcher_test.cpp`, and `docs/user/configuration.md`.

### Scripts that are easy to discover

“Scripts → Open Scripts Folder” is approachable: install an executable and it
appears. Nordend's current custom actions already offer richer matching,
arguments, destinations, and notifications, but require configuration. Borrow
the simple entry point and concise user documentation, plus Luciole's ownership
checks and selection limits.

References:

- Nordend: [custom action examples](config.example.toml).
- Luciole: `docs/user/scripts.md`, `src/platform/program_launcher.cpp`, and
  `src/app/page/execution_controller.cpp`.

### Explicit program execution

Nordend deliberately opens executables through their file association. Luciole
provides separate Run as a Program / Run in Terminal actions,
executable/shebang checks, and a one-use `.desktop` confirmation that displays
the command. This is a useful reference if we add execution. Double-click
behavior can remain predictable.

References:

- Luciole: `src/app/page/execution_controller.cpp`,
  `src/platform/program_launcher.cpp`, and `docs/user/scripts.md`.

### Everyday management tools

Luciole has split panes, batch rename, editable permissions and POSIX ACLs, and
integrated archive browsing/compression/extraction. These go beyond Nordend's
current archive handoff and file-information display. Split panes and batch
rename look particularly valuable to study.

References:

- Luciole: `docs/user/browsing.md`, `docs/user/file-operations.md`,
  `src/app/page/batch_rename_controller.cpp`, and `src/ui/controls/acl_editor.cpp`.
- Nordend: [actions and archive handoff](actions.md).

### Broader previews

Luciole includes PDF first-page previews, audio artwork, and richer media
metadata through external providers. Nordend has deeper photo/raw and text
handling. Those are complementary strengths.

References:

- Luciole: `src/thumbnails/thumbnail_provider.cpp` and
  `src/thumbnails/metadata_probe.cpp`.
- Nordend: [preview features](previews.md).

### Thumbnail freshness and cache management

Luciole checks file identity and high-resolution timestamps before and after
decoding, validates optional size metadata, reads larger cache classes, and
tracks its own cache writes for bounded cleanup. Those are improvements worth
examining against Nordend's simpler URI/mtime cache path.

References:

- Luciole: `src/thumbnails/thumbnail_service.cpp` and
  `src/thumbnails/thumbnail_store.cpp`.
- Nordend: [thumbnail cache](../src/thumbs/src/cache.cpp).

### Filtering as a distinct interaction

Luciole exposes `typing_mode = "filter"` alongside search and typeahead.
Filtering applies a literal, case-insensitive name substring to the live current-folder listing;
it does not start recursive search. The field's label and Ctrl+F scope follow
the selected mode, and clearing the query restores the listing.

The controller applies the query to its existing model, with regression cases
in `testCurrentFolderFilter`. This is a concrete interaction and implementation
to study for Nordend; no latency benchmark was run in this review.

References:

- Luciole: `src/config/settings.cpp`, `src/browser/browser_controller.cpp`,
  `tests/browser_controller_test.cpp`, and `docs/user/browsing.md`.
- Nordend: [search behavior](usage.md#commands-and-finding-files) and
  [listing filter integration](../src/views/src/browser.cpp).

## Thumbnail interoperability: joint work

The thumbnail incident deserves joint work rather than a claim that either
implementation has solved everything.

### Black thumbnails need reproduction

Both projects use the shared thumbnail cache. Luciole already validates metadata,
preserves RGBA in its application thumbnail path, and writes atomically. We cannot
attribute the black thumbnails to missing basics without reproducing them.

The missing icon outline also needs separating from the actual cached image.
The application thumbnail implementation under `src/thumbnails/` is the relevant
path; conclusions should not be drawn solely from similarly named library code.

### A useful shared interoperability fixture

1. Seed thumbnails from another file manager.
2. Open the same files in each application and check valid cache entries remain
   usable.
3. Compare alpha and decoded pixels.
4. Repeat on CIFS and NFS mounts.
5. Include dark opening video frames, mismatched cache sizes, and files changing
   during decoding.
6. Check cache cleanup preserves entries that the application does not own or
   that another application has replaced.

## Contribution access

Their contribution-access problem does not require giving everyone upstream
write access. Private forks and pull requests are supported when access and
organization/repository policies permit them; organization-level permission must
allow repository-level forking. See
[GitHub's forking policy documentation](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/managing-the-forking-policy-for-your-repository).

## Suggested priorities

- Share the file picker and FileChooser portal architecture, integration, and
  test fixtures as a major capability absent from the reviewed Luciole checkout.
- Share the remaining Wine chooser finding, Shift+Delete workflow, and original
  SVG fixtures.
- Bring instant current-folder filtering, visible terminal preferences, script
  discoverability, explicit execution actions, and thumbnail freshness checks
  back into Nordend.
- Build shared thumbnail interoperability cases before drawing conclusions about
  the reported cache damage.
- Study split panes and batch rename as the next broader workflow additions.
