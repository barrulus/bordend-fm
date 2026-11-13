# Roadmap

[Documentation](README.md) · [Current features](features.md) · [Release history](releases.md)

rummage's direction is a native C++ file manager for Wayland with a small dependency
footprint, responsive browsing and isolated preview readers. Noctalia integration is
part of that direction; the application also supports other Wayland desktops.

The following are plans carried forward from the project's development notes, not
implemented features or a release schedule:

- More preview families: SVG documents, AVIF, fonts, archive contents and audio metadata.
- rummage-owned colour labels and star ratings.
- Photography tools in Quick Look: 100% zoom, histogram and comparison.
- A file-operation queue, batch renaming and further pane workflows.
- Translations and a Luau plugin system.
- Additional isolated engines for heavier formats, including PDF and video, and local AI search.

The icon-theme SVG renderer is implemented; that does not make SVG documents a
Quick Look preview family. Optional video thumbnails and Quick Look stills are implemented; playback is absent. Engine manifests exist, but a general plugin API is
not a shipped feature.

Dual panes, configurable actions, menus, file-picker integration and expandable details folders
are documented as current source behavior in the guides, rather than future work.
The [Luciole review](luciole-comparison.md) records additional ideas at a particular
revision; it is not a commitment to implement every comparison item.

The project remains at 0.x while behavior and the configuration/state formats evolve.
The recorded versioning policy uses minor versions for visible features and patch
versions for fixes; 1.0 is intended after everyday use and stabilization of those formats.
