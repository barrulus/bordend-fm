# The image family (ROADMAP 2): photos and images. Decoders (wuffs through picture, libwebp,
# libjxl, our PGM/PPM and TIFF previews), camera raw previews and EXIF (our own readers; exiv2
# still linked until ROADMAP 2e-5 takes it out). Linked into the isolated helper (and tests and
# development tools), never into Nordend.
add_library(nordend_formats_image
    src/camera/info.cpp
    src/camera/previews.cpp
    src/decode.cpp
    src/dng/colour.cpp
    src/dng/develop.cpp
    src/dng/ljpeg.cpp
    src/dng/opcodes.cpp
    src/dng/raw.cpp
    src/fd_file.cpp
    src/jpeg_segments.cpp
    src/jxl_decoder.cpp
    src/load.cpp
    src/photo.cpp
    src/pnm.cpp
    src/preview.cpp
    src/tiff/codecs.cpp
    src/tiff/container.cpp
    src/tiff/decode.cpp
    src/tiff/extract.cpp
    src/webp_decoder.cpp
    src/wuffs_decoder.cpp)
add_library(nordend::formats::image ALIAS nordend_formats_image)

target_include_directories(nordend_formats_image PUBLIC include)
target_link_libraries(nordend_formats_image
    PUBLIC nordend::picture
    PRIVATE PkgConfig::WEBP PkgConfig::JXL PkgConfig::EXIV2 PkgConfig::ZLIB nordend::warnings)
