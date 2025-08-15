# Source fetcher
file(GLOB_RECURSE TEST_SOURCES 
        "${PROJECT_SOURCE_DIR}/test/units/*.cpp"
)

add_executable(run_tests ${TEST_SOURCES})
target_include_directories(run_tests PRIVATE 
        ${PROJECT_SOURCE_DIR}/include
        ${nloh_json_SOURCE_DIR}/single_include
        ${cxxopts_SOURCE_DIR}/include
)

target_link_libraries(run_tests PRIVATE
        SiSlib
        gtest
        gtest_main
        nlohmann_json::nlohmann_json
        cxxopts
)

include(GoogleTest)
gtest_discover_tests(run_tests
        EXTRA_ARGS --gtest_color=yes
)
