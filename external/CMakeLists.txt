include(FetchContent)
# version tag breaks it for some reason hence the commit hashes

# Google Test
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG 52eb810 # v1.17.0  
)
set(INSTALL_GTEST OFF)
set(BUILD_GMOCK ON)
set(BUILD_GTEST ON)
set(gtest_force_shared_crt OFF)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) 
FetchContent_MakeAvailable(googletest)

# linenoise
FetchContent_Declare(
        linenoise
        GIT_REPOSITORY https://github.com/antirez/linenoise.git
        GIT_TAG 97d2850
)
FetchContent_MakeAvailable(linenoise)

add_library(linenoise_lib STATIC ${linenoise_SOURCE_DIR}/linenoise.c)
target_include_directories(linenoise_lib PUBLIC ${linenoise_SOURCE_DIR})
