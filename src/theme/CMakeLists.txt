add_library(nordend_theme
    src/app_icon.cpp
    src/desktop.cpp
    src/oklch.cpp
    src/palette.cpp
    src/theme.cpp)
add_library(nordend::theme ALIAS nordend_theme)

target_include_directories(nordend_theme PUBLIC include)
target_link_libraries(nordend_theme
    PUBLIC nordend::core nordend::ui
    PRIVATE PkgConfig::TOMLPLUSPLUS nordend::warnings)
