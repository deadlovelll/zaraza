file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

if(TEST_SOURCES)
    add_executable(word_finder_tests ${TEST_SOURCES})

    target_link_libraries(word_finder_tests
        PRIVATE
            faster_profanity_lib
            gtest_main
            miniz
    )

    include(GoogleTest)
    gtest_discover_tests(word_finder_tests)
else()
    message(FATAL_ERROR "No test sources found in ${CMAKE_CURRENT_SOURCE_DIR}")
endif()