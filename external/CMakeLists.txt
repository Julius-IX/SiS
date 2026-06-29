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

FetchContent_Declare(
    replxx
    GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx.git
    GIT_TAG master
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(replxx)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG e69e5f977d458f2650bb346dadf2ad30c5320281 # v11.2.0
)

set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_DOC OFF CACHE BOOL "" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(fmt)
