# NAH App Build Template (Examples Only)
# ======================================
# Convenience CMake patterns for the NAH examples.
# This is NOT part of NAH itself - just shared code to reduce duplication in examples.
#
# For production apps, integrate with your build system using the `nah` CLI directly.
#
# Usage:
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/NahAppTemplate.cmake)
#
#   nah_find_cli()
#   nah_find_sdk(framework)  # or: nah_find_sdk(gameengine)
#   nah_create_app(myapp src/main.c)
#   nah_generate_manifest(myapp "${MANIFEST_CONTENT}")
#   nah_package_nap(myapp)

# Find NAH CLI
# Sets: NAH_CLI
macro(nah_find_cli)
    if(NOT DEFINED NAH_CLI)
        find_program(NAH_CLI nah
            PATHS
                /usr/local/bin
                /usr/bin
                $ENV{HOME}/.local/bin
        )
    endif()

    if(NOT NAH_CLI)
        # Fallback for examples tree (development convenience only)
        set(_nah_examples_cli "${CMAKE_CURRENT_SOURCE_DIR}/../../build/tools/nah/nah")
        set(_nah_root_cli "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/tools/nah/nah")

        if(EXISTS "${_nah_examples_cli}")
            set(NAH_CLI "${_nah_examples_cli}")
            message(STATUS "Using NAH CLI from examples tree (development mode)")
        elseif(EXISTS "${_nah_root_cli}")
            set(NAH_CLI "${_nah_root_cli}")
            message(STATUS "Using NAH CLI from project root (development mode)")
        else()
            message(FATAL_ERROR "NAH CLI not found. Install 'nah' or set -DNAH_CLI=/path/to/nah")
        endif()
    endif()

    message(STATUS "NAH CLI: ${NAH_CLI}")
endmacro()

# Find SDK (framework or gameengine)
# Sets: NAH_SDK_INCLUDE_DIR, NAH_SDK_LIB_DIR, NAH_SDK_LIBRARY
macro(nah_find_sdk SDK_NAME)
    set(_sdk_include_var "${SDK_NAME}_SDK_INCLUDE_DIR")
    set(_sdk_lib_var "${SDK_NAME}_SDK_LIB_DIR")
    set(_sdk_dir_var "${SDK_NAME}_SDK_DIR")

    # Check for explicit SDK_DIR
    if(DEFINED ${_sdk_dir_var})
        set(NAH_SDK_INCLUDE_DIR "${${_sdk_dir_var}}/../include")
        set(NAH_SDK_LIB_DIR "${${_sdk_dir_var}}")
        message(STATUS "Using ${SDK_NAME} SDK from: ${${_sdk_dir_var}}")
        # Check for explicit INCLUDE_DIR and LIB_DIR
    elseif(DEFINED ${_sdk_include_var} AND DEFINED ${_sdk_lib_var})
        set(NAH_SDK_INCLUDE_DIR "${${_sdk_include_var}}")
        set(NAH_SDK_LIB_DIR "${${_sdk_lib_var}}")
        message(STATUS "Using ${SDK_NAME} SDK includes: ${NAH_SDK_INCLUDE_DIR}")
        message(STATUS "Using ${SDK_NAME} SDK libs: ${NAH_SDK_LIB_DIR}")
    else()
        # Try system paths first
        if("${SDK_NAME}" STREQUAL "framework")
            set(_header "framework/framework.h")
            set(_examples_include "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include")
            set(_examples_lib "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/build")
        elseif("${SDK_NAME}" STREQUAL "gameengine")
            set(_header "sdk/engine.hpp")
            set(_examples_include "${CMAKE_CURRENT_SOURCE_DIR}/../../conan-sdk/include")
            set(_examples_lib "${CMAKE_CURRENT_SOURCE_DIR}/../../conan-sdk/build/build/Release")
        else()
            message(FATAL_ERROR "Unknown SDK: ${SDK_NAME}")
        endif()

        find_path(NAH_SDK_INCLUDE_DIR ${_header}
            PATHS
                /usr/local/include
                /usr/include
        )
        find_library(NAH_SDK_LIBRARY ${SDK_NAME}
            PATHS
                /usr/local/lib
                /usr/lib
        )

        # Fallback for examples tree
        if(NOT NAH_SDK_INCLUDE_DIR AND EXISTS "${_examples_include}")
            set(NAH_SDK_INCLUDE_DIR "${_examples_include}")
            set(NAH_SDK_LIB_DIR "${_examples_lib}")
            message(STATUS "Using ${SDK_NAME} SDK from examples tree (development mode)")
        endif()
    endif()

    set(NAH_SDK_NAME "${SDK_NAME}")
endmacro()

# Create app executable with SDK linkage
# Usage: nah_create_app(target_name source1 source2 ...)
function(nah_create_app TARGET_NAME)
    set(SOURCES ${ARGN})

    add_executable(${TARGET_NAME} ${SOURCES})

    if(NAH_SDK_INCLUDE_DIR)
        target_include_directories(${TARGET_NAME} PRIVATE ${NAH_SDK_INCLUDE_DIR})
    endif()

    if(NAH_SDK_LIB_DIR AND EXISTS "${NAH_SDK_LIB_DIR}")
        target_link_directories(${TARGET_NAME} PRIVATE ${NAH_SDK_LIB_DIR})
        target_link_libraries(${TARGET_NAME} PRIVATE ${NAH_SDK_NAME})
    elseif(NAH_SDK_LIBRARY)
        target_link_libraries(${TARGET_NAME} PRIVATE ${NAH_SDK_LIBRARY})
    endif()
endfunction()

# Generate manifest from TOML content
# Usage: nah_generate_manifest(target_name "id = \"...\"\nversion = \"...\"")
function(nah_generate_manifest TARGET_NAME MANIFEST_CONTENT)
    set(MANIFEST_TOML "${CMAKE_BINARY_DIR}/${TARGET_NAME}_manifest.toml")
    set(MANIFEST_NAH "${CMAKE_BINARY_DIR}/${TARGET_NAME}_manifest.nah")

    file(WRITE ${MANIFEST_TOML} "${MANIFEST_CONTENT}")

    add_custom_target(${TARGET_NAME}_generate_manifest
        COMMAND ${NAH_CLI} manifest generate ${MANIFEST_TOML} -o ${MANIFEST_NAH}
        COMMENT "Generating manifest for ${TARGET_NAME}"
    )

    set(${TARGET_NAME}_MANIFEST_NAH ${MANIFEST_NAH} PARENT_SCOPE)
endfunction()

# Package NAP from staged files
# Usage: nah_package_nap(target_name APP_ID APP_VERSION [ASSETS_DIR dir])
function(nah_package_nap TARGET_NAME APP_ID APP_VERSION)
    cmake_parse_arguments(ARG "" "ASSETS_DIR" "" ${ARGN})

    set(NAP_STAGING_DIR "${CMAKE_BINARY_DIR}/${TARGET_NAME}_nap_staging")
    set(MANIFEST_NAH "${CMAKE_BINARY_DIR}/${TARGET_NAME}_manifest.nah")
    set(NAP_FILE "${CMAKE_BINARY_DIR}/${APP_ID}-${APP_VERSION}.nap")

    # Stage NAP
    add_custom_target(${TARGET_NAME}_stage_nap
        COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/lib
        COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/assets
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_NAME}> ${NAP_STAGING_DIR}/bin/
        COMMAND ${CMAKE_COMMAND} -E copy ${MANIFEST_NAH} ${NAP_STAGING_DIR}/manifest.nah
        DEPENDS ${TARGET_NAME} ${TARGET_NAME}_generate_manifest
        COMMENT "Staging NAP: ${APP_ID}@${APP_VERSION}"
    )

    # Copy assets if provided
    if(ARG_ASSETS_DIR AND EXISTS "${ARG_ASSETS_DIR}")
        add_custom_command(TARGET ${TARGET_NAME}_stage_nap POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${ARG_ASSETS_DIR} ${NAP_STAGING_DIR}/assets
            COMMENT "Copying assets"
        )
    endif()

    # Package NAP
    add_custom_target(${TARGET_NAME}_package_nap
        COMMAND ${CMAKE_COMMAND} -E echo "Creating NAP: ${APP_ID}-${APP_VERSION}.nap"
        COMMAND ${CMAKE_COMMAND} -E chdir ${NAP_STAGING_DIR}
            ${CMAKE_COMMAND} -E tar czf ${NAP_FILE} --format=gnutar .
        DEPENDS ${TARGET_NAME}_stage_nap
        COMMENT "Packaging NAP: ${NAP_FILE}"
    )

    # Convenience target
    if(NOT TARGET package_nap)
        add_custom_target(package_nap DEPENDS ${TARGET_NAME}_package_nap)
    else()
        add_dependencies(package_nap ${TARGET_NAME}_package_nap)
    endif()
endfunction()
