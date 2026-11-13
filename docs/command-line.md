# Command line

[Documentation](README.md) · [File picker](file-picker.md) · [Development tools](development.md#diagnostic-tools)

## Browser and diagnostics

```text
rummage [--config FILE] [DIR]
rummage [--config FILE] --check-config
rummage --version
rummage --about
rummage --help
```

| Argument | Effect |
| --- | --- |
| `DIR` | Open one directory; without it, follow `startup.mode` (default: Home or restored tabs/panes) |
| `--config FILE` | Select a settings file; does not relocate state or palette |
| `--check-config` | Validate settings, actions, menus and bindings without a window |
| `--version` | Print the compiled project version |
| `--about` | Print version/commit, system, desktop, GPU and discovered engines for a report |
| `--help`, `-h` | Print usage |
| `--test-scene` | Open the developer renderer/input test scene |

`--about` probes the GPU with a surfaceless context; failures can leave GPU details
unavailable. An uncommitted source change is not encoded in the version string.
The command line accepts one positional directory and does not implement a `--`
option terminator; use an absolute path for a directory whose name starts with `-`.

## Standalone picker

```sh
rummage --pick ~/Pictures
rummage --pick --multiple ~/Pictures
rummage --pick-directory ~
rummage --pick-directory --multiple ~
rummage --save-file --name Untitled.txt --title 'Save notes' ~/Documents
```

Put the mode before its modifiers. `--multiple` is for open modes, not Save File.
`--name NAME` supplies an initial filename and `--title TEXT` a window title.
Selected absolute paths are **NUL-separated on stdout**. A save picker returns a
destination; it does not write data. Do not store NUL output in a shell variable.

For human-readable output only:

```sh
rummage --pick --multiple ~/Pictures | tr '\0' '\n'
```

Newline conversion loses unambiguous filename boundaries and a pipeline's status
may hide cancellation. For scripts, capture output and status separately:

```sh
selection_file=$(mktemp)
if rummage --pick --multiple ~/Pictures > "$selection_file"; then
    python3 - "$selection_file" <<'PY'
import os
import pathlib
import sys
for path in pathlib.Path(sys.argv[1]).read_bytes().split(b'\0'):
    if path:
        print(repr(os.fsdecode(path)))
PY
else
    status=$?
    printf 'Picker exited with status %s\n' "$status" >&2
fi
rm -f -- "$selection_file"
```

This example prints escaped paths; replace that consumer with the work you need.
See [picker interaction](file-picker.md#try-without-changing-desktop-defaults) for
accepting folders, filters, keyboard focus and cancellation.

## Portal mode

`rummage --portal` runs the D-Bus FileChooser backend. Normally the session bus
activates it using the installed service file. Configure backend selection using
the [portal guide](file-picker.md), rather than adding it as an ordinary desktop
autostart command. `--picker-channel` is an internal child-process protocol, not
a public scripting interface. SaveFiles is available through the portal, not as
a standalone multi-save CLI mode.

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | Successful command/normal browser exit; picker accepted |
| `1` | Config validation problems or picker cancellation; runtime failures may also be nonzero |
| `2` | Invalid command-line arguments or malformed internal picker request |

Treat nonzero results as failure to obtain a selection. Check stderr for a
diagnostic rather than assuming every nonzero exit is a user cancellation.

## Environment and signals

| Variable | Use |
| --- | --- |
| `RUMMAGE_ENGINE_PATH` | Colon-separated absolute engine-manifest directories for optional helper installations; restart to discover |
| `RUMMAGE_LOG` | `trace`, `debug`, `info`, `warn`, `error`, `off`; logs go to stderr |
| `RUMMAGE_FORCE_CSD=1` | Request rummage's own title bar; compositor has final say |
| `RUMMAGE_FORCE_XCURSOR=1` | Exercise the Xcursor fallback |
| `XCURSOR_THEME`, `XCURSOR_SIZE` | Cursor fallback theme and size |
| `TERMINAL` | Preferred terminal for terminal actions/application launches |
| `XDG_CONFIG_HOME`, `XDG_STATE_HOME`, `XDG_CACHE_HOME`, `XDG_DATA_HOME` | Standard storage locations |
| `XDG_DATA_DIRS`, `XDG_CURRENT_DESKTOP` | Desktop data/association discovery |
| `WAYLAND_DISPLAY`, `XDG_RUNTIME_DIR` | Wayland session connection |

Release builds log at `info` by default; debug builds at `debug`. SIGUSR1 asks the
browser for a memory report; SIGINT/SIGTERM request orderly shutdown. Startup
benchmark variables and `RUMMAGE_SANDBOX_REPORT` are development instrumentation,
described in [development](development.md) and [security](security.md).
