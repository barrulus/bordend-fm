add_library(nordend_preview
    src/info.cpp
    src/providers.cpp
    src/summary.cpp)
add_library(nordend::preview ALIAS nordend_preview)

target_include_directories(nordend_preview PUBLIC include)
target_link_libraries(nordend_preview
    PUBLIC nordend::core nordend::fs nordend::picture nordend::text nordend::engines
    PRIVATE nordend::thumbs nordend::warnings)
