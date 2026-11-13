# Nordend's Nix package, in nixpkgs' style. The flake at the top of the repository builds it
# from the tree it sits in: nix build, or nix run github:pankaspe/nordend-fm
{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  makeWrapper,
  wayland,
  wayland-protocols,
  wayland-scanner,
  libGL,
  libxkbcommon,
  harfbuzz,
  freetype,
  fontconfig,
  tomlplusplus,
  libwebp,
  libjxl,
  lcms2,
  zlib,
  sdbus-cpp_2,
  doctest,
  shared-mime-info,
  hicolor-icon-theme,
  src,
  version,
}:

stdenv.mkDerivation {
  pname = "nordend";
  inherit version src;

  strictDeps = true;

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    wayland-scanner
    makeWrapper
  ];

  buildInputs = [
    wayland
    wayland-protocols
    # src/ui reads the scanner's path from its pkg-config file
    wayland-scanner
    libGL
    libxkbcommon
    harfbuzz
    freetype
    fontconfig
    tomlplusplus
    libwebp
    libjxl
    # libjxl_cms.pc requires it
    lcms2
    zlib
    sdbus-cpp_2
  ];

  # The tests read MIME types from shared-mime-info's database at build time.
  checkInputs = [
    doctest
    shared-mime-info
  ];

  cmakeFlags = [
    # a newer compiler's new warning must not break the package
    (lib.cmakeBool "NORDEND_WERROR" false)
    (lib.cmakeBool "NORDEND_BUILD_TESTS" true)
  ];

  doCheck = true;

  # MIME types and the icon theme's fallback, wherever the system keeps its own.
  postFixup = ''
    wrapProgram $out/bin/nordend \
      --suffix XDG_DATA_DIRS : ${shared-mime-info}/share:${hicolor-icon-theme}/share
  '';

  meta = {
    description = "Fast, native file manager for Wayland, made for photographers";
    homepage = "https://github.com/pankaspe/nordend-fm";
    license = lib.licenses.mit;
    # the only one built and tried so far
    platforms = [ "x86_64-linux" ];
    mainProgram = "nordend";
  };
}
