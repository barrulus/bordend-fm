add_library(nordend_views
    src/browser.cpp
    src/column.cpp
    src/column_view.cpp
    src/draw_helpers.cpp
    src/folder.cpp
    src/format.cpp
    src/grid_view.cpp
    src/quick_look.cpp
    src/thumbnails.cpp)
add_library(nordend::views ALIAS nordend_views)

target_include_directories(nordend_views PUBLIC include)
target_link_libraries(nordend_views
    PUBLIC nordend::core nordend::ui nordend::fs nordend::theme nordend::thumbs nordend::preview
    PRIVATE nordend::warnings)
