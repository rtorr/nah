# Example Application - Standalone Manifest
# ==========================================
# Demonstrates a C application using the Framework SDK with NAH.
# Uses the shared NahAppTemplate.cmake for common build patterns.
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make
#   make package_nap
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

# Find dependencies
nah_find_cli()
nah_find_sdk(framework)

# Create application
nah_create_app(app src/main.c)

# Generate manifest
set(MANIFEST_CONTENT "{
  \"$schema\": \"nah.manifest.input.v2\",
  \"app\": {
    \"id\": \"${APP_ID}\",
    \"version\": \"${APP_VERSION}\",
    \"nak_id\": \"${NAK_ID}\",
    \"nak_version_req\": \"${NAK_VERSION_REQ}\",
    \"entrypoint\": \"bin/app\",
    \"lib_dirs\": [\"lib\"],
    \"asset_dirs\": [\"assets\"],
    \"permissions\": {
      \"filesystem\": [\"read:assets/*\"]
    }
  }
}")

nah_generate_manifest(app "${MANIFEST_CONTENT}")

# Package NAP
nah_package_nap(app ${APP_ID} ${APP_VERSION}
    ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets
)

message(STATUS "Run 'make package_nap' to create NAP package.")
