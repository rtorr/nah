# Game App - Uses conan-sdk (Game Engine) NAK
# =============================================
# Example app targeting the Game Engine SDK built with Conan dependencies.
#
# Build (standalone - app teams develop in isolation):
#   mkdir build && cd build
#   cmake .. -DNAH_CLI=/path/to/nah -DGAMEENGINE_SDK_DIR=/path/to/gameengine/build
#   make
#   make package_nap
#
# Build (within examples - for demo purposes):
#   mkdir build && cd build
#   cmake ..
#   make
#   make package_nap
#
# Output: build/com.example.mygame-1.0.0.nap

cmake_minimum_required(VERSION 3.16)
project(game_app VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Application identity
set(APP_ID "com.example.mygame")
set(APP_VERSION "${PROJECT_VERSION}")

# NAK requirement - targets the Game Engine SDK (conan-sdk)
set(NAK_ID "com.example.gameengine")
set(NAK_VERSION_REQ ">=1.0.0 <2.0.0")

message(STATUS "Building Application: ${APP_ID}@${APP_VERSION}")
message(STATUS "  Requires NAK: ${NAK_ID} ${NAK_VERSION_REQ}")

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
# Find Game Engine SDK (optional - for development builds)
# =============================================================================

if(DEFINED GAMEENGINE_SDK_DIR)
    # GAMEENGINE_SDK_DIR points to build output (e.g., conan-sdk/build/build/Release)
    # Include dir is at the source root (e.g., conan-sdk/include)
    get_filename_component(_sdk_build_parent "${GAMEENGINE_SDK_DIR}" DIRECTORY)
    get_filename_component(_sdk_build_parent "${_sdk_build_parent}" DIRECTORY)
    get_filename_component(_sdk_root "${_sdk_build_parent}" DIRECTORY)
    set(SDK_INCLUDE_DIR "${_sdk_root}/include")
    set(SDK_LIB_DIR "${GAMEENGINE_SDK_DIR}")
    message(STATUS "Using Game Engine SDK from: ${GAMEENGINE_SDK_DIR}")
elseif(DEFINED GAMEENGINE_SDK_INCLUDE_DIR AND DEFINED GAMEENGINE_SDK_LIB_DIR)
    set(SDK_INCLUDE_DIR "${GAMEENGINE_SDK_INCLUDE_DIR}")
    set(SDK_LIB_DIR "${GAMEENGINE_SDK_LIB_DIR}")
else()
    # Try system paths first
    find_path(SDK_INCLUDE_DIR sdk/engine.hpp
        PATHS
            /usr/local/include
            /usr/include
    )
    find_library(SDK_LIBRARY gameengine
        PATHS
            /usr/local/lib
            /usr/lib
    )

    # Fallback for examples tree (development convenience only)
    if(NOT SDK_INCLUDE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../conan-sdk/include")
        set(SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../conan-sdk/include")
        set(SDK_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../conan-sdk/build/build/Release")
        message(STATUS "Using Game Engine SDK from examples tree (development mode)")
    endif()
endif()

# =============================================================================
# Application Binary
# =============================================================================

add_executable(mygame src/main.cpp)

if(SDK_INCLUDE_DIR)
    target_include_directories(mygame PRIVATE ${SDK_INCLUDE_DIR})
endif()

if(SDK_LIB_DIR AND EXISTS "${SDK_LIB_DIR}")
    target_link_directories(mygame PRIVATE ${SDK_LIB_DIR})
    target_link_libraries(mygame PRIVATE gameengine)

    # The SDK is a static library, so we need its transitive dependencies
    # These are bundled in the NAK at runtime, but needed for dev builds
    # SDK_LIB_DIR is conan-sdk/build/build/Release, deploy is at conan-sdk/build/deploy
    get_filename_component(_sdk_build_root "${SDK_LIB_DIR}/../.." ABSOLUTE)
    set(CONAN_DEPLOY_DIR "${_sdk_build_root}/deploy/full_deploy/host")

    if(EXISTS "${CONAN_DEPLOY_DIR}")
        message(STATUS "Finding Conan dependencies in: ${CONAN_DEPLOY_DIR}")

        # Find and link Conan-deployed dependencies using GLOB_RECURSE
        file(GLOB_RECURSE CURL_LIB "${CONAN_DEPLOY_DIR}/libcurl/*/libcurl.a")
        file(GLOB_RECURSE ZLIB_LIB "${CONAN_DEPLOY_DIR}/zlib/*/libz.a")
        file(GLOB_RECURSE SSL_LIB "${CONAN_DEPLOY_DIR}/openssl/*/libssl.a")
        file(GLOB_RECURSE CRYPTO_LIB "${CONAN_DEPLOY_DIR}/openssl/*/libcrypto.a")
        file(GLOB_RECURSE SPDLOG_LIB "${CONAN_DEPLOY_DIR}/spdlog/*/libspdlog.a")
        file(GLOB_RECURSE FMT_LIB "${CONAN_DEPLOY_DIR}/fmt/*/libfmt.a")

        message(STATUS "  CURL: ${CURL_LIB}")
        message(STATUS "  ZLIB: ${ZLIB_LIB}")

        target_link_libraries(mygame PRIVATE
            ${CURL_LIB}
            ${SSL_LIB}
            ${CRYPTO_LIB}
            ${ZLIB_LIB}
            ${SPDLOG_LIB}
            ${FMT_LIB}
        )

        # macOS system frameworks needed by curl/openssl
        if(APPLE)
            find_library(SECURITY_FRAMEWORK Security)
            find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
            find_library(SYSTEMCONFIGURATION_FRAMEWORK SystemConfiguration)
            target_link_libraries(mygame PRIVATE
                ${SECURITY_FRAMEWORK}
                ${COREFOUNDATION_FRAMEWORK}
                ${SYSTEMCONFIGURATION_FRAMEWORK}
            )
        endif()
    else()
        message(WARNING "Conan deploy directory not found: ${CONAN_DEPLOY_DIR}")
    endif()
elseif(SDK_LIBRARY)
    target_link_libraries(mygame PRIVATE ${SDK_LIBRARY})
endif()

# =============================================================================
# Manifest Generation
# =============================================================================

set(MANIFEST_TOML "${CMAKE_BINARY_DIR}/manifest.toml")
set(MANIFEST_NAH "${CMAKE_BINARY_DIR}/manifest.nah")

file(WRITE ${MANIFEST_TOML} "id = \"${APP_ID}\"
version = \"${APP_VERSION}\"
nak_id = \"${NAK_ID}\"
nak_version_req = \"${NAK_VERSION_REQ}\"
entrypoint = \"bin/mygame\"
lib_dirs = [\"lib\"]
asset_dirs = [\"assets\"]

[permissions]
filesystem = [\"read:assets/*\"]
network = [\"connect:*.example.com:443\"]
")

add_custom_target(generate_manifest
    COMMAND ${NAH_CLI} manifest generate ${MANIFEST_TOML} -o ${MANIFEST_NAH}
    COMMENT "Generating binary manifest"
)

# =============================================================================
# NAP Package Generation
# =============================================================================

set(NAP_STAGING_DIR "${CMAKE_BINARY_DIR}/nap_staging")

add_custom_target(stage_nap
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mygame> ${NAP_STAGING_DIR}/bin/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${NAP_STAGING_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E copy ${MANIFEST_NAH} ${NAP_STAGING_DIR}/manifest.nah
    DEPENDS mygame generate_manifest
    COMMENT "Staging NAP package: ${APP_ID}@${APP_VERSION}"
)

add_custom_target(package_nap
    COMMAND ${CMAKE_COMMAND} -E echo "Creating NAP: ${APP_ID}-${APP_VERSION}.nap"
    COMMAND ${CMAKE_COMMAND} -E chdir ${NAP_STAGING_DIR}
        ${CMAKE_COMMAND} -E tar czf ${CMAKE_BINARY_DIR}/${APP_ID}-${APP_VERSION}.nap --format=gnutar .
    DEPENDS stage_nap
    COMMENT "Packaging NAP"
)

message(STATUS "Run 'make package_nap' to create NAP package.")
