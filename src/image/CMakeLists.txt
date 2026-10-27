# wuffs (third_party/wuffs): PNG, JPEG and GIF. Compiled on its own, without our warnings.
add_library(nordend_wuffs STATIC src/wuffs_impl.cpp)
target_include_directories(nordend_wuffs SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/wuffs)
target_compile_options(nordend_wuffs PRIVATE -w)
if(NORDEND_SANITIZER)
    target_compile_options(nordend_wuffs PRIVATE -fsanitize=${NORDEND_SANITIZER})
endif()

add_library(nordend_image
    src/decode.cpp
    src/fit.cpp
    src/jpeg_segments.cpp
    src/jxl_decoder.cpp
    src/png.cpp
    src/preview.cpp
    src/scale.cpp
    src/tiff.cpp
    src/webp_decoder.cpp
    src/wuffs_decoder.cpp)
add_library(nordend::image ALIAS nordend_image)

target_include_directories(nordend_image PUBLIC include)
target_link_libraries(nordend_image
    PRIVATE nordend_wuffs PkgConfig::WEBP PkgConfig::JXL PkgConfig::EXIV2 PkgConfig::ZLIB
            nordend::warnings)
