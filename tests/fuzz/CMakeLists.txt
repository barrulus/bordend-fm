# libFuzzer targets (ROADMAP 2c, 2d), built with NORDEND_FUZZ=ON (see the top CMakeLists.txt).
#   build-fuzz/tests/fuzz/nordend-fuzz-tiff CORPUS tests/data/image/tiff -max_total_time=3600
add_executable(nordend-fuzz-tiff fuzz_tiff.cpp)
target_include_directories(nordend-fuzz-tiff
    PRIVATE ${PROJECT_SOURCE_DIR}/src/formats/image/src)
target_link_libraries(nordend-fuzz-tiff PRIVATE nordend::formats::image nordend::warnings)
target_link_options(nordend-fuzz-tiff PRIVATE -fsanitize=fuzzer)

# Camera files' previews (ROADMAP 2e): seeds are the first bytes of raw files, kept locally.
add_executable(nordend-fuzz-camera fuzz_camera.cpp)
target_link_libraries(nordend-fuzz-camera PRIVATE nordend::formats::image nordend::warnings)
target_link_options(nordend-fuzz-camera PRIVATE -fsanitize=fuzzer)

add_executable(nordend-fuzz-picture fuzz_picture.cpp)
target_link_libraries(nordend-fuzz-picture PRIVATE nordend::formats nordend::warnings)
target_link_options(nordend-fuzz-picture PRIVATE -fsanitize=fuzzer)

# DNG (ROADMAP 2d): seeds from tests/fuzz/make_dng_seeds.py and tests/data/image/dng.
foreach(target dng ljpeg)
    add_executable(nordend-fuzz-${target} fuzz_${target}.cpp)
    target_include_directories(nordend-fuzz-${target}
        PRIVATE ${PROJECT_SOURCE_DIR}/src/formats/image/src)
    target_link_libraries(nordend-fuzz-${target} PRIVATE nordend::formats::image nordend::warnings)
    target_link_options(nordend-fuzz-${target} PRIVATE -fsanitize=fuzzer)
endforeach()

# Text (ROADMAP 3b): seeds are a few text files in different encodings.
add_executable(nordend-fuzz-text fuzz_text.cpp)
target_link_libraries(nordend-fuzz-text PRIVATE nordend::formats::text nordend::warnings)
target_link_options(nordend-fuzz-text PRIVATE -fsanitize=fuzzer)

add_executable(nordend-fuzz-svg fuzz_svg.cpp)
target_link_libraries(nordend-fuzz-svg PRIVATE nordend::svg nordend::warnings)
target_link_options(nordend-fuzz-svg PRIVATE -fsanitize=fuzzer)
