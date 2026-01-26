cmake_minimum_required(VERSION 3.16)
project(suite-app VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# Application identity
set(APP_ID "com.example.suite")
set(APP_VERSION "${PROJECT_VERSION}")

message(STATUS "Building Multi-Component Suite: ${APP_ID}@${APP_VERSION}")

# Include shared build template
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/NahAppTemplate.cmake)

# Find dependencies
nah_find_cli()

# Create executables
add_executable(launcher src/launcher.c)
add_executable(editor src/editor.c)
add_executable(viewer src/viewer.c)
add_executable(converter src/converter.c)

# Set output directories
set_target_properties(launcher editor viewer converter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy pre-made manifest to expected location
set(MANIFEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/assets/config.json")
set(MANIFEST_FILE "${CMAKE_BINARY_DIR}/launcher_nap.json")
configure_file(${MANIFEST_SOURCE} ${MANIFEST_FILE} COPYONLY)

# Set variables for nah_package
set(launcher_MANIFEST_FILE ${MANIFEST_FILE})
set(launcher_APP_ID ${APP_ID})
set(launcher_APP_VERSION ${APP_VERSION})

# Generate package (using launcher as the main target)
nah_package(launcher
    ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# Make sure all components are built and copied when packaging
add_dependencies(launcher_stage editor viewer converter)

# Copy all component binaries to staging directory
add_custom_command(TARGET launcher_stage POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy 
        ${CMAKE_BINARY_DIR}/bin/editor
        ${CMAKE_BINARY_DIR}/launcher_staging/bin/editor
    COMMAND ${CMAKE_COMMAND} -E copy 
        ${CMAKE_BINARY_DIR}/bin/viewer
        ${CMAKE_BINARY_DIR}/launcher_staging/bin/viewer
    COMMAND ${CMAKE_COMMAND} -E copy 
        ${CMAKE_BINARY_DIR}/bin/converter
        ${CMAKE_BINARY_DIR}/launcher_staging/bin/converter
    COMMENT "Copying component binaries"
)

message(STATUS "Run 'make nah_package' to create NAP package.")
message(STATUS "Package will be: ${APP_ID}-${APP_VERSION}.nap")

