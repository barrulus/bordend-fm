add_library(nordend_core
    src/log.cpp)
add_library(nordend::core ALIAS nordend_core)

target_include_directories(nordend_core PUBLIC include)
target_link_libraries(nordend_core PRIVATE nordend::warnings)
