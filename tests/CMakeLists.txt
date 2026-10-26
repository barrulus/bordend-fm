add_executable(nordend-tests
    main.cpp
    event_loop_test.cpp
    worker_pool_test.cpp)
target_link_libraries(nordend-tests PRIVATE nordend::core doctest::doctest nordend::warnings)
if(NORDEND_SANITIZER)
    target_sources(nordend-tests PRIVATE ${PROJECT_SOURCE_DIR}/src/core/src/lsan_suppressions.cpp)
endif()

add_test(NAME nordend-tests COMMAND nordend-tests)
