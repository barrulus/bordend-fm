add_library(nordend_thumbs
    src/cache.cpp
    src/md5.cpp)
add_library(nordend::thumbs ALIAS nordend_thumbs)

target_include_directories(nordend_thumbs PUBLIC include)
target_link_libraries(nordend_thumbs PUBLIC nordend::image PRIVATE nordend::warnings)
