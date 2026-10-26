# Nordend

A native, Wayland-only file manager for the [Noctalia](https://github.com/noctalia-dev) ecosystem.

> **Status:** pre-alpha, design phase. Nothing to build yet.

## Goals

- **Native and fast:** C++20, Wayland + EGL/GLES, no Qt or GTK. Directory loading and thumbnailing
  never block the UI.
- **Finder-style column view** (Miller columns) and a thumbnail grid.
- **Quick Look:** press <kbd>Space</kbd> to preview a file, including the embedded JPEG of camera RAW files.
- **Noctalia integration:** follows the Noctalia color palette and reloads it live.
- **Keyboard first**, mouse always.

## Documentation

- [Design](docs/DESIGN.md)
- [Roadmap](docs/ROADMAP.md)
- [Devlog](docs/devlog.md)
- [Upstream bug log](docs/buglog.md)

## Requirements

Wayland compositor with `xdg-shell` (developed on [Umbriel](https://github.com/noctalia-dev)).
Build (Void Linux package names):

```sh
sudo xbps-install -S cmake ninja pkgconf wayland-devel wayland-protocols \
    libxkbcommon-devel libglvnd-devel MesaLib-devel
cmake -B build -G Ninja
cmake --build build
./build/nordend --version
```

## Development

Nordend is copiloted by **Claude Opus** (Anthropic), which assists with code writing, code analysis,
bug hunting, testing and documentation. Design decisions, testing on real hardware and every release
are the author's.

## License

[MIT](LICENSE)
