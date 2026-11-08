# The families of files Nordend reads (ROADMAP 2), one module each, and the registry that asks
# them in order. Linked into the isolated helper (nordend-preview), never into Nordend.
add_subdirectory(image)  # photos and images
add_subdirectory(text)   # text files (3b)

add_library(nordend_formats src/registry.cpp)
add_library(nordend::formats ALIAS nordend_formats)
target_include_directories(nordend_formats PUBLIC include)
target_link_libraries(nordend_formats
    PUBLIC nordend::picture
    PRIVATE nordend::formats::image nordend::warnings)
