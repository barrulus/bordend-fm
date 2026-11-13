# Security model

[Documentation](README.md) · [Architecture](architecture.md) · [File operations](file-operations.md)

rummage separates image/text file parsing from the browser process and uses bounded
protocols and resource limits. This page describes those boundaries, not a claim
that the entire file manager or every external program is sandboxed.

## Image and text helpers

`rummage-preview` receives already-open input descriptors from rummage and returns
picture/text results. Libraries are warmed up before entering the sandbox so later
decoding does not need to discover configuration or load codec resources from disk.

In normal builds the helper applies:

- A seccomp syscall allowlist. File opens fail with `EACCES`; unexpected calls
  normally kill the helper. The helper exits if seccomp setup fails.
- Landlock restrictions when supported by the running kernel. Filesystem access
  is denied; network and cross-process restrictions depend on the available ABI.
  Landlock is additional protection, not a requirement for the helper to start.
- Resource limits: no core dumps, 32 file descriptors and 4 GiB of virtual address
  space in ordinary builds. Decoder and protocol limits also bound requests/results.
- Parent-side timeout and failure handling. The default helper request timeout is
  3 seconds (8 seconds for media), and idle helpers close after 30 seconds; allocations can be returned
  earlier during idle time.

The filter has architecture definitions for x86-64 and AArch64. Crashes, timeouts
and malformed replies are treated as preview failures. The browser does not fall
back to decoding the same unsupported input in-process.

See [sandbox.cpp](../src/isolate/src/sandbox.cpp),
[helper_main.cpp](../src/isolate/src/helper_main.cpp) and
[helpers.cpp](../src/isolate/src/helpers.cpp). Tests include forbidden-operation
probes and helpers that deliberately crash, hang or send invalid results.

## Boundaries outside the helper

| Component | Boundary |
| --- | --- |
| Theme SVG/PNG icons | Loaded/rendered in-process; SVG parsing/rendering has explicit work and size limits |
| Thumbnail cache | Shared desktop files read by the application; not a trusted private store |
| Optional video previews | `rummage-media` links FFmpeg libraries inside the same descriptor-based sandbox, with separate decode/time limits |
| Desktop applications and custom actions | Ordinary user processes, outside the preview sandbox |
| Archive extraction | External archive tools; their parser and extraction behavior remains their responsibility |
| File operations, MIME lookup and desktop metadata | Run with rummage's user permissions |

Do not infer the helper's protections for any other row. The
[SVG validation document](svg-renderer-validation.md) records its resource limits
and required adversarial testing separately.

## File-operation guarantees and limits

Built-in copies stage output before publishing it and cross-filesystem moves flush
the copy before removing the source. Conflicts ask for a policy, trash failures do
not become silent permanent deletes, and permanent deletion asks for confirmation.
These measures do not make multi-file operations transactional or replace backups.
Completed steps can remain after cancellation, and overwritten or permanently
deleted content is not generally recoverable through Undo.

Custom actions use an argument vector, preserving path boundaries without implicit
shell evaluation, globbing or environment expansion. A user-configured shell or
executable still has the user's normal authority. The destination picker confirms
an existing save path but the external program controls actual writes and conflicts.
External changes are outside rummage's undo history.

Archive CLI workflows use new destination names and preserve sources, but are not
the image/text decoder sandbox. Consult the external tool's behavior for untrusted
archives. This documentation does not claim a universal archive containment guarantee.

## Development diagnostics

Sanitizer builds relax address-space limits and add syscall allowances for runtime
instrumentation. They are useful for finding errors but are not equivalent to the
production sandbox. `RUMMAGE_SANDBOX_REPORT=1` logs unexpected syscalls and returns
`ENOSYS` instead of using the normal kill path; use it for diagnosis, not as a
normal runtime setting.

When investigating a failure, retain the input, revision, build mode, stderr and
reproduction command. Avoid putting private file contents into a public report.
For review and test workflows, see [development](development.md).
