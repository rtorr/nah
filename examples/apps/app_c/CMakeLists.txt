# Application C - C++ App Example
# =================================
# This represents a C++ application using the Framework SDK with NAH v1.1.0.
# Uses the shared NahAppTemplate.cmake for common build patterns.
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make
#   make package_all
#
# Output: build/com.example.app_c-1.0.0.nap

cmake_minimum_required(VERSION 3.16)
project(app_c VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Application identity
set(APP_ID "com.example.app_c")
set(APP_VERSION "${PROJECT_VERSION}")

# NAK requirement
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
nah_create_app(app_c src/main.cpp)

# Generate manifest and package using the shorthand syntax
nah_app(app_c
    ID "${APP_ID}"
    VERSION "${APP_VERSION}"
    NAK "${NAK_ID}"
    NAK_VERSION "${NAK_VERSION_REQ}"
    ENTRYPOINT "bin/app_c"
    ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    ENV_VARS "APP_MODE=packaged"
)

message(STATUS "Run 'make package_all' to create NAP package.")
message(STATUS "Package will be: ${APP_ID}-${APP_VERSION}.nap")
