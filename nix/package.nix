{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  makeWrapper,
  makeFontsConf,
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
  doctest,
  mesa,
  dejavu_fonts,
  shared-mime-info,
  hicolor-icon-theme,
}:

stdenv.mkDerivation {
  pname = "nordend";
  version = "0.5.0";

  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../CMakeLists.txt
      ../LICENSE
      ../cmake
      ../data
      ../docs/config.example.toml
      ../src
      ../tests
      ../third_party
    ];
  };

  # Adapt host-dependent test fixtures in the build copy only.
  postPatch = ''
    # shared-mime-info also associates .md with ROM images; .markdown is unambiguous.
    substituteInPlace tests/column_view_test.cpp tests/details_view_test.cpp \
      --replace-fail '.md' '.markdown'
    # /etc/hostname is absent in the Nix sandbox; passwd exists and must be denied.
    substituteInPlace tests/sandbox_probe.cpp \
      --replace-fail '/etc/hostname' '/etc/passwd'
    # ext4 cannot represent the original 1 PiB sparse file. Exceed available space
    # by 1 GiB instead, retaining the check that a copy is refused before it starts.
    substituteInPlace tests/transfer_test.cpp \
      --replace-fail 'std::uintmax_t{1} << 50U' '(fs::space(t.to).available + (std::uintmax_t{1} << 30U))' \
      --replace-fail 'Not enough space: 1125.9 TB needed, ' 'Not enough space: '
  '';

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    makeWrapper
    wayland-scanner
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
    "-DNORDEND_BUILD_TESTS=ON"
  ];

  doCheck = true;
  checkInputs = [ doctest ];
  preCheck = ''
    export HOME="$TMPDIR/home"
    mkdir -p "$HOME"
    export XDG_DATA_DIRS="${shared-mime-info}/share"
    export FONTCONFIG_FILE="${
      makeFontsConf {
        fontDirectories = [ dejavu_fonts ];
        includes = [ "${fontconfig.out}/etc/fonts/conf.d" ];
      }
    }"
    export __EGL_VENDOR_LIBRARY_FILENAMES="${mesa}/share/glvnd/egl_vendor.d/50_mesa.json"
    export LIBGL_ALWAYS_SOFTWARE=1
  '';

  checkPhase = ''
    runHook preCheck
    # Run the allocator-sensitive test in a fresh process, before rendering tests
    # can populate malloc arenas and software-renderer caches.
    ./tests/nordend-tests --test-case="memory: large blocks go back to the system once freed"
    ./tests/nordend-tests --test-case-exclude="memory: large blocks go back to the system once freed"
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
