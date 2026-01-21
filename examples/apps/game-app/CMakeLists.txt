# Game App - Uses conan-sdk (Game Engine) NAK
# =============================================
# Example app targeting the Game Engine SDK built with Conan dependencies.
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make
#   make nah_package
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

# Include shared build template
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/NahAppTemplate.cmake)

# Find dependencies (CLI is now optional for pure CMake workflows)
nah_find_cli()
nah_find_sdk(gameengine)

# Create application
nah_create_app(mygame src/main.cpp)

# If we found the SDK for dev builds, link against it and its dependencies
if(NAH_SDK_LIB_DIR AND EXISTS "${NAH_SDK_LIB_DIR}")
    # The SDK is a static library, so we need its transitive dependencies
    # These are bundled in the NAK at runtime, but needed for dev builds
    # SDK_LIB_DIR is conan-sdk/build/build/Release, deploy is at conan-sdk/build/deploy
    get_filename_component(_sdk_build_root "${NAH_SDK_LIB_DIR}/../.." ABSOLUTE)
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
endif()

# Generate manifest and package
nah_app_manifest(mygame
    ID "${APP_ID}"
    VERSION "${APP_VERSION}"
    NAK_ID "${NAK_ID}"
    NAK_VERSION "${NAK_VERSION_REQ}"
    ENTRYPOINT "bin/mygame"
    LIB_DIRS "lib"
    ASSET_DIRS "assets"
)

nah_package(mygame
    ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

message(STATUS "Run 'make nah_package' to create NAP package.")
message(STATUS "Package will be: ${APP_ID}-${APP_VERSION}.nap")
