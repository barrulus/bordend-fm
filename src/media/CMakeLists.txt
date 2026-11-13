# The only target that links FFmpeg. The application works without this optional package.
add_executable(nordend-media backend.cpp
    ../isolate/src/helper_main.cpp ../isolate/src/protocol.cpp ../isolate/src/sandbox.cpp)
target_include_directories(nordend-media PRIVATE ../isolate/src)
target_link_libraries(nordend-media PRIVATE nordend::picture nordend::text
    PkgConfig::AVFORMAT PkgConfig::AVCODEC PkgConfig::AVUTIL PkgConfig::SWSCALE nordend::warnings)
set_target_properties(nordend-media PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
install(TARGETS nordend-media RUNTIME DESTINATION libexec/nordend COMPONENT media)
configure_file("${PROJECT_SOURCE_DIR}/data/engines/media.toml"
    "${PROJECT_BINARY_DIR}/engines/media.toml" COPYONLY)
install(FILES "${PROJECT_SOURCE_DIR}/data/engines/media.toml"
    DESTINATION share/nordend/engines COMPONENT media)
