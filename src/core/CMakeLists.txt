add_library(nordend_core
    src/event_loop.cpp
    src/log.cpp
    src/worker_pool.cpp)
add_library(nordend::core ALIAS nordend_core)

target_include_directories(nordend_core PUBLIC include)
target_link_libraries(nordend_core PRIVATE nordend::warnings)
