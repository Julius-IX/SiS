# stdlib/demo/CMakeLists.txt
#
# The demo module is simple enough that it doesn't NEED this file
# auto-discovery would pick up demo.cpp just fine. This file exists to
# show the pattern for modules that DO need something custom.
#
# Common reasons a module needs its own CMakeLists.txt:
#   - Links against a third-party library (e.g. libcurl, libssl)
#   - Has multiple source files
#   - Needs find_package() or FetchContent
#   - Needs platform-specific compile flags
#
# The parent already called:
#   add_subdirectory(demo)          ← which runs this file
#   and set: SIS_INCLUDE_DIR, SIS_OUTPUT_DIR, SIS_PLATFORM_SUBDIR
#   and defined: the sis_add_module() function
#
# So you have all of those available here.

# Simple case: just delegate to the helper.
# The function handles PREFIX, output dir, visibility flags, etc.
sis_add_module("demo")

# Example: if demo needed libcurl you'd add after sis_add_module:
#
# find_package(CURL REQUIRED)
# target_link_libraries(demo PRIVATE CURL::libcurl)

# Example: multiple source files
#
# add_library(demo SHARED demo.cpp demo_helpers.cpp)
# set_target_properties(demo PROPERTIES
#   PREFIX ""
#   LIBRARY_OUTPUT_DIRECTORY "${SIS_OUTPUT_DIR}"
#   RUNTIME_OUTPUT_DIRECTORY "${SIS_OUTPUT_DIR}"
#   CXX_VISIBILITY_PRESET hidden
#   VISIBILITY_INLINES_HIDDEN ON)
# target_include_directories(demo PRIVATE "${SIS_INCLUDE_DIR}")
