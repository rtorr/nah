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
#   make package_nap
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
    # Fallback for examples tree
    if(EXISTS "${CMAKE_SOURCE_DIR}/../../../build/tools/nah/nah")
        set(NAH_CLI "${CMAKE_SOURCE_DIR}/../../../build/tools/nah/nah")
        message(STATUS "Using NAH CLI from examples tree (development mode)")
    else()
        message(FATAL_ERROR "NAH CLI not found. Install 'nah' or set -DNAH_CLI=/path/to/nah")
    endif()
endif()

message(STATUS "NAH CLI: ${NAH_CLI}")

# =============================================================================
# Manifest Generation
# =============================================================================
# The manifest points to the script as entrypoint.
# The script doesn't need to be a compiled binary - the NAK loader
# will receive it via {NAH_APP_ENTRY} and execute it.

set(MANIFEST_JSON "${CMAKE_BINARY_DIR}/manifest.json")
set(MANIFEST_NAH "${CMAKE_BINARY_DIR}/manifest.nah")

file(WRITE ${MANIFEST_JSON} "{
  \"$schema\": \"nah.manifest.input.v2\",
  \"app\": {
    \"id\": \"${APP_ID}\",
    \"version\": \"${APP_VERSION}\",
    \"nak_id\": \"${NAK_ID}\",
    \"nak_version_req\": \"${NAK_VERSION_REQ}\",
    \"nak_loader\": \"default\",
    \"entrypoint\": \"scripts/main.sh\",
    \"asset_dirs\": [\"assets\"],
    \"description\": \"Shell script app with no compiled binary\",
    \"permissions\": {
      \"filesystem\": [\"read:assets/*\", \"read:scripts/*\"]
    }
  }
}")

add_custom_target(generate_manifest ALL
    COMMAND ${NAH_CLI} manifest generate ${MANIFEST_JSON} -o ${MANIFEST_NAH}
    COMMENT "Generating binary manifest for script app"
)

# =============================================================================
# NAP Package Generation
# =============================================================================
# No binary to compile - just package the scripts and assets!

set(NAP_STAGING_DIR "${CMAKE_BINARY_DIR}/nap_staging")

add_custom_target(stage_nap
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/scripts
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAP_STAGING_DIR}/assets

    # Copy scripts (and make executable)
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/main.sh ${NAP_STAGING_DIR}/scripts/
    COMMAND chmod +x ${NAP_STAGING_DIR}/scripts/main.sh

    # Copy assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets ${NAP_STAGING_DIR}/assets

    # Copy manifest
    COMMAND ${CMAKE_COMMAND} -E copy ${MANIFEST_NAH} ${NAP_STAGING_DIR}/manifest.nah

    DEPENDS generate_manifest
    COMMENT "Staging NAP package: ${APP_ID}@${APP_VERSION} (script-only, no binary)"
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

message(STATUS "")
message(STATUS "This app contains NO compiled code!")
message(STATUS "Run 'make package_nap' to create NAP package.")
