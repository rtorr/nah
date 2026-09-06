cmake_minimum_required(VERSION 3.21)
project(nah_host_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." "${CMAKE_BINARY_DIR}/nah" EXCLUDE_FROM_ALL)

add_executable(host_api_demo src/host_api_demo.cpp)
target_link_libraries(host_api_demo PRIVATE NAH::nah)

add_executable(contract_inspector src/contract_inspector.cpp)
target_link_libraries(contract_inspector PRIVATE NAH::nah)
