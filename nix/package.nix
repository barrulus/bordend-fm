{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  makeWrapper,
  wayland,
  wayland-scanner,
  wayland-protocols,
  libglvnd,
  libxkbcommon,
  harfbuzz,
  freetype,
  fontconfig,
  tomlplusplus,
  libwebp,
  libjxl,
  zlib,
  sdbus-cpp_2,
  shared-mime-info,
  hicolor-icon-theme,
  doctest,
  dbus,
  dejavu_fonts,
  makeFontsConf,
  mesa,
  _7zz,
  unzip,
  writeShellScriptBin,
  withTests ? false,
}:

stdenv.mkDerivation {
  pname = "nordend";
  version = "0.5.0";

  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions (
      [
        ../CMakeLists.txt
        ../LICENSE
        ../cmake
        ../data
        ../docs/config.example.toml
        ../src
        # The preview helper embeds these images to warm up codecs before sandboxing.
        ../tests/data/image
        # The development SVG renderer shares its comparison helper with the tests.
        ../tests/svg_compare.hpp
        ../third_party
      ]
      ++ lib.optional withTests ../tests
    );
  };

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    makeWrapper
    wayland-scanner
  ]
  ++ lib.optionals withTests [
    doctest
    (writeShellScriptBin "dbus-run-session" ''
      exec ${dbus}/bin/dbus-run-session --config-file=${dbus}/share/dbus-1/session.conf "$@"
    '')
  ];

  buildInputs = [
    wayland
    wayland-protocols
    libglvnd
    libxkbcommon
    harfbuzz
    freetype
    fontconfig
    tomlplusplus
    libwebp
    libjxl
    zlib
    sdbus-cpp_2
  ];

  cmakeFlags = [
    "-DNORDEND_WERROR=OFF"
    "-DNORDEND_BUILD_TESTS=${if withTests then "ON" else "OFF"}"
  ];

  doCheck = withTests;
  nativeCheckInputs = lib.optionals withTests [
    _7zz
    unzip
    (writeShellScriptBin "7z" ''exec ${_7zz}/bin/7zz "$@"'')
  ];
  # These two fixtures create executable scripts at runtime, after patchShebangs runs.
  postPatch = lib.optionalString withTests ''
    substituteInPlace tests/about_test.cpp tests/platform_test.cpp \
      --replace-fail '#!/bin/sh' '#!${stdenv.shell}'
  '';
  checkPhase = ''
    runHook preCheck
    export FONTCONFIG_FILE=${
      makeFontsConf {
        fontDirectories = [ dejavu_fonts ];
        includes = [ "${fontconfig.out}/share/fontconfig/conf.avail/60-latin.conf" ];
      }
    }
    export XDG_DATA_DIRS=${shared-mime-info}/share:${hicolor-icon-theme}/share
    export LIBGL_ALWAYS_SOFTWARE=1
    export EGL_PLATFORM=surfaceless
    export __EGL_VENDOR_LIBRARY_FILENAMES=${mesa}/share/glvnd/egl_vendor.d/50_mesa.json
    export LIBGL_DRIVERS_PATH=${mesa}/lib/dri
    ctest --show-only | grep -F file-picker-portal
    ctest --show-only | grep -F action-notifications
    ctest --output-on-failure --timeout 180
    runHook postCheck
  '';

  postInstall = ''
    install -Dm644 "$src/LICENSE" "$out/share/licenses/nordend/LICENSE"
    wrapProgram "$out/bin/nordend" \
      --set-default XDG_DATA_DIRS /usr/local/share:/usr/share \
      --suffix XDG_DATA_DIRS : "${
        lib.makeSearchPath "share" [
          shared-mime-info
          hicolor-icon-theme
        ]
      }"
  '';

  meta = {
    description = "Native file manager for Wayland";
    homepage = "https://github.com/pankaspe/nordend-fm";
    license = lib.licenses.mit;
    mainProgram = "nordend";
    platforms = [
      "x86_64-linux"
      "aarch64-linux"
    ];
  };
}
