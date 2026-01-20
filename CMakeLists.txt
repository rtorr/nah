cmake_minimum_required(VERSION 3.21)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" NAH_VERSION_RAW)
string(STRIP "${NAH_VERSION_RAW}" NAH_VERSION)
# Extract just MAJOR.MINOR.PATCH for CMake (ignore prerelease/build metadata)
string(REGEX REPLACE "^([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" NAH_VERSION_CMAKE "${NAH_VERSION}")

project(nah VERSION ${NAH_VERSION_CMAKE} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Determine if NAH is the top-level project or included via add_subdirectory/FetchContent
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(NAH_MAIN_PROJECT ON)
else()
    set(NAH_MAIN_PROJECT OFF)
endif()

# Global options
option(NAH_ENABLE_TESTS "Build NAH tests" ${NAH_MAIN_PROJECT})
option(NAH_ENABLE_TOOLS "Build NAH CLI tools" ${NAH_MAIN_PROJECT})
option(NAH_ENABLE_WARNINGS "Enable strict warnings" ${NAH_MAIN_PROJECT})
option(NAH_ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)
option(NAH_INSTALL "Generate install targets" ${NAH_MAIN_PROJECT})

# Always enable tests in CI
if(DEFINED ENV{CI})
    set(NAH_ENABLE_TESTS ON CACHE BOOL "Build NAH tests" FORCE)
    message(STATUS "CI detected - enabling tests")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake)

# Common compile options (only for main project to avoid polluting parent)
if(NAH_ENABLE_WARNINGS AND NAH_MAIN_PROJECT)
    if(MSVC)
        add_compile_options(/W4 /permissive- /wd4324)
    else()
        add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    endif()
endif()

# Sanitizers (opt-in)
if(NAH_ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# Header-only library (the core NAH API)
add_library(nah_core INTERFACE)
target_include_directories(nah_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(nah_core INTERFACE cxx_std_17)
target_link_libraries(nah_core INTERFACE nlohmann_json::nlohmann_json semver)

if(NAH_ENABLE_TOOLS)
    add_subdirectory(tools)
endif()

if(NAH_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
if(NAH_INSTALL)
    include(GNUInstallDirs)

    # Install CLI tool
    if(NAH_ENABLE_TOOLS)
        install(TARGETS nah
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    # Install headers (header-only library - users include directly)
    install(DIRECTORY include/nah
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )
endif()
