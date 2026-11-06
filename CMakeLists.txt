cmake_minimum_required(VERSION 3.20)
project(nordend VERSION 0.0.1 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Warning flags — applied to our code only, never to system libraries.
# ---------------------------------------------------------------------------
add_library(nordend_warnings INTERFACE)
add_library(nordend::warnings ALIAS nordend_warnings)
# NORDEND_WERROR=OFF for package builds: a newer compiler's new warning must not break them.
option(NORDEND_WERROR "Turn warnings into errors" ON)
target_compile_options(nordend_warnings INTERFACE
    -Wall -Wextra -Wpedantic $<$<BOOL:${NORDEND_WERROR}>:-Werror>
    -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual
    -Wconversion -Wsign-conversion -Wdouble-promotion
    -Wnull-dereference -Wformat=2 -Wimplicit-fallthrough)

# ---------------------------------------------------------------------------
# Sanitizers for Debug builds in their own directory, e.g.
#   cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNORDEND_SANITIZER=address,undefined
# ---------------------------------------------------------------------------
set(NORDEND_SANITIZER "" CACHE STRING "Sanitizers to enable (e.g. address,undefined or thread)")
if(NORDEND_SANITIZER)
    target_compile_options(nordend_warnings INTERFACE
        -fsanitize=${NORDEND_SANITIZER} -fno-omit-frame-pointer)
    target_link_options(nordend_warnings INTERFACE -fsanitize=${NORDEND_SANITIZER})
endif()

option(NORDEND_CLANG_TIDY "Run clang-tidy while building the nordend targets" OFF)
if(NORDEND_CLANG_TIDY)
    find_program(NORDEND_CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${NORDEND_CLANG_TIDY_EXE}")
endif()

# ---------------------------------------------------------------------------
# System dependencies (pkg-config). Found here so a missing -devel package
# fails at configure time; targets link them as the code starts using them.
# wayland-scanner and wayland-protocols are looked up by src/ui.
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client wayland-egl wayland-cursor)
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)
pkg_check_modules(GLESV2 REQUIRED IMPORTED_TARGET glesv2)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
pkg_check_modules(TOMLPLUSPLUS REQUIRED IMPORTED_TARGET tomlplusplus)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp libwebpdemux)
pkg_check_modules(JXL REQUIRED IMPORTED_TARGET libjxl)
pkg_check_modules(EXIV2 REQUIRED IMPORTED_TARGET exiv2)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)
pkg_check_modules(SDBUSCPP REQUIRED IMPORTED_TARGET sdbus-c++)  # udisks2 (ROADMAP 7.3)
pkg_check_modules(TURBOJPEG IMPORTED_TARGET libturbojpeg)  # optional for now: the benchmark

# ---------------------------------------------------------------------------
# Modules
# ---------------------------------------------------------------------------
add_subdirectory(src/core)      # logging, event loop, worker pool, config
add_subdirectory(src/ui)        # Wayland connection, windows, rendering
add_subdirectory(src/theme)     # Noctalia palette, hot reload
add_subdirectory(src/fs)        # directory loading and sorting
add_subdirectory(src/image)     # image decoding (wuffs, libwebp, libjxl), previews, PNG
add_subdirectory(src/thumbs)    # freedesktop thumbnail cache
add_subdirectory(src/preview)   # Quick Look providers: images, text, information
add_subdirectory(src/platform)  # default applications (freedesktop), launching processes
add_subdirectory(src/views)     # column view and grid

# Renderer test scene (ROADMAP 1.4), shared by the app and nordend-snapshot.
add_library(nordend_test_scene STATIC src/app/test_scene.cpp)
target_include_directories(nordend_test_scene PUBLIC src/app)
target_link_libraries(nordend_test_scene PUBLIC nordend::ui nordend::theme PRIVATE nordend::warnings)

# The commit Nordend is built from, for "About" (ROADMAP 7.5d): read when CMake configures, which
# it does again after every commit (it watches .git/logs/HEAD). Empty outside a git checkout.
set(NORDEND_COMMIT "")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=7 HEAD
                    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
                    OUTPUT_VARIABLE NORDEND_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
    if(EXISTS "${PROJECT_SOURCE_DIR}/.git/logs/HEAD")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                     "${PROJECT_SOURCE_DIR}/.git/logs/HEAD")
    endif()
endif()

add_executable(nordend src/app/main.cpp)
target_compile_definitions(nordend PRIVATE NORDEND_VERSION="${PROJECT_VERSION}"
                                           NORDEND_COMMIT="${NORDEND_COMMIT}")
target_link_libraries(nordend
    PRIVATE nordend::core nordend::ui nordend::theme nordend::views nordend::platform
            nordend_test_scene
            nordend::warnings)

# Development tool: offscreen render of the test scene or a folder list to a PPM file, and the
# list scrolling benchmark. Not installed.
add_executable(nordend-snapshot src/tools/snapshot.cpp)
target_link_libraries(nordend-snapshot
    PRIVATE nordend::core nordend::ui nordend::theme nordend::views nordend::platform
            nordend_test_scene nordend::warnings)

# Development tool: directory loading benchmark (ROADMAP 3.1). Not installed.
add_executable(nordend-bench-dir src/tools/bench_dir.cpp)
target_link_libraries(nordend-bench-dir PRIVATE nordend::core nordend::fs nordend::warnings)

# Development tool: start-up benchmark, runs nordend and times each step of its start
# (ROADMAP 7.6). Not installed.
add_executable(nordend-bench-start src/tools/bench_start.cpp)
target_link_libraries(nordend-bench-start PRIVATE nordend::warnings)

# Development tool: MIME detection check against GLib (ROADMAP 3.2). Not installed.
add_executable(nordend-mime src/tools/mime.cpp)
target_link_libraries(nordend-mime PRIVATE nordend::core nordend::fs nordend::warnings)

# Development tool: live directory model, prints changes (ROADMAP 3.3). Not installed.
add_executable(nordend-watch src/tools/watch.cpp)
target_link_libraries(nordend-watch PRIVATE nordend::core nordend::fs nordend::warnings)

# Development tool: decodes images, prints sizes and timings (ROADMAP 5.1). Not installed.
add_executable(nordend-decode src/tools/decode.cpp)
target_link_libraries(nordend-decode PRIVATE nordend::core nordend::image nordend::warnings)

# Development tool: thumbnails through the freedesktop cache (ROADMAP 5.2). Not installed.
add_executable(nordend-thumb src/tools/thumb.cpp)
target_link_libraries(nordend-thumb PRIVATE nordend::thumbs nordend::warnings)

# Development tool: which application opens a file or a type, and how (ROADMAP 6.1). Not
# installed.
add_executable(nordend-open src/tools/open.cpp)
target_link_libraries(nordend-open PRIVATE nordend::core nordend::fs nordend::platform nordend::warnings)

# Development tool: the copy and move engine from the command line, timed (ROADMAP 6.3). Not
# installed.
add_executable(nordend-transfer src/tools/transfer.cpp)
target_link_libraries(nordend-transfer PRIVATE nordend::core nordend::fs nordend::warnings)

# Development tool: JPEG decoding, wuffs against TurboJPEG scaling (ROADMAP 5.2). Not installed;
# built only when libjpeg-turbo-devel is there.
if(TURBOJPEG_FOUND)
    add_executable(nordend-bench-jpeg src/tools/bench_jpeg.cpp)
    target_link_libraries(nordend-bench-jpeg
        PRIVATE nordend::image PkgConfig::TURBOJPEG nordend::warnings)
endif()

# ---------------------------------------------------------------------------
# Tests (doctest, system package). Skipped with a notice when doctest is missing.
# ---------------------------------------------------------------------------
option(NORDEND_BUILD_TESTS "Build the unit tests" ON)
if(NORDEND_BUILD_TESTS)
    find_package(doctest CONFIG QUIET)
    if(doctest_FOUND)
        enable_testing()
        add_subdirectory(tests)
    else()
        message(STATUS "doctest not found: unit tests disabled (xbps-install doctest-devel)")
    endif()
endif()

# Known third-party leak suppressions, compiled into every executable of a sanitizer build.
if(NORDEND_SANITIZER)
    foreach(executable nordend nordend-snapshot nordend-bench-dir nordend-mime nordend-watch
            nordend-decode nordend-thumb nordend-open nordend-transfer)
        target_sources(${executable} PRIVATE src/core/src/lsan_suppressions.cpp)
    endforeach()
endif()

install(TARGETS nordend RUNTIME DESTINATION bin)
# The desktop entry (app id "nordend"): menus, and the default for folders (inode/directory).
install(FILES data/nordend.desktop DESTINATION share/applications)
# Its icon, named after the app id (the desktop entry's Icon=nordend).
install(FILES data/nordend.svg DESTINATION share/icons/hicolor/scalable/apps)
