# NAH Host Integration Examples
# ==============================
# Demonstrates using NAH host library for integration.
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make

cmake_minimum_required(VERSION 3.16)
project(nah_host_examples VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Building NAH Host Examples")

# Fetch nlohmann/json (required by NAH headers)
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# Find NAH headers
set(NAH_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

if(NOT EXISTS "${NAH_INCLUDE_DIR}/nah/nah_host.h")
    message(FATAL_ERROR "NAH host library not found at ${NAH_INCLUDE_DIR}")
endif()

message(STATUS "NAH include: ${NAH_INCLUDE_DIR}")

# =============================================================================
# Host API Demo
# =============================================================================

add_executable(host_api_demo
    src/host_api_demo.cpp
)

target_include_directories(host_api_demo PRIVATE
    ${NAH_INCLUDE_DIR}
)

target_link_libraries(host_api_demo PRIVATE
    nlohmann_json::nlohmann_json
)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_link_libraries(host_api_demo PRIVATE pthread dl)
elseif(WIN32)
    target_compile_definitions(host_api_demo PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# =============================================================================
# Contract Inspector
# =============================================================================

add_executable(contract_inspector
    src/contract_inspector.cpp
)

target_include_directories(contract_inspector PRIVATE
    ${NAH_INCLUDE_DIR}
)

target_link_libraries(contract_inspector PRIVATE
    nlohmann_json::nlohmann_json
)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_link_libraries(contract_inspector PRIVATE pthread dl)
elseif(WIN32)
    target_compile_definitions(contract_inspector PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Install targets
install(TARGETS host_api_demo contract_inspector
    RUNTIME DESTINATION bin
)

message(STATUS "NAH Host examples configured")