cmake_minimum_required(VERSION 3.22.1)
project(SiS)
set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_C_COMPILER "/usr/bin/gcc" CACHE STRING "C compiler" FORCE)
#set(CMAKE_CXX_COMPILER "/usr/bin/g++" CACHE STRING "C++ compiler" FORCE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall -Wextra")

# Source fetchers
file(GLOB_RECURSE SOURCES 
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        #"${PROJECT_SOURCE_DIR}/src/*.c"
)

add_executable(SiS ${SOURCES})

# find_package(Threads REQUIRED)
# target_compile_options(SiS PRIVATE
#      -pthread
# )

# target_link_libraries(SiS PRIVATE
#         Threads::Threads
# )

# Build specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
endif()

target_include_directories(SiS PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    #${PROJECT_SOURCE_DIR}/external
)

# Test that shit
# enable_testing()
# add_subdirectory(test)
# add_subdirectory(external)

# commands for when lsp is a bitch
# ln -sf build/Debug/compile_commands.json compile_commands.json
# ln -sf build/Release/compile_commands.json compile_commands.json
