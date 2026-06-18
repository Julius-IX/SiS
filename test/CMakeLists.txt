# Source fetcher
file(GLOB_RECURSE TEST_SOURCES 
        "${PROJECT_SOURCE_DIR}/test/units/*.cpp"
)

add_executable(sis_tests ${TEST_SOURCES})
target_include_directories(sis_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${nloh_json_SOURCE_DIR}/single_include
        ${cxxopts_SOURCE_DIR}/include
)

target_link_libraries(sis_tests PRIVATE
        SiSlib
        gtest
        gtest_main
        cxxopts
)

include(GoogleTest)
gtest_discover_tests(sis_tests
        EXTRA_ARGS --gtest_color=yes PRE_TEST
)
