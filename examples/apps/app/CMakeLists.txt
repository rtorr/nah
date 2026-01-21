# Example Application - JSON Manifest
# ====================================
# Demonstrates a C application using the Framework SDK with NAH v1.1.0.
# Uses the shared NahAppTemplate.cmake for common build patterns.
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make
#   make package_all
#
# Output: build/com.example.app-1.0.0.nap

cmake_minimum_required(VERSION 3.16)
project(example_app VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# Application identity
set(APP_ID "com.example.app")
set(APP_VERSION "${PROJECT_VERSION}")

# NAK requirement
# Change this to demonstrate different version constraints:
#   ">=1.0.0 <2.0.0" - any 1.x.x version (broad compatibility)
#   ">=1.2.0 <2.0.0" - version 1.2.0 or higher in 1.x series
#   ">=1.2.0 <1.3.0" - version 1.2.x only
#   "1.2.3"  - exact version
set(NAK_ID "com.example.sdk")
set(NAK_VERSION_REQ ">=1.0.0 <2.0.0")

message(STATUS "Building Application: ${APP_ID}@${APP_VERSION}")
message(STATUS "  Requires NAK: ${NAK_ID} ${NAK_VERSION_REQ}")

# Include shared build template
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/NahAppTemplate.cmake)

# Find dependencies (CLI is now optional for pure CMake workflows)
nah_find_cli()
nah_find_sdk(framework)

# Create application
nah_create_app(app src/main.c)

# Generate manifest and package (Option 1: Explicit)
nah_app_manifest(app
    ID "${APP_ID}"
    VERSION "${APP_VERSION}"
    NAK_ID "${NAK_ID}"
    NAK_VERSION "${NAK_VERSION_REQ}"
    ENTRYPOINT "bin/app"
    LIB_DIRS "lib"
    ASSET_DIRS "assets"
)

nah_package(app
    ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# Alternative (Option 2: Shorthand - commented out)
# nah_app(app
#     ID "${APP_ID}"
#     VERSION "${APP_VERSION}"
#     NAK "${NAK_ID}"
#     NAK_VERSION "${NAK_VERSION_REQ}"
#     ENTRYPOINT "bin/app"
#     ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets"
# )

message(STATUS "Run 'make package_all' to create NAP package.")
message(STATUS "Package will be: ${APP_ID}-${APP_VERSION}.nap")
