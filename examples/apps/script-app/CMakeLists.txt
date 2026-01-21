# Script App - App Without Compiled Binary
# =========================================
# This example demonstrates an application that has NO compiled binary.
# The app consists only of a shell script that gets executed by the
# NAK's loader. This pattern is useful for:
#   - Scripting language apps (Python, Ruby, Shell, etc.)
#   - Data-only apps where the NAK provides the runtime
#   - Configuration apps interpreted by framework loaders
#
# The entrypoint points to the script file, which the NAK loader
# receives via {NAH_APP_ENTRY} and executes.
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make nah_package
#
# Output: build/com.example.script-app-1.0.0.nap

cmake_minimum_required(VERSION 3.16)
project(script_app VERSION 1.0.0 LANGUAGES NONE)  # No compiled languages!

# Application identity
set(APP_ID "com.example.script-app")
set(APP_VERSION "${PROJECT_VERSION}")

# NAK requirement - we need the SDK which provides the loader
set(NAK_ID "com.example.sdk")
set(NAK_VERSION_REQ ">=1.0.0 <2.0.0")

message(STATUS "Building Script Application: ${APP_ID}@${APP_VERSION}")
message(STATUS "  Requires NAK: ${NAK_ID} ${NAK_VERSION_REQ}")
message(STATUS "  Note: This app has NO compiled binary - just scripts!")

# Include shared build template (even for script-only apps!)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/NahAppTemplate.cmake)

# Find CLI (optional)
nah_find_cli()

# =============================================================================
# NAP Package Generation (No binary compilation!)
# =============================================================================

set(NAP_STAGING_DIR "${CMAKE_BINARY_DIR}/nap_staging")

# Create a custom target that acts like a compiled binary
# (but actually just stages the script)
add_custom_target(script_app ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/scripts
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/assets
    
    # Copy scripts (and make executable)
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/main.sh ${NAP_STAGING_DIR}/scripts/
    COMMAND chmod +x ${NAP_STAGING_DIR}/scripts/main.sh
    
    # Copy assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets ${NAP_STAGING_DIR}/assets
    
    COMMENT "Staging script app (no compilation)"
)

# Generate manifest - note the entrypoint points to the script!
# We create the manifest file manually since we don't have a binary target
set(MANIFEST_FILE "${CMAKE_BINARY_DIR}/script_app_nap.json")
file(WRITE ${MANIFEST_FILE} "{
  \"$schema\": \"https://nah.rtorr.com/schemas/nap.v1.json\",
  \"app\": {
    \"identity\": {
      \"id\": \"${APP_ID}\",
      \"version\": \"${APP_VERSION}\",
      \"nak_id\": \"${NAK_ID}\",
      \"nak_version_req\": \"${NAK_VERSION_REQ}\"
    },
    \"execution\": {
      \"entrypoint\": \"scripts/main.sh\"
    },
    \"layout\": {
      \"asset_dirs\": [\"assets\"]
    },
    \"metadata\": {
      \"description\": \"Shell script app with no compiled binary\"
    }
  }
}")

# Set variables for nah_package (mimicking what nah_app_manifest does)
set(script_app_MANIFEST_FILE ${MANIFEST_FILE})
set(script_app_APP_ID ${APP_ID})
set(script_app_APP_VERSION ${APP_VERSION})

# Package it up
add_custom_target(script_app_stage
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${NAP_STAGING_DIR}_final
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}_final
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${NAP_STAGING_DIR} ${NAP_STAGING_DIR}_final
    COMMAND ${CMAKE_COMMAND} -E copy ${MANIFEST_FILE} ${NAP_STAGING_DIR}_final/nap.json
    DEPENDS script_app
    COMMENT "Staging NAP: ${APP_ID}@${APP_VERSION}"
)

set(PACKAGE_FILE "${CMAKE_BINARY_DIR}/${APP_ID}-${APP_VERSION}.nap")

add_custom_target(script_app_package ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Creating NAP package: ${PACKAGE_FILE}"
    COMMAND ${CMAKE_COMMAND} -E tar czf ${PACKAGE_FILE} --format=gnutar .
    WORKING_DIRECTORY ${NAP_STAGING_DIR}_final
    DEPENDS script_app_stage
    COMMENT "Packaging NAP: ${APP_ID}-${APP_VERSION}.nap"
    BYPRODUCTS ${PACKAGE_FILE}
)

# Add to global nah_package target
if(NOT TARGET nah_package)
    add_custom_target(nah_package)
endif()
add_dependencies(nah_package script_app_package)

message(STATUS "")
message(STATUS "This app contains NO compiled code!")
message(STATUS "Run 'make nah_package' to create NAP package.")
message(STATUS "Package will be: ${APP_ID}-${APP_VERSION}.nap")
