# Host Integration Examples
# =========================
# Demonstrates using NAH C and C++ APIs for host integration.
#
# Build:
#   mkdir build && cd build
#   cmake .. -DNAH_BUILD_DIR=/path/to/nah/build
#   make

cmake_minimum_required(VERSION 3.16)
project(host_examples VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Building Host Integration Examples")

# =============================================================================
# Find NAH
# =============================================================================

if(DEFINED NAH_BUILD_DIR)
    set(NAH_LIB_DIR "${NAH_BUILD_DIR}/src")
    set(NAH_INCLUDE_DIR "${NAH_BUILD_DIR}/../include")
else()
    set(NAH_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../build")
    set(NAH_LIB_DIR "${NAH_BUILD_DIR}/src")
    set(NAH_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
endif()

message(STATUS "NAH include: ${NAH_INCLUDE_DIR}")
message(STATUS "NAH libs: ${NAH_LIB_DIR}")

# =============================================================================
# Host API Demo
# =============================================================================

add_executable(host_api_demo
    src/host_api_demo.cpp
)

target_include_directories(host_api_demo PRIVATE ${NAH_INCLUDE_DIR})
target_link_directories(host_api_demo PRIVATE ${NAH_LIB_DIR})
target_link_libraries(host_api_demo PRIVATE
    nahhost
    nah_contract
    nah_packaging
    nah_manifest
    nah_platform
    nah_config
)

# =============================================================================
# Contract Inspector
# =============================================================================

add_executable(contract_inspector
    src/contract_inspector.cpp
)

target_include_directories(contract_inspector PRIVATE ${NAH_INCLUDE_DIR})
target_link_directories(contract_inspector PRIVATE ${NAH_LIB_DIR})
target_link_libraries(contract_inspector PRIVATE
    nahhost
    nah_contract
    nah_packaging
    nah_manifest
    nah_platform
    nah_config
)

# =============================================================================
# Host C API Demo (Pure C)
# =============================================================================

add_executable(host_c_api_demo
    src/host_c_api_demo.c
)

target_include_directories(host_c_api_demo PRIVATE ${NAH_INCLUDE_DIR})
target_link_directories(host_c_api_demo PRIVATE ${NAH_LIB_DIR})

# Link against static library - requires C++ standard library
target_link_libraries(host_c_api_demo PRIVATE
    nahhost
    nah_contract
    nah_packaging
    nah_materializer
    nah_manifest
    nah_platform
    nah_config
)

# Need to link C++ runtime since nahhost is C++ internally
if(APPLE)
    target_link_libraries(host_c_api_demo PRIVATE "-lc++")
elseif(UNIX)
    target_link_libraries(host_c_api_demo PRIVATE stdc++)
endif()

message(STATUS "Host examples configured.")
