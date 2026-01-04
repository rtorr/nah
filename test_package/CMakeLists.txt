cmake_minimum_required(VERSION 3.21)
project(test_nah CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nah REQUIRED)

add_executable(test_nah src/test_nah.cpp)
target_link_libraries(test_nah PRIVATE nah::nahhost)
