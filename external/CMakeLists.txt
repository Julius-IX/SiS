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

# nlohmann json
FetchContent_Declare(
        nloh_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG 55f9368 # v3.12.0
)
FetchContent_MakeAvailable(nloh_json)
