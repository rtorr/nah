cmake_minimum_required(VERSION 3.21)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" NAH_VERSION_RAW)
string(STRIP "${NAH_VERSION_RAW}" NAH_VERSION)
# Extract just MAJOR.MINOR.PATCH for CMake (ignore prerelease/build metadata)
string(REGEX REPLACE "^([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" NAH_VERSION_CMAKE "${NAH_VERSION}")

project(nah VERSION ${NAH_VERSION_CMAKE} LANGUAGES CXX)

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

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake)

# Warning flags are applied only to NAH-owned targets.
if(NAH_ENABLE_WARNINGS AND NAH_MAIN_PROJECT)
    if(MSVC)
        set(NAH_WARNING_FLAGS /W4 /permissive- /wd4324)
    else()
        set(NAH_WARNING_FLAGS -Wall -Wextra -Wconversion -Wsign-conversion)
    endif()
endif()

# Sanitizers (opt-in)
if(NAH_ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# Header-only library (the core NAH API)
add_library(nah_core INTERFACE)
add_library(NAH::core ALIAS nah_core)
set_target_properties(nah_core PROPERTIES EXPORT_NAME core)
target_include_directories(nah_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(nah_core INTERFACE cxx_std_17)

add_library(nah_headers INTERFACE)
add_library(NAH::nah ALIAS nah_headers)
set_target_properties(nah_headers PROPERTIES EXPORT_NAME nah)
target_link_libraries(nah_headers INTERFACE
    nah_core
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)

# Archive support is separate because it adds a zlib dependency.
add_library(nah_package INTERFACE)
add_library(NAH::package ALIAS nah_package)
set_target_properties(nah_package PROPERTIES EXPORT_NAME package)
target_link_libraries(nah_package INTERFACE
    nah_core
    $<BUILD_INTERFACE:ZLIB::ZLIB>
    $<INSTALL_INTERFACE:ZLIB::ZLIB>
)

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
    include(CMakePackageConfigHelpers)

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

    install(TARGETS nah_core nah_headers nah_package
        EXPORT NAHTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(EXPORT NAHTargets
        FILE NAHTargets.cmake
        NAMESPACE NAH::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NAH
    )
    configure_package_config_file(
        cmake/NAHConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/NAHConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NAH
    )
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/NAHConfigVersion.cmake
        VERSION ${NAH_VERSION_CMAKE}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/NAHConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/NAHConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NAH
    )
    install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/nah)
endif()
