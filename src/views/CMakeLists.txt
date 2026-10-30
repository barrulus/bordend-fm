add_library(nordend_views
    src/breadcrumb.cpp
    src/browser.cpp
    src/column.cpp
    src/column_view.cpp
    src/confirm_dialog.cpp
    src/conflict_dialog.cpp
    src/context_menu.cpp
    src/details_view.cpp
    src/file_clipboard.cpp
    src/draw_helpers.cpp
    src/folder.cpp
    src/format.cpp
    src/grid_view.cpp
    src/info_pane.cpp
    src/name_editor.cpp
    src/shortcuts.cpp
    src/shortcuts_overlay.cpp
    src/sidebar.cpp
    src/status_bar.cpp
    src/title_bar.cpp
    src/quick_look.cpp
    src/thumbnails.cpp
    src/undo.cpp)
add_library(nordend::views ALIAS nordend_views)

target_include_directories(nordend_views PUBLIC include)
target_link_libraries(nordend_views
    PUBLIC nordend::core nordend::ui nordend::fs nordend::theme nordend::thumbs nordend::preview
    PRIVATE nordend::warnings)
