add_executable(nordend-tests
    main.cpp
    breadcrumb_test.cpp
    column_view_test.cpp
    config_test.cpp
    details_view_test.cpp
    directory_model_test.cpp
    directory_test.cpp
    event_loop_test.cpp
    file_watcher_test.cpp
    format_test.cpp
    icon_test.cpp
    image_test.cpp
    mime_test.cpp
    palette_test.cpp
    places_test.cpp
    platform_test.cpp
    preview_test.cpp
    quick_look_test.cpp
    scroller_test.cpp
    shortcuts_test.cpp
    sort_test.cpp
    thumbs_test.cpp
    worker_pool_test.cpp)
target_link_libraries(nordend-tests PRIVATE nordend::core nordend::theme nordend::fs nordend::views nordend::image nordend::thumbs nordend::preview nordend::platform doctest::doctest nordend::warnings)
# The shipped example config must parse clean and match the built-in defaults.
target_compile_definitions(nordend-tests
    PRIVATE NORDEND_EXAMPLE_CONFIG="${PROJECT_SOURCE_DIR}/docs/config.example.toml"
            NORDEND_TEST_DATA="${PROJECT_SOURCE_DIR}/tests/data")
if(NORDEND_SANITIZER)
    target_sources(nordend-tests PRIVATE ${PROJECT_SOURCE_DIR}/src/core/src/lsan_suppressions.cpp)
endif()

add_test(NAME nordend-tests COMMAND nordend-tests)
