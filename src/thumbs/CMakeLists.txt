add_library(nordend_thumbs
    src/cache.cpp
    src/video.cpp
    src/md5.cpp)
add_library(nordend::thumbs ALIAS nordend_thumbs)

target_include_directories(nordend_thumbs PUBLIC include)
target_compile_definitions(nordend_thumbs PRIVATE
    NORDEND_VIDEO_THUMBNAILER="${NORDEND_VIDEO_THUMBNAILER}")
target_link_libraries(nordend_thumbs PUBLIC nordend::picture nordend::core PRIVATE nordend::warnings)
