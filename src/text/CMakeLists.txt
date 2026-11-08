# What Nordend does with a text file (ROADMAP 3b): the Text type and how one is asked for. No
# reader of files: those are in formats/text, run by the base engine.
add_library(nordend_text src/text.cpp)
add_library(nordend::text ALIAS nordend_text)
target_include_directories(nordend_text PUBLIC include)
target_link_libraries(nordend_text PRIVATE nordend::warnings)
