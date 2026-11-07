# Nordend's preview helpers (ROADMAP 2b): the client side, linked into Nordend, and the
# nordend-preview program that runs the parsers.
add_library(nordend_isolate
    src/helpers.cpp
    src/protocol.cpp)
add_library(nordend::isolate ALIAS nordend_isolate)
target_include_directories(nordend_isolate PUBLIC include PRIVATE src)
target_link_libraries(nordend_isolate PUBLIC nordend::image PRIVATE nordend::warnings)

add_executable(nordend-preview src/helper_main.cpp src/protocol.cpp)
target_include_directories(nordend-preview PRIVATE src)
target_link_libraries(nordend-preview PRIVATE nordend::image nordend::warnings)
# Next to nordend in the build tree, where default_program() looks first.
set_target_properties(nordend-preview PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
