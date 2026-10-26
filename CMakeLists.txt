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
target_compile_options(nordend_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Werror
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
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client wayland-egl)
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)
pkg_check_modules(GLESV2 REQUIRED IMPORTED_TARGET glesv2)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)

# ---------------------------------------------------------------------------
# Modules
# ---------------------------------------------------------------------------
add_subdirectory(src/core)      # logging
add_subdirectory(src/ui)        # Wayland connection, windows, rendering

add_executable(nordend src/app/main.cpp)
target_compile_definitions(nordend PRIVATE NORDEND_VERSION="${PROJECT_VERSION}")
target_link_libraries(nordend PRIVATE nordend::core nordend::ui nordend::warnings)

install(TARGETS nordend RUNTIME DESTINATION bin)
