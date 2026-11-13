add_library(rummage_theme
    src/app_icon.cpp
    src/desktop.cpp
    src/oklch.cpp
    src/palette.cpp
    src/syntax.cpp
    src/theme.cpp)
add_library(rummage::theme ALIAS rummage_theme)

target_include_directories(rummage_theme PUBLIC include)
target_link_libraries(rummage_theme
    PUBLIC rummage::core rummage::ui
    PRIVATE PkgConfig::TOMLPLUSPLUS rummage::warnings)
