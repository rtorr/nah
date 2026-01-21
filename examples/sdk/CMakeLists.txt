# Framework SDK - NAK Pack Example
# =================================
# This represents a framework team's SDK deliverable.
# It builds completely independently and produces a NAK pack.
#
# v1.1.0: Updated to use JSON manifest at root (not META/)
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make
#   make package_all
#
# Output: build/com.example.sdk-1.2.3.nak

cmake_minimum_required(VERSION 3.16)
project(framework_sdk VERSION 1.2.3 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)

# SDK identity
set(NAK_ID "com.example.sdk")
set(NAK_VERSION "${PROJECT_VERSION}")

message(STATUS "Building Framework SDK: ${NAK_ID}@${NAK_VERSION}")

# =============================================================================
# Framework Library
# =============================================================================

add_library(framework SHARED
    lib/framework.c
    lib/resources.c
    lib/lifecycle.c
)

target_include_directories(framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(framework PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Create symlink for linking (libframework.dylib -> libframework.1.2.3.dylib)
add_custom_command(TARGET framework POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        $<TARGET_FILE_NAME:framework>
        ${CMAKE_BINARY_DIR}/libframework${CMAKE_SHARED_LIBRARY_SUFFIX}
    COMMENT "Creating libframework symlink"
)

# =============================================================================
# Optional Loader Binary
# =============================================================================

add_executable(framework_loader
    bin/loader.c
)

target_link_libraries(framework_loader PRIVATE framework)

# =============================================================================
# NAK Pack Generation (v1.1.0 format)
# =============================================================================

set(NAK_STAGING_DIR "${CMAKE_BINARY_DIR}/nak_staging")

# Generate nak.json from template (now at root, with nested structure)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/nak.json.in
    ${CMAKE_BINARY_DIR}/nak.json
    @ONLY
)

add_custom_target(stage_nak
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${NAK_STAGING_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAK_STAGING_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAK_STAGING_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAK_STAGING_DIR}/resources

    # Copy library
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:framework> ${NAK_STAGING_DIR}/lib/

    # Copy loader
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:framework_loader> ${NAK_STAGING_DIR}/bin/

    # Copy resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources ${NAK_STAGING_DIR}/resources

    # Copy nak.json to root (NEW: not in META/)
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/nak.json ${NAK_STAGING_DIR}/nak.json

    DEPENDS framework framework_loader
    COMMENT "Staging NAK pack: ${NAK_ID}@${NAK_VERSION}"
)

add_custom_target(package_nak ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Creating NAK pack: ${NAK_ID}-${NAK_VERSION}.nak"
    COMMAND ${CMAKE_COMMAND} -E tar czf
        ${CMAKE_BINARY_DIR}/${NAK_ID}-${NAK_VERSION}.nak
        --format=gnutar
        .
    WORKING_DIRECTORY ${NAK_STAGING_DIR}
    DEPENDS stage_nak
    COMMENT "Packaging NAK: ${CMAKE_BINARY_DIR}/${NAK_ID}-${NAK_VERSION}.nak"
    BYPRODUCTS ${CMAKE_BINARY_DIR}/${NAK_ID}-${NAK_VERSION}.nak
)

# Add to global package_all target
if(NOT TARGET package_all)
    add_custom_target(package_all)
endif()
add_dependencies(package_all package_nak)

message(STATUS "SDK build configured. Run 'make package_all' to create NAK pack.")
message(STATUS "Package will be: ${NAK_ID}-${NAK_VERSION}.nak")
