cmake_minimum_required(VERSION 3.16)
project(test_nah CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nah REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)

add_executable(test_nah test_nah.cpp)
target_link_libraries(test_nah PRIVATE nah::core nlohmann_json::nlohmann_json)
