# Installation

[Documentation](README.md) · [First steps](usage.md) · [Troubleshooting](troubleshooting.md)

Use a development download for your distribution, Nix for a dependency-managed
build, or CMake for a native source build. Fork packages use this repository's
source, not an upstream Nordend release tarball.

## Runtime requirements

rummage needs Linux, a Wayland compositor with xdg-shell, and an EGL/OpenGL ES 3.0
driver. Its preview sandbox supports x86-64 and AArch64. X11-only sessions are not
supported. Install fonts and the shared-mime-info database for text and file types.

A session D-Bus connection provides desktop integration. Optional services and
programs enable particular features:

| Dependency | Used for |
| --- | --- |
| udisks2 and the desktop's authorization setup | Mounting, unmounting and ejecting devices |
| xdg-desktop-portal and a suitable settings backend | Desktop light/dark and accent settings |
| xdg-desktop-portal with rummage selected for FileChooser | Application Open/Save dialogs |
| A desktop notification service | Custom-action completion popups |
| A terminal emulator or `xdg-terminal-exec` | F4 and terminal desktop applications |
| unrar, 7zz/7z, File Roller, unzip, gzip or tar | Archive extraction, depending on format |
| Programs named in your custom actions | Those actions only |

FFmpeg libraries are optional and used only by the separate media helper. The
core application does not require FFmpeg. See
[preview support](previews.md) and [desktop compatibility](compatibility.md).

## Upgrading from Nordend

The command, desktop entry, portal backend, packages, Nix module, and environment variables
now use `rummage` / `RUMMAGE` (including `rummage-media` and `programs.rummage`).
Update launcher commands, `xdg-mime` defaults, compositor rules, portal preferences, and
Noctalia templates accordingly; the old executable and portal names are not installed as aliases.

On the first normal launch without `--config`, rummage copies existing Nordend `config.toml`,
`colors.toml`, and session `state.toml` into the corresponding `rummage` XDG directories,
only where the destination is absent. Originals and existing rummage files are left intact.
Custom commands and paths inside copied settings are preserved; update any that invoke Nordend.
Explicit `--config`, config checks, and picker/portal-only launches do not migrate files.
Screenshots and historical development records may still show the upstream name.

## Development downloads

The [latest development build](https://github.com/barrulus/rummage/releases/tag/development)
is refreshed automatically after **all checks pass on `main`**. These are unsigned Linux
x86_64 development snapshots. Choose your exact distribution; the Fedora RPM is **not
validated for Leap or RHEL**, and the DEB is not validated for Ubuntu or older Debian.

| System | Download | Install after downloading |
|---|---|---|
| Debian 13 (trixie), amd64 | [DEB](https://github.com/barrulus/rummage/releases/download/development/rummage-debian13-amd64.deb) | `sudo apt install ./rummage-debian13-amd64.deb` |
| Fedora 44, x86_64 | [RPM](https://github.com/barrulus/rummage/releases/download/development/rummage-fedora44-x86_64.rpm) | `sudo dnf install ./rummage-fedora44-x86_64.rpm` |
| Arch Linux, x86_64 | [Arch package](https://github.com/barrulus/rummage/releases/download/development/rummage-arch-x86_64.pkg.tar.zst) | `sudo pacman -Syu`, then `sudo pacman -U ./rummage-arch-x86_64.pkg.tar.zst` |
| openSUSE Tumbleweed, x86_64 | [Tumbleweed RPM](https://github.com/barrulus/rummage/releases/download/development/rummage-tumbleweed-x86_64.rpm) | `sudo zypper dup`, then `sudo zypper install --allow-unsigned-rpm ./rummage-tumbleweed-x86_64.rpm` |
| Void glibc, x86_64 | [XBPS bundle](https://github.com/barrulus/rummage/releases/download/development/rummage-void-glibc-x86_64.tar.gz) | See the two commands below. |

For Void, extract the bundle and keep its package and repository index together:

```sh
tar -xzf rummage-void-glibc-x86_64.tar.gz
sudo xbps-install -R "$PWD/rummage-void-x86_64/packages" rummage
```

For Nix, with flakes enabled, run or install the same tested source revision through the
moving `development` tag (this builds rummage if no cached binary is available):

```sh
nix run github:barrulus/rummage/development
nix profile install github:barrulus/rummage/development
```

[Checksums](https://github.com/barrulus/rummage/releases/download/development/SHA256SUMS)
and [source identity](https://github.com/barrulus/rummage/releases/download/development/source.json)
are provided with every build. To verify a download, save `SHA256SUMS` beside it and run
`sha256sum --check --ignore-missing SHA256SUMS`. Downloads are replaced as builds pass;
use the full commit from `source.json` instead of `development` in the Nix reference to
pin a version. These downloads are individual packages, not an automatic-update repository.

For a **Debian source package**, download the
[Debian source bundle](https://github.com/barrulus/rummage/releases/download/development/rummage-debian-source.tar.gz).
It contains the matching `.dsc`, `.orig.tar.gz`, `.debian.tar.xz`, source `.changes` and
`.buildinfo`, with their versioned filenames preserved. CI builds the published DEB from
this source package. Rebuild on Debian 13:

```sh
sudo apt update
sudo apt install build-essential
tar -xzf rummage-debian-source.tar.gz
cd rummage-debian-source
dpkg-source -x rummage_*.dsc source
sudo apt build-dep ./source
cd source
dpkg-buildpackage -b -us -uc
```

These source packages are unsigned development snapshots. The
[generic source archive](https://github.com/barrulus/rummage/releases/download/development/rummage-source.tar.gz)
is also available; it is distinct from the Debian source package.

## Optional video previews

Install the matching `rummage-media` package for video thumbnails and still previews in the
information pane and Space Quick Look. The grid draws cinema filmstrip borders; cached images
and larger previews stay undecorated. This is still-frame preview, not video playback.

The helper uses system FFmpeg 6.1 or newer shared libraries (`libavformat`, `libavcodec`,
`libavutil`, `libswscale`); it does not execute `ffmpeg`, `ffprobe`, or `ffmpegthumbnailer`.
Your package manager reuses compatible installed libraries. Some distributions package these
libraries together with the FFmpeg commands. Codec availability follows the distribution build.

The main package has no FFmpeg dependency. Without the media helper, videos retain their file
icons and information cards; existing valid cached thumbnails remain usable. Enter opens the
selected video in its default application. Missing helpers/libraries are retried after restarting
rummage and do not create permanent failed-thumbnail records.

For a source build, add `-DRUMMAGE_MEDIA=ON` with the four libraries' development packages
installed. It defaults to OFF. `cmake --install build --component media` installs only the helper
and its manifest; `--component Unspecified` installs the core application. The helper uses the
existing seccomp/Landlock sandbox, reads only the supplied descriptor, and has bounded decoding,
output dimensions, concurrency and time. Thumbnail selection tries beyond dark opening frames.

For Nix, `nix build .#rummage-with-media` builds the application configured with the optional
helper; `.#rummage-media` selects its separate helper output. The normal default remains free of
FFmpeg. Separate custom prefixes can be registered through `RUMMAGE_ENGINE_PATH`, a colon-separated
list of absolute `share/rummage/engines` directories. Restart after changing the installed engines.

## Nix

With flakes enabled, from this checkout:

```sh
nix build
nix run . -- ~/Pictures
nix flake check
```

`nix build` produces `result/bin/rummage` plus the preview helper, manifests and
desktop integration files. The ordinary package omits tests.
`nix flake check` builds a separate test derivation (including media), runs installed
package smoke checks and checks the NixOS module. Packages and development shells are
defined for `x86_64-linux` and `aarch64-linux`; checking one host is not evidence of
runtime testing on both architectures.

On non-NixOS hosts, running a Nix-built GUI also requires access to the host's
graphics drivers. A successful package build alone does not establish that setup.

### NixOS configuration

Add an input to your existing system flake. The fork remote configured for this
checkout is `barrulus/rummage`:

```nix
inputs.rummage.url = "github:barrulus/rummage";
```

For unpublished local work, use `path:/absolute/path/to/rummage-fm` instead. A remote
input only contains work pushed to that remote; it cannot include local edits.

Include `rummage` in your existing output arguments and import its module:

```nix
outputs = { nixpkgs, rummage, ... }: {
  nixosConfigurations.my-host = nixpkgs.lib.nixosSystem {
    system = "x86_64-linux";
    modules = [
      ./configuration.nix
      rummage.nixosModules.default
      { programs.rummage.enable = true; }
    ];
  };
};
```

Keep your existing inputs, outputs and modules; replace `my-host` with the actual
host name. Apply from your configuration directory:

```sh
sudo nixos-rebuild switch --flake .#my-host
```

`programs.rummage.package` can select another package. Enabling the application
alone does not change portal preferences. For Open/Save dialogs also enable
`programs.rummage.filePicker.enable`; see the [picker guide](file-picker.md) for
desktop selection and preserving other portal backends.

### Home Manager

The flake exports a NixOS module, not a Home Manager module. To install the package
through Home Manager, pass `inputs` using `extraSpecialArgs` in your existing
`homeManagerConfiguration`, or `home-manager.extraSpecialArgs` in a NixOS setup:

```nix
extraSpecialArgs = { inherit inputs; };
```

Then add to your home configuration:

```nix
{ inputs, pkgs, ... }: {
  home.packages = [ inputs.rummage.packages.${pkgs.stdenv.hostPlatform.system}.default ];
}
```

Apply with `home-manager switch --flake .#your-user`, or rebuild NixOS when it
manages Home Manager. Installing the package does not select it as a portal backend.

## Build dependencies

[CMakeLists.txt](../CMakeLists.txt) is the authoritative list for this checkout.
Use a C++20 compiler and standard library with `std::format`, CMake 3.20 or newer,
Ninja (for the examples), and pkg-config. Required library/module names are:

| Area | CMake/pkg-config requirements |
| --- | --- |
| Display | `wayland-client`, `wayland-egl`, `wayland-cursor`, `egl`, `glesv2`, `xkbcommon` |
| Protocol generation | `wayland-scanner`, `wayland-protocols` including xdg-dialog-v1 and stable tablet-v2 XML |
| Text | `harfbuzz`, `freetype2`, `fontconfig` |
| Configuration | `tomlplusplus` |
| Images | `libwebp`, `libwebpdemux`, `libjxl`, `libheif >= 1.23`, `lcms2`, `zlib` |
| Desktop services | `sdbus-c++` |
| Optional media helper | FFmpeg 6.1+ development libraries: `libavformat`, `libavcodec`, `libavutil`, `libswscale`; enable `RUMMAGE_MEDIA` |
| Tests | `doctest`; `dbus-run-session` for portal/notification integration tests |

HEIC decoding requires libheif's **libde265** decoder at runtime (built in or
installed as a codec plugin) and Little CMS 2. The Nix development shell/package
supplies them. The preview helper initializes codecs and decodes a trusted HEIC
fixture before entering its sandbox; later decoding adds no sandbox permissions.

Optional reference dependencies are libjpeg-turbo for the JPEG benchmark, LibRaw
for DNG comparison tests, and exiv2 for metadata comparison tests/benchmarks.
These reference libraries are not required to build the application.

Distribution package names vary. The [packaging recipes](ci-packaging.md) contain
platform-specific dependencies. Use the Nix development shell when maintaining
local build dependencies is inconvenient.

## Build from source

Run from the checkout you intend to build:

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
ctest --test-dir build --output-on-failure
./build/rummage ~/Pictures
cmake --install build --strip
```

Install doctest before configuration if you want tests: CMake skips them with a
notice when it is missing. See [development](development.md) for test environment
requirements and build options. Keep the helper and engine manifests alongside the
application in the build tree or installation; copying only `rummage` loses previews.

The install prefix contains:

| Path | Purpose |
| --- | --- |
| `bin/rummage` | File manager and portal backend |
| `libexec/rummage/rummage-preview` | Isolated image/text reader |
| `share/rummage/engines/base.toml` | Engine registration |
| `share/applications/rummage.desktop` | Launcher and folder association |
| `share/icons/hicolor/scalable/apps/rummage.svg` | Application icon |
| `share/rummage/noctalia/rummage` | Noctalia colour template |
| `share/rummage/config.example.toml` | Complete annotated settings reference |
| `share/xdg-desktop-portal/portals/rummage.portal` | Portal registration |
| `share/dbus-1/services/org.freedesktop.impl.portal.desktop.rummage.service` | On-demand portal activation |

The annotated configuration is installed at `share/rummage/config.example.toml`.
A first normal launch creates a personal config only if absent. Existing personal
files are never replaced by updates. Ensure the prefix's `bin` is on `PATH` and
its desktop data is discoverable. The portal service uses an absolute executable
path generated for the install prefix.

## Build distribution packages

Use the [package builder guide](ci-packaging.md) for Void glibc, Debian 13, Fedora
44, Arch Linux and openSUSE Tumbleweed. Those builders archive a clean committed
checkout, record its identity and validate installation in disposable containers.
They reject uncommitted changes; their output directory must be outside the checkout.
The Void template is populated by the builder with an archive and checksum rather
than downloading an upstream release. Ordinary CMake builds need no container.

## Desktop setup and updates

To make rummage the default application for folders:

```sh
xdg-mime default rummage.desktop inode/directory
```

Assign `rummage` to a compositor shortcut if desired. For Umbriel's `config.toml`:

```toml
"Mod+E" = { action = "spawn rummage", repeat = false }
```

[Noctalia colours](appearance.md#noctalia-colours) and
[file-picker integration](file-picker.md) have separate setup steps. Setting the
folder association does not change application file dialogs.

To update a source install, rebuild and reinstall from the intended revision using
the same prefix. For a flake input, update its lock entry in your configuration and
rebuild. Personal `config.toml` files are never rewritten to add new options; compare
with the current reference. Record `rummage --about` when diagnosing differences
between installed and development builds.
