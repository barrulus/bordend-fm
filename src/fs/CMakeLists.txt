add_library(nordend_fs
    src/directory.cpp
    src/directory_model.cpp
    src/directory_watcher.cpp
    src/icon.cpp
    src/mime.cpp
    src/places.cpp
    src/sort.cpp)
add_library(nordend::fs ALIAS nordend_fs)

target_include_directories(nordend_fs PUBLIC include)
target_link_libraries(nordend_fs PUBLIC nordend::core PRIVATE nordend::warnings)
