# The text family (ROADMAP 3b): text files, their encoding found from the bytes. Linked into the
# isolated helper (and tests and development tools), never into Nordend.
add_library(nordend_formats_text src/read.cpp)
add_library(nordend::formats::text ALIAS nordend_formats_text)
target_include_directories(nordend_formats_text PUBLIC include)
target_link_libraries(nordend_formats_text PUBLIC nordend::text PRIVATE nordend::warnings)
