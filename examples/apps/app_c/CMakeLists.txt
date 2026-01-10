# Application C - C++ App with Embedded Manifest
# ===============================================
# This represents a C++ application that embeds the NAH manifest
# directly in the binary at compile time.
#
# This approach:
#   - Manifest JSON is defined in CMake
#   - `nah manifest generate` creates binary manifest at build time
#   - A script converts manifest bytes to a C header file
#   - The header is compiled into the binary in a special section
#   - NAH extracts manifest from binary section at install time
#
# Build (standalone - app teams develop in isolation):
#   mkdir build && cd build
#   cmake .. -DNAH_CLI=/path/to/nah -DNAH_INCLUDE_DIR=/path/to/nah/include
#   make
#   make package_nap
#
# Build (within examples - for demo purposes):
#   mkdir build && cd build
#   cmake ..
#   make
#   make package_nap
#
# Output: build/com.example.app_c-1.0.0.nap

cmake_minimum_required(VERSION 3.16)
project(app_c VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Application identity (used in manifest)
set(APP_ID "com.example.app_c")
set(APP_VERSION "${PROJECT_VERSION}")

# SDK requirement
set(NAK_ID "com.example.sdk")
set(NAK_VERSION_REQ ">=1.0.0 <2.0.0")

message(STATUS "Building Application: ${APP_ID}@${APP_VERSION}")
message(STATUS "  Requires NAK: ${NAK_ID} ${NAK_VERSION_REQ}")
message(STATUS "  Manifest: Embedded at compile time")

# =============================================================================
# Find NAH CLI
# =============================================================================

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
    if(EXISTS "${CMAKE_SOURCE_DIR}/../../../build/tools/nah/nah")
        set(NAH_CLI "${CMAKE_SOURCE_DIR}/../../../build/tools/nah/nah")
        message(STATUS "Using NAH CLI from examples tree (development mode)")
    else()
        message(FATAL_ERROR "NAH CLI not found. Install 'nah' or set -DNAH_CLI=/path/to/nah")
    endif()
endif()

message(STATUS "NAH CLI: ${NAH_CLI}")

# =============================================================================
# Find NAH Headers (for manifest section macros)
# =============================================================================

if(NOT DEFINED NAH_INCLUDE_DIR)
    find_path(NAH_INCLUDE_DIR nah/manifest_builder.hpp
        PATHS
            /usr/local/include
            /usr/include
    )
endif()

if(NOT NAH_INCLUDE_DIR)
    # Fallback for examples tree
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../include/nah")
        set(NAH_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../include")
        message(STATUS "Using NAH headers from examples tree (development mode)")
    else()
        message(FATAL_ERROR "NAH headers not found. Install NAH or set -DNAH_INCLUDE_DIR=/path/to/include")
    endif()
endif()

message(STATUS "NAH headers: ${NAH_INCLUDE_DIR}")

# =============================================================================
# Find Framework SDK (optional - for development builds)
# =============================================================================

if(DEFINED FRAMEWORK_SDK_DIR)
    set(SDK_INCLUDE_DIR "${FRAMEWORK_SDK_DIR}/../include")
    set(SDK_LIB_DIR "${FRAMEWORK_SDK_DIR}")
    message(STATUS "Using SDK from: ${FRAMEWORK_SDK_DIR}")
elseif(DEFINED FRAMEWORK_SDK_INCLUDE_DIR AND DEFINED FRAMEWORK_SDK_LIB_DIR)
    set(SDK_INCLUDE_DIR "${FRAMEWORK_SDK_INCLUDE_DIR}")
    set(SDK_LIB_DIR "${FRAMEWORK_SDK_LIB_DIR}")
else()
    find_path(SDK_INCLUDE_DIR framework/framework.h
        PATHS
            /usr/local/include
            /usr/include
    )
    find_library(SDK_LIBRARY framework
        PATHS
            /usr/local/lib
            /usr/lib
    )

    # Fallback for examples tree
    if(NOT SDK_INCLUDE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include")
        set(SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include")
        set(SDK_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/build")
        message(STATUS "Using SDK from examples tree (development mode)")
    endif()
endif()

# =============================================================================
# Manifest Generation (Build Time)
# =============================================================================
# 1. Write manifest.json
# 2. Run `nah manifest generate` to create binary manifest
# 3. Convert binary to C header with byte array

set(MANIFEST_JSON "${CMAKE_BINARY_DIR}/manifest.json")
set(MANIFEST_NAH "${CMAKE_BINARY_DIR}/manifest.nah")
set(MANIFEST_HEADER "${CMAKE_BINARY_DIR}/generated/manifest_data.h")

# Write manifest JSON
file(WRITE ${MANIFEST_JSON} "{
  \"$schema\": \"nah.manifest.input.v2\",
  \"app\": {
    \"id\": \"${APP_ID}\",
    \"version\": \"${APP_VERSION}\",
    \"nak_id\": \"${NAK_ID}\",
    \"nak_version_req\": \"${NAK_VERSION_REQ}\",
    \"entrypoint\": \"bin/app_c\",
    \"lib_dirs\": [\"lib\"],
    \"asset_dirs\": [\"assets\"],
    \"description\": \"C++ application with embedded NAH manifest\",
    \"author\": \"NAH Examples\",
    \"license\": \"MIT\",
    \"environment\": {
      \"APP_MODE\": \"embedded\"
    }
  }
}")

# Create generated directory
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

# Custom command to generate binary manifest and convert to header
add_custom_command(
    OUTPUT ${MANIFEST_HEADER}
    COMMAND ${NAH_CLI} manifest generate ${MANIFEST_JSON} -o ${MANIFEST_NAH}
    COMMAND ${CMAKE_COMMAND} -DINPUT_FILE=${MANIFEST_NAH} -DOUTPUT_FILE=${MANIFEST_HEADER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/bin2header.cmake
    DEPENDS ${MANIFEST_JSON} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/bin2header.cmake
    COMMENT "Generating embedded manifest header"
    VERBATIM
)

add_custom_target(generate_manifest_header DEPENDS ${MANIFEST_HEADER})

# =============================================================================
# Application Binary (C++ with embedded manifest)
# =============================================================================

add_executable(app_c
    src/main.cpp
)

add_dependencies(app_c generate_manifest_header)

# Include generated header directory
target_include_directories(app_c PRIVATE
    ${NAH_INCLUDE_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

if(SDK_INCLUDE_DIR)
    target_include_directories(app_c PRIVATE ${SDK_INCLUDE_DIR})
endif()

if(SDK_LIB_DIR)
    target_link_directories(app_c PRIVATE ${SDK_LIB_DIR})
    target_link_libraries(app_c PRIVATE framework)
elseif(SDK_LIBRARY)
    target_link_libraries(app_c PRIVATE ${SDK_LIBRARY})
endif()

# =============================================================================
# NAP Package Generation
# =============================================================================
# Note: No manifest.nah file needed - manifest is embedded in binary

set(NAP_STAGING_DIR "${CMAKE_BINARY_DIR}/nap_staging")

add_custom_target(stage_nap
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/assets

    # Copy application binary (contains embedded manifest)
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:app_c> ${NAP_STAGING_DIR}/bin/

    # Copy assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets ${NAP_STAGING_DIR}/assets

    DEPENDS app_c
    COMMENT "Staging NAP package: ${APP_ID}@${APP_VERSION} (embedded manifest)"
)

add_custom_target(package_nap
    COMMAND ${CMAKE_COMMAND} -E echo "Creating NAP package: ${APP_ID}-${APP_VERSION}.nap"
    COMMAND ${CMAKE_COMMAND} -E chdir ${NAP_STAGING_DIR}
        ${CMAKE_COMMAND} -E tar czf
        ${CMAKE_BINARY_DIR}/${APP_ID}-${APP_VERSION}.nap
        --format=gnutar
        .
    DEPENDS stage_nap
    COMMENT "Packaging NAP: ${CMAKE_BINARY_DIR}/${APP_ID}-${APP_VERSION}.nap"
)

message(STATUS "Run 'make package_nap' to create NAP package.")
