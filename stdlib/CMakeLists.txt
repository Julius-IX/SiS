cmake_minimum_required(VERSION 3.21)
project(sis_stdlib CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where does the interpreter look for native modules at runtime?
#
# SIS_INCLUDE_DIR:   path to your SisRegistry.h / Value.h etc.
# SIS_OUTPUT_DIR:    where built .so/.dll files land (the platform subdir is appended automatically, see below).
#
# Override from the command line:
#   cmake -DSIS_INCLUDE_DIR=/my/headers -DSIS_OUTPUT_DIR=/my/sis ..

set(SIS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../include"
    CACHE PATH "Path to SiS interpreter headers (SisRegistry.h etc.)")
get_filename_component(
    SIS_INCLUDE_DIR
    "${CMAKE_SOURCE_DIR}/../include"
    ABSOLUTE
)

set(SIS_OUTPUT_BASE "${CMAKE_SOURCE_DIR}/../runtime"
    CACHE PATH "Base path; platform subdir is appended (linux/ or windows/)")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(SIS_PLATFORM_SUBDIR "windows")
  # MinGW produces .dll; MSVC also .dll either way CMake handles the suffix.
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(SIS_PLATFORM_SUBDIR "macos")
else()
  set(SIS_PLATFORM_SUBDIR "linux")
endif()
set(SIS_OUTPUT_DIR "${SIS_OUTPUT_BASE}/${SIS_PLATFORM_SUBDIR}")
message(STATUS "SiS stdlib output dir: ${SIS_OUTPUT_DIR}")

# Helper: sis_add_module(<name>)
#
# Registers a single source file (stdlib/<name>/<name>.cpp) as a shared
# library. Modules with their own CMakeLists.txt never call this directly —
# they do their own thing and just use this as a reference or call it
# themselves via include(). Simple modules get auto-registered below.
#
# Properties set on every module target:
#   - PREFIX ""               so output is "demo.so" not "libdemo.so"
#   - No SOVERSION / VERSION  (these are plugins, not versioned ABI libs)
#   - Output goes to SIS_OUTPUT_DIR
#   - Linked with SIS_INCLUDE_DIR on the include path
function(sis_add_module name)
  set(src "${CMAKE_CURRENT_SOURCE_DIR}/dynamic/${name}/${name}.cpp")
  if(NOT EXISTS "${src}")
    message(WARNING "sis_add_module(${name}): expected source at ${src}, skipping.")
    return()
  endif()
  add_library(${name} SHARED ${src})
  # No "lib" prefix the interpreter loads "demo.so", not "libdemo.so"
  set_target_properties(${name} PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${SIS_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${SIS_OUTPUT_DIR}"  # Windows DLL lands here
  )
  target_include_directories(${name} PRIVATE "${SIS_INCLUDE_DIR}")
  # Modules shouldn't export anything except sis_module_init.
  # Hide everything else to keep the symbol table clean.
  # set_target_properties(${name} PROPERTIES
  #   CXX_VISIBILITY_PRESET hidden
  #   VISIBILITY_INLINES_HIDDEN ON
  # )
endfunction()

# Auto-discovery
#
# Scans immediate subdirectories of stdlib/. For each subdir:
#   - If it has its own CMakeLists.txt  →  add_subdirectory() (module owns
#     its build, can call sis_add_module() or do something custom entirely)
#   - Else if it has <name>/<name>.cpp  →  call sis_add_module(<name>)
#   - Else                              →  skip with a warning
#
# To exclude a directory from auto-discovery (e.g. a shared/ helpers dir),
# create a file called .sis_ignore in it.
file(GLOB module_dirs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
     "${CMAKE_CURRENT_SOURCE_DIR}/dynamic/*/")
foreach(dir IN LISTS module_dirs)
  # Skip non-directories and hidden dirs
  if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dir}")
    continue()
  endif()
  # .sis_ignore opt-out
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/.sis_ignore")
    message(STATUS "  [stdlib] skipping ${dir} (.sis_ignore)")
    continue()
  endif()
  get_filename_component(modname "${dir}" NAME)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/CMakeLists.txt")
    # Module brings its own build logic
    message(STATUS "  [stdlib] ${dir}: using own CMakeLists.txt")
    add_subdirectory("${dir}")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${modname}.cpp")
    # Simple module: one .cpp, auto-register it
    message(STATUS "  [stdlib] ${dir}: auto-registering ${modname}.cpp")
    sis_add_module("${modname}")
  else()
    message(WARNING "  [stdlib] ${dir}: no CMakeLists.txt and no ${modname}.cpp, skipping")
  endif()
endforeach()

# Install target
#
# cmake --install . --prefix /wherever/sis
# puts all .so/.dll files under <prefix>/linux/ or <prefix>/windows/ etc.
#
# The install script / package step can then just zip up the platform dir.
install(DIRECTORY "${SIS_OUTPUT_DIR}/"
        DESTINATION "${SIS_PLATFORM_SUBDIR}"
        FILES_MATCHING
          PATTERN "*.so"
          PATTERN "*.dll"
          PATTERN "*.dylib")
