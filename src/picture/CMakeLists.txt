# What Nordend does with a picture, wherever it comes from: the Image type, turning and scaling,
# the thumbnail cache's PNG (written with zlib, read with wuffs), and how a picture is asked for
# (LoadRequest, Loaded, the loader). No parser of outside files: those are in formats/, run by
# the isolated helper.

# wuffs (third_party/wuffs): PNG, JPEG and GIF, memory-safe by construction. Compiled on its own,
# without our warnings.
add_library(nordend_wuffs STATIC src/wuffs_impl.cpp)
target_include_directories(nordend_wuffs SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/wuffs)
target_compile_options(nordend_wuffs PRIVATE -w)
if(NORDEND_SANITIZER)
    target_compile_options(nordend_wuffs PRIVATE -fsanitize=${NORDEND_SANITIZER})
endif()

add_library(nordend_picture
    src/load.cpp
    src/orientation.cpp
    src/png.cpp
    src/scale.cpp
    src/wuffs.cpp)
add_library(nordend::picture ALIAS nordend_picture)
target_include_directories(nordend_picture PUBLIC include)
target_link_libraries(nordend_picture PRIVATE nordend_wuffs PkgConfig::ZLIB nordend::warnings)
