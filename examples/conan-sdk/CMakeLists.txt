cmake_minimum_required(VERSION 3.15)
project(gameengine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# NAK identity
set(NAK_ID "com.example.gameengine")
set(NAK_VERSION "${PROJECT_VERSION}")

message(STATUS "Building GameEngine SDK: ${NAK_ID}@${NAK_VERSION}")

# Find dependencies via Conan
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

# SDK library
add_library(gameengine
    src/engine.cpp
    src/network.cpp
    src/assets.cpp
    src/crypto.cpp
)

target_include_directories(gameengine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gameengine PUBLIC
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    fmt::fmt
)

# Engine loader binary
add_executable(engine-loader
    bin/loader_main.cpp
)

target_link_libraries(engine-loader PRIVATE
    gameengine
)

# =============================================================================
# NAK Pack Generation
# =============================================================================

set(NAK_STAGING_DIR "${CMAKE_BINARY_DIR}/nak_staging")

# Conan full_deploy output location
# When using: conan install . --deployer=full_deploy --deployer-folder=build/deploy
set(CONAN_DEPLOY_DIR "${CMAKE_SOURCE_DIR}/build/deploy/full_deploy" CACHE PATH "Conan full_deploy output directory")

# Generate nak.json from template
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/META/nak.json.in
    ${CMAKE_BINARY_DIR}/META/nak.json
    @ONLY
)

# Custom script to collect Conan dependency libraries
file(WRITE ${CMAKE_BINARY_DIR}/collect_deps.cmake "
# Collect all library files from Conan deploy directory
file(GLOB_RECURSE DEP_LIBS
    \"\${DEPLOY_DIR}/host/*/*.a\"
    \"\${DEPLOY_DIR}/host/*/*.so\"
    \"\${DEPLOY_DIR}/host/*/*.so.*\"
    \"\${DEPLOY_DIR}/host/*/*.dylib\"
)

# Copy each library to staging
foreach(LIB \${DEP_LIBS})
    get_filename_component(LIB_NAME \${LIB} NAME)
    # Skip symlinks by checking if it's a regular file
    if(NOT IS_SYMLINK \${LIB})
        file(COPY \${LIB} DESTINATION \${STAGING_DIR}/lib)
        message(STATUS \"  Copied: \${LIB_NAME}\")
    endif()
endforeach()
")

add_custom_target(stage_nak
    # Create directories
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAK_STAGING_DIR}/META
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAK_STAGING_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAK_STAGING_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${NAK_STAGING_DIR}/resources

    # Copy SDK library
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gameengine> ${NAK_STAGING_DIR}/lib/

    # Copy loader
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:engine-loader> ${NAK_STAGING_DIR}/bin/

    # Copy Conan dependency libraries
    COMMAND ${CMAKE_COMMAND} -E echo "Collecting Conan dependencies..."
    COMMAND ${CMAKE_COMMAND} -DDEPLOY_DIR=${CONAN_DEPLOY_DIR} -DSTAGING_DIR=${NAK_STAGING_DIR} -P ${CMAKE_BINARY_DIR}/collect_deps.cmake

    # Copy resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources ${NAK_STAGING_DIR}/resources

    # Copy metadata
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/META/nak.json ${NAK_STAGING_DIR}/META/

    DEPENDS gameengine engine-loader
    COMMENT "Staging NAK pack: ${NAK_ID}@${NAK_VERSION}"
)

add_custom_target(package_nak
    COMMAND ${CMAKE_COMMAND} -E echo "Creating NAK pack: ${NAK_ID}-${NAK_VERSION}.nak"
    COMMAND ${CMAKE_COMMAND} -E chdir ${NAK_STAGING_DIR}
        ${CMAKE_COMMAND} -E tar czf
        ${CMAKE_BINARY_DIR}/${NAK_ID}-${NAK_VERSION}.nak
        --format=gnutar
        .
    DEPENDS stage_nak
    COMMENT "Packaging NAK: ${CMAKE_BINARY_DIR}/${NAK_ID}-${NAK_VERSION}.nak"
)

message(STATUS "NAK packaging configured. After building, run:")
message(STATUS "  cmake --build . --target package_nak")
