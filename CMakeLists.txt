cmake_minimum_required(VERSION 3.22.1)
project(SiS)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall -Wextra")

# Build specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
else()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
endif()

# Fetch external dependencies
add_subdirectory(external)
find_package(Threads REQUIRED)

# Source fetcher
file(GLOB_RECURSE SOURCES 
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_library(SiSlib STATIC ${SOURCES})
target_include_directories(SiSlib PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(SiSlib PUBLIC
        cxxopts
)

add_executable(SiS src/main.cpp)

target_link_libraries(SiS PRIVATE
    SiSlib
    spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)

# Test that shit
enable_testing()
add_subdirectory(test)

add_custom_target(run_tests
    COMMAND ctest --output-on-failure
    DEPENDS sis_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
