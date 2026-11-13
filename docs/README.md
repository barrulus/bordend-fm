# rummage documentation

[Project overview](../README.md)

These guides describe rummage's current source. The version remains 0.5.0 while
this fork develops beyond upstream Nordend's release. Use the source commit and
package identity when reporting behavior. See [installation](install.md),
[release history](releases.md) and [package validation](ci-packaging.md).

## Get started

| Task | Guide |
| --- | --- |
| Install, build or update | [Installation](install.md) |
| Learn views, tabs, search and selection | [Using rummage](usage.md) |
| Copy, move, rename, trash or restore files | [File operations](file-operations.md) |
| Preview pictures, raw photos, text and videos | [Previews and formats](previews.md) |
| See supported capabilities and limits | [Feature overview](features.md) |
| Fix a problem or prepare a report | [Troubleshooting](troubleshooting.md) |

## Configure and integrate

| Task | Reference |
| --- | --- |
| Change settings or understand saved state | [Configuration](configuration.md) |
| Copy a complete annotated settings reference | [config.example.toml](config.example.toml) |
| Change colours, fonts, icons or transparency | [Appearance](appearance.md) |
| Add commands, destination dialogs or menu groups | [Actions and context menus](actions.md) |
| Look up default keys or override bindings | [Keyboard shortcuts](shortcuts.md) |
| Use command-line options or diagnostic variables | [Command line](command-line.md) |
| Replace application Open/Save dialogs | [File picker and portal](file-picker.md) |
| Understand compositor fallbacks and past testing | [Desktop compatibility](compatibility.md) |

## Develop

- [Contributing](../CONTRIBUTING.md): changes, verification and documentation expectations.
- [Development](development.md): builds, tests, sanitizers, fuzzers and diagnostic tools.
- [Architecture](architecture.md): modules, data flow, threads and extension points.
- [Security model](security.md): preview isolation, file operations and external programs.
- [SVG renderer validation](svg-renderer-validation.md): the current renderer acceptance policy.
- [CI and package validation](ci-packaging.md): build artifacts and development publishing.
- [Third-party material](../THIRD_PARTY.md): bundled sources and licenses.

## History and plans

These records provide context; dated results and proposals are not current feature promises.

- [Release history](releases.md) and [roadmap](roadmap.md).
- [Development log](devlog.md): decisions and implementation history, newest first.
- [Benchmarks](benchmarks.md): dated results and reproduction commands.
- [Upstream bug log](buglog.md): recorded Noctalia/Umbriel issues and their last known status.
- [SVG renderer brief](svg-renderer-brief.md): original implementation plan, with superseded criteria marked.
- [Luciole comparison](luciole-comparison.md): a source review at the revisions stated in the document.

Use the topic guide for current behavior, source and tests for implementation detail,
and dated records to understand why it changed. Documentation maintenance conventions
are in [Contributing](../CONTRIBUTING.md#documentation).
