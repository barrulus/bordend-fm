add_executable(nordend-tests
    main.cpp
    config_test.cpp
    directory_model_test.cpp
    directory_test.cpp
    event_loop_test.cpp
    file_watcher_test.cpp
    icon_test.cpp
    mime_test.cpp
    palette_test.cpp
    sort_test.cpp
    worker_pool_test.cpp)
target_link_libraries(nordend-tests PRIVATE nordend::core nordend::theme nordend::fs doctest::doctest nordend::warnings)
# The shipped example config must parse clean and match the built-in defaults.
target_compile_definitions(nordend-tests
    PRIVATE NORDEND_EXAMPLE_CONFIG="${PROJECT_SOURCE_DIR}/docs/config.example.toml")
if(NORDEND_SANITIZER)
    target_sources(nordend-tests PRIVATE ${PROJECT_SOURCE_DIR}/src/core/src/lsan_suppressions.cpp)
endif()

add_test(NAME nordend-tests COMMAND nordend-tests)
