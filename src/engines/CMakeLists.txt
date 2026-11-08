# Nordend's engines (ROADMAP 3a): their manifests and the registry of who shows which type. No
# file parser here: the engines are separate programs.
add_library(nordend_engines
    src/manifest.cpp
    src/registry.cpp)
add_library(nordend::engines ALIAS nordend_engines)

target_include_directories(nordend_engines PUBLIC include)
target_link_libraries(nordend_engines
    PUBLIC nordend::fs
    PRIVATE nordend::core PkgConfig::TOMLPLUSPLUS nordend::warnings)

# The manifests next to nordend in the build tree, where default_directory() looks first.
set(NORDEND_ENGINE_MANIFESTS base.toml)
foreach(name IN LISTS NORDEND_ENGINE_MANIFESTS)
    configure_file("${PROJECT_SOURCE_DIR}/data/engines/${name}"
                   "${PROJECT_BINARY_DIR}/engines/${name}" COPYONLY)
    install(FILES "${PROJECT_SOURCE_DIR}/data/engines/${name}" DESTINATION share/nordend/engines)
endforeach()
