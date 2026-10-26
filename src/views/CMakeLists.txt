add_library(nordend_views
    src/format.cpp
    src/list_view.cpp)
add_library(nordend::views ALIAS nordend_views)

target_include_directories(nordend_views PUBLIC include)
target_link_libraries(nordend_views
    PUBLIC nordend::core nordend::ui nordend::fs nordend::theme
    PRIVATE nordend::warnings)
