cmake_minimum_required(VERSION 3.16)
project(test_nah CXX)

find_package(nah REQUIRED CONFIG)
add_executable(test_nah test_nah.cpp)
target_compile_features(test_nah PRIVATE cxx_std_17)
target_link_libraries(test_nah PRIVATE NAH::nah)
